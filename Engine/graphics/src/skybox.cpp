#include "skybox.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "cmath"
#include "compute_pipeline.hpp"
#include "logger.hpp"

namespace Humongous
{

Skybox::Skybox(const SkyboxCreateInfo& createInfo) : m_logicalDevice{createInfo.logicalDevice}
{
    LoadCubemap(createInfo.cubemapPath);
    GeneratePBRImages(createInfo.uniformPool, createInfo.imagePool, createInfo.storageImagePool);
    LoadDescriptorSet(createInfo.descriptorSetLayout, createInfo.compDescriptorSetLayout, &createInfo.imagePool);
}

Skybox::~Skybox()
{
    m_skybox->Destroy();
    m_irradiance->Destroy();
    m_logicalDevice.GetVkDevice().destroyImageView(m_irradianceWriteView, nullptr);

    for(auto& view: m_prefilteredReadViews) { m_logicalDevice.GetVkDevice().destroyImageView(view, nullptr); }
    for(auto& view: m_prefilteredWriteViews) { m_logicalDevice.GetVkDevice().destroyImageView(view, nullptr); }

    m_prefilteredMap->Destroy();
    m_brdflut->Destroy();
}

void Skybox::LoadCubemap(const std::string& cubemapPath)
{
    m_skybox = std::make_unique<Texture>(m_logicalDevice, cubemapPath, Texture::ImageType::CUBEMAP);
}

void Skybox::LoadDescriptorSet(DescriptorSetLayout& descriptorLayout, DescriptorSetLayout& compLayout, DescriptorPoolGrowable* pool)
{
    auto                    imgInfo = m_skybox->GetDescriptorInfo();
    auto                    irradInfo = m_irradiance->GetDescriptorInfo();
    vk::DescriptorImageInfo info{m_prefilteredMap->GetRawSamplerHandle(), m_prefilteredReadViews[1], m_prefilteredMap->GetRawImageLayout()};
    auto                    brdfInfo = m_brdflut->GetDescriptorInfo();
    DescriptorWriter(descriptorLayout, pool)
        .WriteImage(0, &imgInfo)
        .WriteImage(1, &irradInfo)
        .WriteImage(2, &info)
        .WriteImage(3, &brdfInfo)
        .Build(m_cubeMapSet);

    DescriptorWriter(compLayout, pool)
        .WriteImage(0, &imgInfo)
        .WriteImage(1, &irradInfo)
        .WriteImage(2, &info)
        .WriteImage(3, &brdfInfo)
        .Build(m_compCubeMapSet);
}

struct PrefilteredData
{
    float roughness;
    u32   mipLevel;
};

void Skybox::CreatePrefilteredMipViews()
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = m_prefilteredMap->GetRawImageHandle();
    viewInfo.viewType = vk::ImageViewType::eCube;
    viewInfo.format = vk::Format::eR16G16B16A16Sfloat;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    m_prefilteredReadViews.resize(m_prefilteredMap->GetMipLevels());
    m_prefilteredWriteViews.resize(m_prefilteredMap->GetMipLevels());
    for(uint32_t mipLevel = 0; mipLevel < m_prefilteredMap->GetMipLevels(); ++mipLevel)
    {
        viewInfo.viewType = vk::ImageViewType::eCube;
        viewInfo.subresourceRange.baseMipLevel = mipLevel;
        viewInfo.subresourceRange.levelCount = 1;

        if(m_logicalDevice.GetVkDevice().createImageView(&viewInfo, nullptr, &m_prefilteredReadViews[mipLevel]) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create image view for prefiltered map read mip level %u", mipLevel);
        }

        viewInfo.viewType = vk::ImageViewType::e2DArray;
        if(m_logicalDevice.GetVkDevice().createImageView(&viewInfo, nullptr, &m_prefilteredWriteViews[mipLevel]) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create image view for prefiltered map write mip level %u", mipLevel);
        }
    }

    if(m_logicalDevice.GetVkDevice().createImageView(&viewInfo, nullptr, &m_irradianceWriteView) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create image view for irradiance map");
    }
}

void Skybox::GeneratePBRImages(DescriptorPoolGrowable& uniformPool, DescriptorPoolGrowable& combinedImagePool,
                               DescriptorPoolGrowable& storageImagePool)
{
    // Prep work
    m_irradiance =
        std::make_unique<Texture>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::TEXTURE, "papermill"),
                                  Texture::ImageType::CUBEMAP, true);

    m_prefilteredMap =
        std::make_unique<Texture>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::TEXTURE, "papermill"),
                                  Texture::ImageType::CUBEMAP, true);

    CreatePrefilteredMipViews();

    m_brdflut = std::make_unique<Texture>(m_logicalDevice);
    m_brdflut->FillWithEmpty(m_logicalDevice, 512, 512, true);

    DescriptorSetLayout::Builder envImageBuilder{m_logicalDevice};
    envImageBuilder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    auto envLayout = envImageBuilder.Build();

    DescriptorSetLayout::Builder IrradImageBuilder{m_logicalDevice};
    IrradImageBuilder.AddBinding(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute);
    auto irradLayout = IrradImageBuilder.Build();

    DescriptorSetLayout::Builder PrefiltImageBuilder{m_logicalDevice};
    PrefiltImageBuilder.AddBinding(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute);
    auto prefilteredLayout = PrefiltImageBuilder.Build();

    DescriptorSetLayout::Builder brdfImageBuilder{m_logicalDevice};
    brdfImageBuilder.AddBinding(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute);
    auto brdfLayout = brdfImageBuilder.Build();

    std::array<vk::DescriptorSetLayout, 2> irradDescriptorLayouts = {envLayout->GetDescriptorSetLayout(), irradLayout->GetDescriptorSetLayout()};

    vk::PipelineLayoutCreateInfo irradPipelineLayoutInfo{};
    irradPipelineLayoutInfo.setLayoutCount = irradDescriptorLayouts.size();
    irradPipelineLayoutInfo.pSetLayouts = irradDescriptorLayouts.data();
    irradPipelineLayoutInfo.pushConstantRangeCount = 0;
    irradPipelineLayoutInfo.pPushConstantRanges = nullptr;

    vk::PipelineLayout irradePipelineLayout;
    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&irradPipelineLayoutInfo, nullptr, &irradePipelineLayout) != vk::Result::eSuccess)
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
    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&prefiltPipelineLayoutInfo, nullptr, &prefiltPipelineLayout) != vk::Result::eSuccess)
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
    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&brdfPipelineLayoutInfo, nullptr, &brdfPipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout");
    }

    vk::DescriptorSet envSet;
    vk::DescriptorSet irradSet;
    vk::DescriptorSet brdfSet;

    auto imgInfo = m_skybox->GetDescriptorInfo();
    auto irradianceInfo = m_irradiance->GetDescriptorInfo();
    irradianceInfo.imageView = m_irradianceWriteView;
    auto brdflutInfo = m_brdflut->GetDescriptorInfo();

    DescriptorWriter(*envLayout, &combinedImagePool).WriteImage(0, &imgInfo).Build(envSet);
    DescriptorWriter(*irradLayout, &storageImagePool).WriteImage(0, &irradianceInfo).Build(irradSet);
    DescriptorWriter(*brdfLayout, &storageImagePool).WriteImage(0, &brdflutInfo).Build(brdfSet);

    const u32 LOCAL_SIZE_X = 16;
    const u32 LOCAL_SIZE_Y = 16;
    const u32 LOCAL_SIZE_Z = 1;

    // Irradiance map
    HGINFO("Generating irradiance map");
    {
        ComputePipeline::ComputePipelineCreateInfo configInfo{m_logicalDevice};
        configInfo.pipelineLayout = irradePipelineLayout;
        configInfo.shaderFile = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "irradiance.comp");

        ComputePipeline irradiancePipeline{configInfo};
        auto            cmd = m_logicalDevice.BeginSingleTimeCommands();

        std::array<vk::DescriptorSet, 2> irradianceSets = {envSet, irradSet};

        irradiancePipeline.BindPipeline(cmd);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, irradePipelineLayout, 0, irradianceSets.size(), irradianceSets.data(), 0, nullptr);

        u32 irradianceMapSize = 512;
        u32 irradianceGroupCountX = (irradianceMapSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
        u32 irradianceGroupCountY = (irradianceMapSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
        u32 irradianceGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z; // Since LOCAL_SIZE_Z is 1, this is 6

        vkCmdDispatch(cmd, irradianceGroupCountX, irradianceGroupCountY, irradianceGroupCountZ);

        m_logicalDevice.EndSingleTimeCommands(cmd);
    }
    HGINFO("Done generating irradiance map");

    // Prefiltered map
    HGINFO("Generating prefiltered map");
    {
        ComputePipeline::ComputePipelineCreateInfo configInfo{m_logicalDevice};
        configInfo.pipelineLayout = prefiltPipelineLayout;
        configInfo.shaderFile = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "prefiltredmap.comp");
        ComputePipeline prefilteredPipeline{configInfo};

        std::array<vk::DescriptorSet, 2> prefilteredSets = {envSet};

        auto cmd = m_logicalDevice.BeginSingleTimeCommands();
        prefilteredPipeline.BindPipeline(cmd);

        for(u32 mipLevel = 0; mipLevel < m_prefilteredMap->GetMipLevels(); mipLevel++)
        {
            u32 mipSize = m_prefilteredMap->GetBaseSize() >> mipLevel;
            mipSize = std::max(1u, mipSize);

            u32 prefilteredGroupCountX = (mipSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
            u32 prefilteredGroupCountY = (mipSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
            u32 prefilteredGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z;

            vk::DescriptorImageInfo info;
            info.imageLayout = vk::ImageLayout::eGeneral;
            info.imageView = m_prefilteredWriteViews[mipLevel];

            vk::DescriptorSet s;
            DescriptorWriter(*prefilteredLayout, &storageImagePool).WriteImage(0, &info).Build(s);

            prefilteredSets[1] = s;
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, prefiltPipelineLayout, 0, 2, prefilteredSets.data(), 0, nullptr);

            PrefilteredData prefilterData;
            prefilterData.roughness = pow((float)mipLevel / (float)(m_prefilteredMap->GetMipLevels() - 1), 0.6);
            prefilterData.mipLevel = mipLevel;

            cmd.pushConstants(prefiltPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PrefilteredData), &prefilterData);

            vkCmdDispatch(cmd, prefilteredGroupCountX, prefilteredGroupCountY, prefilteredGroupCountZ);
        }

        m_logicalDevice.EndSingleTimeCommands(cmd);
    }
    HGINFO("Done generating prefiltered map");
    // BRDF LUT
    HGINFO("Generating BRDF LUT");
    {
        ComputePipeline::ComputePipelineCreateInfo configInfo{m_logicalDevice};
        configInfo.pipelineLayout = brdfPipelineLayout;
        configInfo.shaderFile = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "brdflut.comp");
        ComputePipeline brdfPipeline{configInfo};

        auto cmd = m_logicalDevice.BeginSingleTimeCommands();

        std::array<vk::DescriptorSet, 1> brdfSets = {brdfSet};

        brdfPipeline.BindPipeline(cmd);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, brdfPipelineLayout, 0, brdfSets.size(), brdfSets.data(), 0, nullptr);

        u32 brdfMapSize = 512;
        u32 brdfGroupCountX = (brdfMapSize + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
        u32 brdfGroupCountY = (brdfMapSize + LOCAL_SIZE_Y - 1) / LOCAL_SIZE_Y;
        u32 brdfGroupCountZ = (6 + LOCAL_SIZE_Z - 1) / LOCAL_SIZE_Z;

        vkCmdDispatch(cmd, brdfGroupCountX, brdfGroupCountY, brdfGroupCountZ);

        m_logicalDevice.EndSingleTimeCommands(cmd);
    }
    HGINFO("Done generating BRDF LUT");

    // Afterwork
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(irradePipelineLayout, nullptr);
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(prefiltPipelineLayout, nullptr);
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(brdfPipelineLayout, nullptr);
}

} // namespace Humongous
