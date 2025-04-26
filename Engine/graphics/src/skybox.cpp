#include "skybox.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "extra.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "renderer.hpp"
#include "tiny_gltf.h"

namespace Humongous
{

Skybox::Skybox(const SkyboxCreateInfo& createInfo) : m_logicalDevice{createInfo.logicalDevice}
{
    LoadCube();
    LoadCubemap(createInfo.cubemapPath);
    GeneratePBRImages(createInfo.uniformPool);
    LoadDescriptorSet(createInfo.descriptorSetLayout, &createInfo.imagePool);
}

Skybox::~Skybox()
{
    m_skybox->Destroy();
    m_irradiance->Destroy();

    for(auto& view: m_prefilteredMipViews) { vkDestroyImageView(m_logicalDevice->GetVkDevice(), view, nullptr); }

    m_prefilteredMap->Destroy();
    m_brdflut->Destroy();
}

void Skybox::LoadCube()
{
    // These are hardcoded, as we will always create a cube

    // source: https://pastebin.com/4T10MFgb
    std::vector<n32> indices = {0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
                                12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21};

    std::vector<Model::Vertex> vertices = {
        // left face (white)
        {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
        {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
        {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},

        // right face (yellow)
        {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
        {{.5f, .5f, .5f}, {.8f, .8f, .1f}},
        {{.5f, -.5f, .5f}, {.8f, .8f, .1f}},
        {{.5f, .5f, -.5f}, {.8f, .8f, .1f}},

        // top face (orange, remember y axis points down)
        {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
        {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},
        {{-.5f, -.5f, .5f}, {.9f, .6f, .1f}},
        {{.5f, -.5f, -.5f}, {.9f, .6f, .1f}},

        // bottom face (red)
        {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
        {{.5f, .5f, .5f}, {.8f, .1f, .1f}},
        {{-.5f, .5f, .5f}, {.8f, .1f, .1f}},
        {{.5f, .5f, -.5f}, {.8f, .1f, .1f}},

        // nose face (blue)
        {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
        {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
        {{-.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
        {{.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},

        // tail face (green)
        {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
        {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
        {{-.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
        {{.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
    };

    m_vertexCount = static_cast<n32>(vertices.size());
    m_indexCount = static_cast<n32>(indices.size());

    // Vertex buffer
    {
        VkDeviceSize bufferSize = sizeof(Model::Vertex) * m_vertexCount;

        Buffer stagingBuffer{m_logicalDevice,
                             bufferSize,
                             1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)vertices.data());

        m_vertexBuffer =
            std::make_unique<Buffer>(m_logicalDevice, bufferSize, 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_vertexBuffer, bufferSize);
    }

    // Index Buffer
    {
        VkDeviceSize bufferSize = sizeof(n32) * m_indexCount;

        Buffer stagingBuffer{m_logicalDevice,
                             bufferSize,
                             1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)indices.data());

        m_indexBuffer =
            std::make_unique<Buffer>(m_logicalDevice, bufferSize, 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_CPU_COPY);

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_indexBuffer, bufferSize);
    }

    // Indirect Draw buffer
    {
        VkDeviceSize bufferSize = sizeof(VkDrawIndexedIndirectCommand);

        Buffer stagingBuffer{m_logicalDevice,
                             bufferSize,
                             1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             4};
        m_command.firstIndex = 0;
        m_command.indexCount = m_indexCount;
        m_command.vertexOffset = 0;
        m_command.instanceCount = 1;
        m_command.firstInstance = 0;
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)&m_command);

        m_indirectDrawBuffer =
            std::make_unique<Buffer>(m_logicalDevice, bufferSize, 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_CPU_COPY, 4);

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_indirectDrawBuffer, bufferSize);
    }
}

void Skybox::LoadCubemap(const std::string& cubemapPath)
{
    m_skybox = std::make_unique<Texture>(m_logicalDevice, cubemapPath, Texture::ImageType::CUBEMAP);
}

void Skybox::LoadDescriptorSet(DescriptorSetLayout& descriptorLayout, DescriptorPoolGrowable* pool)
{
    auto                  imgInfo = m_skybox->GetDescriptorInfo();
    auto                  irradInfo = m_irradiance->GetDescriptorInfo();
    VkDescriptorImageInfo info{m_prefilteredMap->GetRawSamplerHandle(), m_prefilteredMipViews[1], m_prefilteredMap->GetRawImageLayout()};
    auto                  brdfInfo = m_brdflut->GetDescriptorInfo();
    DescriptorWriter(descriptorLayout, pool)
        .WriteImage(0, &imgInfo)
        .WriteImage(1, &irradInfo)
        .WriteImage(2, &info)
        .WriteImage(3, &brdfInfo)
        .Build(m_cubeMapSet);
}

struct PrefilteredData
{
    float roughness;
    n32   mipLevel;
};

void Skybox::CreatePrefilteredMipViews()
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_prefilteredMap->GetRawImageHandle(); // The main VkImage resource
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;            // It's a cubemap
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;        // Match your image format
    viewInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseArrayLayer = 0; // Start from the first face
    viewInfo.subresourceRange.layerCount = 6;     // View all 6 faces

    m_prefilteredMipViews.resize(m_prefilteredMap->GetMipLevels());
    for(uint32_t mipLevel = 0; mipLevel < m_prefilteredMap->GetMipLevels(); ++mipLevel)
    {
        // Set the subresource range to target ONLY this mip level
        viewInfo.subresourceRange.baseMipLevel = mipLevel;
        viewInfo.subresourceRange.levelCount = 1; // View only this single mip level

        if(vkCreateImageView(m_logicalDevice->GetVkDevice(), &viewInfo, nullptr, &m_prefilteredMipViews[mipLevel]) != VK_SUCCESS)
        {
            HGERROR("Failed to create image view for prefiltered map mip level %u", mipLevel);
        }
    }
}

void Skybox::GeneratePBRImages(DescriptorPoolGrowable& pool)
{
    // Prep work
    m_irradiance =
        std::make_unique<Texture>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::TEXTURE, "papermill"),
                                  Texture::ImageType::CUBEMAP, true);

    m_prefilteredMap =
        std::make_unique<Texture>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::TEXTURE, "papermill"),
                                  Texture::ImageType::CUBEMAP, true);

    CreatePrefilteredMipViews();

    m_brdflut = std::make_unique<Texture>();
    m_brdflut->FillWithEmpty(m_logicalDevice, 512, 512, true);

    VkShaderModule irradianceMod;
    {
        auto code = Utils::ReadFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "irradiance.comp"));

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const n32*>(code.data());

        if(vkCreateShaderModule(m_logicalDevice->GetVkDevice(), &createInfo, nullptr, &irradianceMod) != VK_SUCCESS)
        {
            HGERROR("Failed to create shader module!");
        }
    }

    VkShaderModule prefiltredMod;
    {
        auto code = Utils::ReadFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "prefiltredmap.comp"));

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const n32*>(code.data());

        if(vkCreateShaderModule(m_logicalDevice->GetVkDevice(), &createInfo, nullptr, &prefiltredMod) != VK_SUCCESS)
        {
            HGERROR("Failed to create shader module!");
        }
    }

    VkShaderModule brdfMod;
    {
        auto code = Utils::ReadFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "brdflut.comp"));

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const n32*>(code.data());

        if(vkCreateShaderModule(m_logicalDevice->GetVkDevice(), &createInfo, nullptr, &brdfMod) != VK_SUCCESS)
        {
            HGERROR("Failed to create shader module!");
        }
    }

    DescriptorSetLayout::Builder envImageBuilder{*m_logicalDevice};
    envImageBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
    auto envLayout = envImageBuilder.Build();

    DescriptorSetLayout::Builder IrradImageBuilder{*m_logicalDevice};
    IrradImageBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    auto irradLayout = IrradImageBuilder.Build();

    DescriptorSetLayout::Builder PrefiltImageBuilder{*m_logicalDevice};
    PrefiltImageBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    auto prefilteredLayout = PrefiltImageBuilder.Build();

    DescriptorSetLayout::Builder brdfImageBuilder{*m_logicalDevice};
    brdfImageBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    auto brdfLayout = brdfImageBuilder.Build();

    std::array<VkDescriptorSetLayout, 2> irradDescriptorLayouts = {envLayout->GetDescriptorSetLayout(), irradLayout->GetDescriptorSetLayout()};

    VkPipelineLayoutCreateInfo irradPipelineLayoutInfo{};
    irradPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    irradPipelineLayoutInfo.setLayoutCount = irradDescriptorLayouts.size();
    irradPipelineLayoutInfo.pSetLayouts = irradDescriptorLayouts.data();
    irradPipelineLayoutInfo.pushConstantRangeCount = 0;
    irradPipelineLayoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout irradePipelineLayout;
    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &irradPipelineLayoutInfo, nullptr, &irradePipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    std::array<VkDescriptorSetLayout, 2> prefiltDescriptorLayouts = {envLayout->GetDescriptorSetLayout(),
                                                                     prefilteredLayout->GetDescriptorSetLayout()};

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(PrefilteredData);
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo prefiltPipelineLayoutInfo{};
    prefiltPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    prefiltPipelineLayoutInfo.setLayoutCount = prefiltDescriptorLayouts.size();
    prefiltPipelineLayoutInfo.pSetLayouts = prefiltDescriptorLayouts.data();
    prefiltPipelineLayoutInfo.pushConstantRangeCount = 1;
    prefiltPipelineLayoutInfo.pPushConstantRanges = &range;

    VkPipelineLayout prefiltPipelineLayout;
    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &prefiltPipelineLayoutInfo, nullptr, &prefiltPipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    std::array<VkDescriptorSetLayout, 1> brdfDescriptorLayouts = {brdfLayout->GetDescriptorSetLayout()};

    VkPipelineLayoutCreateInfo brdfPipelineLayoutInfo{};
    brdfPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    brdfPipelineLayoutInfo.setLayoutCount = brdfDescriptorLayouts.size();
    brdfPipelineLayoutInfo.pSetLayouts = brdfDescriptorLayouts.data();
    brdfPipelineLayoutInfo.pushConstantRangeCount = 0;
    brdfPipelineLayoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout brdfPipelineLayout;
    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &brdfPipelineLayoutInfo, nullptr, &brdfPipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    VkDescriptorSet envSet;
    VkDescriptorSet irradSet;
    VkDescriptorSet brdfSet;

    auto imgInfo = m_skybox->GetDescriptorInfo();
    auto irradianceInfo = m_irradiance->GetDescriptorInfo();
    auto brdflutInfo = m_brdflut->GetDescriptorInfo();

    DescriptorWriter(*envLayout, &pool).WriteImage(0, &imgInfo).Build(envSet);
    DescriptorWriter(*irradLayout, &pool).WriteImage(0, &irradianceInfo).Build(irradSet);
    DescriptorWriter(*brdfLayout, &pool).WriteImage(0, &brdflutInfo).Build(brdfSet);

    const n32 LOCAL_SIZE_X = 16;
    const n32 LOCAL_SIZE_Y = 16;
    const n32 LOCAL_SIZE_Z = 1;

    // Irradiance map
    {
        VkPipelineShaderStageCreateInfo irradianceStage{};
        irradianceStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        irradianceStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        irradianceStage.pName = "main";
        irradianceStage.module = irradianceMod;

        VkComputePipelineCreateInfo compInfo{};
        compInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compInfo.layout = irradePipelineLayout;
        compInfo.stage = irradianceStage;

        VkPipeline irradiancePipeline;
        if(vkCreateComputePipelines(m_logicalDevice->GetVkDevice(), nullptr, 1, &compInfo, nullptr, &irradiancePipeline) != VK_SUCCESS)
        {
            HGFATAL("Failed to create irradiance compute pipeline!");
        }
        auto cmd = m_logicalDevice->BeginSingleTimeCommands();

        std::array<VkDescriptorSet, 2> irradianceSets = {envSet, irradSet};

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, irradiancePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, irradePipelineLayout, 0, irradianceSets.size(), irradianceSets.data(), 0,
                                nullptr);

        n32 irradianceMapSize = 512;
        n32 irradianceGroupCountX = (irradianceMapSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
        n32 irradianceGroupCountY = (irradianceMapSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
        n32 irradianceGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z; // Since LOCAL_SIZE_Z is 1, this is 6

        vkCmdDispatch(cmd, irradianceGroupCountX, irradianceGroupCountY, irradianceGroupCountZ);

        m_logicalDevice->EndSingleTimeCommands(cmd);
        vkDestroyPipeline(m_logicalDevice->GetVkDevice(), irradiancePipeline, nullptr);
    }
    // Prefiltered map
    {
        VkPipelineShaderStageCreateInfo prefilteredStage{};
        prefilteredStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        prefilteredStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        prefilteredStage.pName = "main";
        prefilteredStage.module = prefiltredMod;

        VkComputePipelineCreateInfo compInfo{};
        compInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compInfo.layout = prefiltPipelineLayout;
        compInfo.stage = prefilteredStage;

        VkPipeline prefilteredPipeline;
        if(vkCreateComputePipelines(m_logicalDevice->GetVkDevice(), nullptr, 1, &compInfo, nullptr, &prefilteredPipeline) != VK_SUCCESS)
        {
            HGFATAL("Failed to create prefiltered compute pipeline!");
        }

        std::array<VkDescriptorSet, 2> prefilteredSets = {envSet};

        auto cmd = m_logicalDevice->BeginSingleTimeCommands();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prefilteredPipeline);

        for(n32 mipLevel = 0; mipLevel < m_prefilteredMap->GetMipLevels(); mipLevel++)
        {
            n32 mipSize = m_prefilteredMap->GetBaseSize() >> mipLevel;
            mipSize = std::max(1u, mipSize);

            n32 prefilteredGroupCountX = (mipSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
            n32 prefilteredGroupCountY = (mipSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
            n32 prefilteredGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z;

            VkDescriptorImageInfo info;
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            info.imageView = m_prefilteredMipViews[mipLevel];

            VkDescriptorSet s;
            DescriptorWriter(*prefilteredLayout, &pool).WriteImage(0, &info).Build(s);

            prefilteredSets[1] = s;
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prefiltPipelineLayout, 0, 2, prefilteredSets.data(), 0, nullptr);

            PrefilteredData prefilterData;
            prefilterData.roughness = pow((float)mipLevel / (float)(m_prefilteredMap->GetMipLevels() - 1), 0.6);
            prefilterData.mipLevel = mipLevel;

            vkCmdPushConstants(cmd, prefiltPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PrefilteredData), &prefilterData);

            vkCmdDispatch(cmd, prefilteredGroupCountX, prefilteredGroupCountY, prefilteredGroupCountZ);
        }

        m_logicalDevice->EndSingleTimeCommands(cmd);

        vkDestroyPipeline(m_logicalDevice->GetVkDevice(), prefilteredPipeline, nullptr);
    }
    // BRDF LUT
    {
        VkPipelineShaderStageCreateInfo brdfStage{};
        brdfStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        brdfStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        brdfStage.pName = "main";
        brdfStage.module = brdfMod;

        VkComputePipelineCreateInfo compInfo{};
        compInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compInfo.layout = brdfPipelineLayout;
        compInfo.stage = brdfStage;

        VkPipeline brdfPipeline;
        if(vkCreateComputePipelines(m_logicalDevice->GetVkDevice(), nullptr, 1, &compInfo, nullptr, &brdfPipeline) != VK_SUCCESS)
        {
            HGFATAL("Failed to create brdf compute pipeline!");
        }
        auto cmd = m_logicalDevice->BeginSingleTimeCommands();

        std::array<VkDescriptorSet, 1> brdfSets = {brdfSet};

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, brdfPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, brdfPipelineLayout, 0, brdfSets.size(), brdfSets.data(), 0, nullptr);

        n32 brdfMapSize = 512;
        n32 brdfGroupCountX = (brdfMapSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
        n32 brdfGroupCountY = (brdfMapSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
        n32 brdfGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z; // Since LOCAL_SIZE_Z is 1, this is 6

        vkCmdDispatch(cmd, brdfGroupCountX, brdfGroupCountY, brdfGroupCountZ);

        m_logicalDevice->EndSingleTimeCommands(cmd);

        vkDestroyPipeline(m_logicalDevice->GetVkDevice(), brdfPipeline, nullptr);
    }

    // auto cmd = m_logicalDevice->BeginSingleTimeCommands();
    // Renderer::WaitForCompute(cmd);
    // m_logicalDevice->EndSingleTimeCommands(cmd);

    // Afterwork
    vkDestroyShaderModule(m_logicalDevice->GetVkDevice(), irradianceMod, nullptr);
    vkDestroyShaderModule(m_logicalDevice->GetVkDevice(), prefiltredMod, nullptr);
    vkDestroyShaderModule(m_logicalDevice->GetVkDevice(), brdfMod, nullptr);

    vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), irradePipelineLayout, nullptr);
    vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), prefiltPipelineLayout, nullptr);
    vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), brdfPipelineLayout, nullptr);
}

} // namespace Humongous
