#include "skybox.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "extra.hpp"
#include "logger.hpp"
#include "model.hpp"
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
        vk::DeviceSize bufferSize = sizeof(Model::Vertex) * m_vertexCount;

        Buffer stagingBuffer{m_logicalDevice,
                             bufferSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)vertices.data());

        m_vertexBuffer = std::make_unique<Buffer>(m_logicalDevice, bufferSize, 1,
                                                  vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                  vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY);

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_vertexBuffer, bufferSize);
    }

    // Index Buffer
    {
        vk::DeviceSize bufferSize = sizeof(n32) * m_indexCount;

        Buffer stagingBuffer{m_logicalDevice,
                             bufferSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)indices.data());

        m_indexBuffer =
            std::make_unique<Buffer>(m_logicalDevice, bufferSize, 1, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                                     vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_CPU_COPY);

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_indexBuffer, bufferSize);
    }

    // Indirect Draw buffer
    {
        vk::DeviceSize bufferSize = sizeof(vk::DrawIndexedIndirectCommand);

        Buffer stagingBuffer{m_logicalDevice,
                             bufferSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             4};
        m_command.firstIndex = 0;
        m_command.indexCount = m_indexCount;
        m_command.vertexOffset = 0;
        m_command.instanceCount = 1;
        m_command.firstInstance = 0;
        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)&m_command);

        m_indirectDrawBuffer = std::make_unique<Buffer>(m_logicalDevice, bufferSize, 1,
                                                        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndirectBuffer,
                                                        vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_CPU_COPY, 4);

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_indirectDrawBuffer, bufferSize);
    }
}

void Skybox::LoadCubemap(const std::string& cubemapPath)
{
    m_skybox = std::make_unique<Texture>(m_logicalDevice, cubemapPath, Texture::ImageType::CUBEMAP);
}

void Skybox::LoadDescriptorSet(DescriptorSetLayout& descriptorLayout, DescriptorPoolGrowable* pool)
{
    auto                    imgInfo = m_skybox->GetDescriptorInfo();
    auto                    irradInfo = m_irradiance->GetDescriptorInfo();
    vk::DescriptorImageInfo info{m_prefilteredMap->GetRawSamplerHandle(), m_prefilteredMipViews[1], m_prefilteredMap->GetRawImageLayout()};
    auto                    brdfInfo = m_brdflut->GetDescriptorInfo();
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
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = m_prefilteredMap->GetRawImageHandle(); // The main vk::Image resource
    viewInfo.viewType = vk::ImageViewType::eCube;           // It's a cubemap
    viewInfo.format = vk::Format::eR16G16B16A16Sfloat;      // Match your image format
    viewInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseArrayLayer = 0; // Start from the first face
    viewInfo.subresourceRange.layerCount = 6;     // View all 6 faces

    m_prefilteredMipViews.resize(m_prefilteredMap->GetMipLevels());
    for(uint32_t mipLevel = 0; mipLevel < m_prefilteredMap->GetMipLevels(); ++mipLevel)
    {
        // Set the subresource range to target ONLY this mip level
        viewInfo.subresourceRange.baseMipLevel = mipLevel;
        viewInfo.subresourceRange.levelCount = 1; // View only this single mip level

        if(m_logicalDevice->GetVkDevice().createImageView(&viewInfo, nullptr, &m_prefilteredMipViews[mipLevel]) != vk::Result::eSuccess)
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

    vk::ShaderModule irradianceMod;
    {
        auto code = Utils::ReadFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "irradiance.comp"));

        vk::ShaderModuleCreateInfo createInfo{};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const n32*>(code.data());

        if(m_logicalDevice->GetVkDevice().createShaderModule(&createInfo, nullptr, &irradianceMod) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create shader module!");
        }
    }

    vk::ShaderModule prefiltredMod;
    {
        auto code = Utils::ReadFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "prefiltredmap.comp"));

        vk::ShaderModuleCreateInfo createInfo{};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const n32*>(code.data());

        if(m_logicalDevice->GetVkDevice().createShaderModule(&createInfo, nullptr, &prefiltredMod) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create shader module!");
        }
    }

    vk::ShaderModule brdfMod;
    {
        auto code = Utils::ReadFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "brdflut.comp"));

        vk::ShaderModuleCreateInfo createInfo{};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const n32*>(code.data());

        if(m_logicalDevice->GetVkDevice().createShaderModule(&createInfo, nullptr, &brdfMod) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create shader module!");
        }
    }

    DescriptorSetLayout::Builder envImageBuilder{*m_logicalDevice};
    envImageBuilder.addBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    auto envLayout = envImageBuilder.Build();

    DescriptorSetLayout::Builder IrradImageBuilder{*m_logicalDevice};
    IrradImageBuilder.addBinding(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute);
    auto irradLayout = IrradImageBuilder.Build();

    DescriptorSetLayout::Builder PrefiltImageBuilder{*m_logicalDevice};
    PrefiltImageBuilder.addBinding(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute);
    auto prefilteredLayout = PrefiltImageBuilder.Build();

    DescriptorSetLayout::Builder brdfImageBuilder{*m_logicalDevice};
    brdfImageBuilder.addBinding(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute);
    auto brdfLayout = brdfImageBuilder.Build();

    std::array<vk::DescriptorSetLayout, 2> irradDescriptorLayouts = {envLayout->GetDescriptorSetLayout(), irradLayout->GetDescriptorSetLayout()};

    vk::PipelineLayoutCreateInfo irradPipelineLayoutInfo{};
    irradPipelineLayoutInfo.setLayoutCount = irradDescriptorLayouts.size();
    irradPipelineLayoutInfo.pSetLayouts = irradDescriptorLayouts.data();
    irradPipelineLayoutInfo.pushConstantRangeCount = 0;
    irradPipelineLayoutInfo.pPushConstantRanges = nullptr;

    vk::PipelineLayout irradePipelineLayout;
    if(m_logicalDevice->GetVkDevice().createPipelineLayout(&irradPipelineLayoutInfo, nullptr, &irradePipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout");
    }

    std::array<vk::DescriptorSetLayout, 2> prefiltDescriptorLayouts = {envLayout->GetDescriptorSetLayout(),
                                                                       prefilteredLayout->GetDescriptorSetLayout()};

    vk::PushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(PrefilteredData);
    range.stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::PipelineLayoutCreateInfo prefiltPipelineLayoutInfo{};
    prefiltPipelineLayoutInfo.setLayoutCount = prefiltDescriptorLayouts.size();
    prefiltPipelineLayoutInfo.pSetLayouts = prefiltDescriptorLayouts.data();
    prefiltPipelineLayoutInfo.pushConstantRangeCount = 1;
    prefiltPipelineLayoutInfo.pPushConstantRanges = &range;

    vk::PipelineLayout prefiltPipelineLayout;
    if(m_logicalDevice->GetVkDevice().createPipelineLayout(&prefiltPipelineLayoutInfo, nullptr, &prefiltPipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout");
    }

    std::array<vk::DescriptorSetLayout, 1> brdfDescriptorLayouts = {brdfLayout->GetDescriptorSetLayout()};

    vk::PipelineLayoutCreateInfo brdfPipelineLayoutInfo{};
    brdfPipelineLayoutInfo.setLayoutCount = brdfDescriptorLayouts.size();
    brdfPipelineLayoutInfo.pSetLayouts = brdfDescriptorLayouts.data();
    brdfPipelineLayoutInfo.pushConstantRangeCount = 0;
    brdfPipelineLayoutInfo.pPushConstantRanges = nullptr;

    vk::PipelineLayout brdfPipelineLayout;
    if(m_logicalDevice->GetVkDevice().createPipelineLayout(&brdfPipelineLayoutInfo, nullptr, &brdfPipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout");
    }

    vk::DescriptorSet envSet;
    vk::DescriptorSet irradSet;
    vk::DescriptorSet brdfSet;

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
        vk::PipelineShaderStageCreateInfo irradianceStage{};
        irradianceStage.stage = vk::ShaderStageFlagBits::eCompute;
        irradianceStage.pName = "main";
        irradianceStage.module = irradianceMod;

        vk::ComputePipelineCreateInfo compInfo{};
        compInfo.layout = irradePipelineLayout;
        compInfo.stage = irradianceStage;

        vk::Pipeline irradiancePipeline;
        if(m_logicalDevice->GetVkDevice().createComputePipelines(nullptr, 1, &compInfo, nullptr, &irradiancePipeline) != vk::Result::eSuccess)
        {
            HGFATAL("Failed to create irradiance compute pipeline!");
        }
        auto cmd = m_logicalDevice->BeginSingleTimeCommands();

        std::array<vk::DescriptorSet, 2> irradianceSets = {envSet, irradSet};

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, irradiancePipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, irradePipelineLayout, 0, irradianceSets.size(), irradianceSets.data(), 0, nullptr);

        n32 irradianceMapSize = 512;
        n32 irradianceGroupCountX = (irradianceMapSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
        n32 irradianceGroupCountY = (irradianceMapSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
        n32 irradianceGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z; // Since LOCAL_SIZE_Z is 1, this is 6

        vkCmdDispatch(cmd, irradianceGroupCountX, irradianceGroupCountY, irradianceGroupCountZ);

        m_logicalDevice->EndSingleTimeCommands(cmd);
        m_logicalDevice->GetVkDevice().destroyPipeline(irradiancePipeline, nullptr);
    }
    // Prefiltered map
    {
        vk::PipelineShaderStageCreateInfo prefilteredStage{};
        prefilteredStage.stage = vk::ShaderStageFlagBits::eCompute;
        prefilteredStage.pName = "main";
        prefilteredStage.module = prefiltredMod;

        vk::ComputePipelineCreateInfo compInfo{};
        compInfo.layout = prefiltPipelineLayout;
        compInfo.stage = prefilteredStage;

        vk::Pipeline prefilteredPipeline;
        if(m_logicalDevice->GetVkDevice().createComputePipelines(nullptr, 1, &compInfo, nullptr, &prefilteredPipeline) != vk::Result::eSuccess)
        {
            HGFATAL("Failed to create prefiltered compute pipeline!");
        }

        std::array<vk::DescriptorSet, 2> prefilteredSets = {envSet};

        auto cmd = m_logicalDevice->BeginSingleTimeCommands();
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, prefilteredPipeline);

        for(n32 mipLevel = 0; mipLevel < m_prefilteredMap->GetMipLevels(); mipLevel++)
        {
            n32 mipSize = m_prefilteredMap->GetBaseSize() >> mipLevel;
            mipSize = std::max(1u, mipSize);

            n32 prefilteredGroupCountX = (mipSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
            n32 prefilteredGroupCountY = (mipSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
            n32 prefilteredGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z;

            vk::DescriptorImageInfo info;
            info.imageLayout = vk::ImageLayout::eGeneral;
            info.imageView = m_prefilteredMipViews[mipLevel];

            vk::DescriptorSet s;
            DescriptorWriter(*prefilteredLayout, &pool).WriteImage(0, &info).Build(s);

            prefilteredSets[1] = s;
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, prefiltPipelineLayout, 0, 2, prefilteredSets.data(), 0, nullptr);

            PrefilteredData prefilterData;
            prefilterData.roughness = pow((float)mipLevel / (float)(m_prefilteredMap->GetMipLevels() - 1), 0.6);
            prefilterData.mipLevel = mipLevel;

            cmd.pushConstants(prefiltPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PrefilteredData), &prefilterData);

            vkCmdDispatch(cmd, prefilteredGroupCountX, prefilteredGroupCountY, prefilteredGroupCountZ);
        }

        m_logicalDevice->EndSingleTimeCommands(cmd);

        m_logicalDevice->GetVkDevice().destroyPipeline(prefilteredPipeline, nullptr);
    }
    // BRDF LUT
    {
        vk::PipelineShaderStageCreateInfo brdfStage{};
        brdfStage.stage = vk::ShaderStageFlagBits::eCompute;
        brdfStage.pName = "main";
        brdfStage.module = brdfMod;

        vk::ComputePipelineCreateInfo compInfo{};
        compInfo.layout = brdfPipelineLayout;
        compInfo.stage = brdfStage;

        vk::Pipeline brdfPipeline;
        if(m_logicalDevice->GetVkDevice().createComputePipelines(nullptr, 1, &compInfo, nullptr, &brdfPipeline) != vk::Result::eSuccess)
        {
            HGFATAL("Failed to create brdf compute pipeline!");
        }
        auto cmd = m_logicalDevice->BeginSingleTimeCommands();

        std::array<vk::DescriptorSet, 1> brdfSets = {brdfSet};

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, brdfPipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, brdfPipelineLayout, 0, brdfSets.size(), brdfSets.data(), 0, nullptr);

        n32 brdfMapSize = 512;
        n32 brdfGroupCountX = (brdfMapSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
        n32 brdfGroupCountY = (brdfMapSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
        n32 brdfGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z; // Since LOCAL_SIZE_Z is 1, this is 6

        vkCmdDispatch(cmd, brdfGroupCountX, brdfGroupCountY, brdfGroupCountZ);

        m_logicalDevice->EndSingleTimeCommands(cmd);

        m_logicalDevice->GetVkDevice().destroyPipeline(brdfPipeline, nullptr);
    }

    // auto cmd = m_logicalDevice->BeginSingleTimeCommands();
    // Renderer::WaitForCompute(cmd);
    // m_logicalDevice->EndSingleTimeCommands(cmd);

    // Afterwork
    m_logicalDevice->GetVkDevice().destroyShaderModule(irradianceMod, nullptr);
    m_logicalDevice->GetVkDevice().destroyShaderModule(prefiltredMod, nullptr);
    m_logicalDevice->GetVkDevice().destroyShaderModule(brdfMod, nullptr);

    m_logicalDevice->GetVkDevice().destroyPipelineLayout(irradePipelineLayout, nullptr);
    m_logicalDevice->GetVkDevice().destroyPipelineLayout(prefiltPipelineLayout, nullptr);
    m_logicalDevice->GetVkDevice().destroyPipelineLayout(brdfPipelineLayout, nullptr);
}

} // namespace Humongous
