#include "resource_manager.hpp"
#include "asset_manager.hpp"
#include "audio_source.hpp"
#include "logger.hpp"
#include "skybox.hpp"
#include <AL/al.h>

namespace Humongous
{

void ResourceManager::Internal_Init(LogicalDevice* device)
{
    HGINFO("Initializing Resource manager...");
    m_logicalDevice = device;
    InitDescriptors();
    HGINFO("Resource manager initialized");
}

void ResourceManager::Internal_Shutdown()
{
    HGINFO("Shutting down resource manager...");

    HGINFO("Destroying %i models", m_modelMap.size());
    HGINFO("Destroying %i textures", m_textureMap.size());

    for(auto& [key, model]: m_modelMap) { model.reset(); }
    for(auto& [key, texture]: m_textureMap) { texture.texture.Destroy(); }

    m_modelDescriptors.nodeLayout.reset();
    m_modelDescriptors.debugLayout.reset();

    m_materialDataBuffer.reset();
    m_bindlessTexturePool.reset();
    m_bindlessLayout.reset();

    m_descriptorPools.imagePool.reset();
    m_descriptorPools.uniformPool.reset();
    m_descriptorPools.storageBufferPool.reset();
    m_descriptorPools.storageImagePool.reset();
    m_descriptorPools.debugPool.reset();

    m_skyboxLayout.reset();

    HGINFO("Resource manager shutdown");
}

void ResourceManager::InitDescriptors()
{
    std::vector<vk::DescriptorType> t1 = {vk::DescriptorType::eCombinedImageSampler, vk::DescriptorType::eStorageBuffer};
    std::vector<vk::DescriptorType> t2 = {vk::DescriptorType::eUniformBuffer};
    std::vector<vk::DescriptorType> t3 = {vk::DescriptorType::eStorageBuffer};
    std::vector<vk::DescriptorType> t4 = {vk::DescriptorType::eStorageImage};

    m_descriptorPools.imagePool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t1);
    m_descriptorPools.uniformPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t2);
    m_descriptorPools.storageBufferPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t3);
    m_descriptorPools.storageImagePool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t4);
    m_descriptorPools.debugPool =
        std::make_unique<DescriptorPoolGrowable>(*m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t3);

    m_bindlessTexturePool = std::make_unique<DescriptorPoolGrowable>(
        *m_logicalDevice, 1024, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, t1);

    DescriptorSetLayout::Builder bbbbbbbb{*m_logicalDevice};
    bbbbbbbb.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1024)
        .AddBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment, 1);
    m_bindlessLayout = bbbbbbbb.Build();

    m_bindlessSet = m_bindlessTexturePool->AllocateDescriptor(m_bindlessLayout->GetDescriptorSetLayout());

    DescriptorSetLayout::Builder nodeBuilder{*m_logicalDevice};
    nodeBuilder.AddBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex);
    m_modelDescriptors.nodeLayout = nodeBuilder.Build();

    DescriptorSetLayout::Builder debugBuilder{*m_logicalDevice};
    debugBuilder.AddBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex);
    m_modelDescriptors.debugLayout = debugBuilder.Build();

    DescriptorSetLayout::Builder builder{*m_logicalDevice};
    builder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    m_skyboxLayout = builder.Build();
}

n32 ResourceManager::Internal_LoadModel(const std::string& name)
{
    auto it = m_modelNameToHandle.find(name);
    if(it != m_modelNameToHandle.end()) { return it->second; }

    HGINFO("Loading model %s with handle %i", name.c_str(), m_nextModelID);
    auto m = std::make_shared<Model>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::MODEL, name), 1.0f);
    m->Init(m_modelDescriptors.nodeLayout.get(), m_descriptorPools.imagePool.get(), m_descriptorPools.uniformPool.get(),
            m_descriptorPools.storageBufferPool.get());

    n32 handleToReturn = m_nextModelID++;
    m_modelMap.emplace(handleToReturn, std::move(m));

    HGINFO("Model %s loaded. Added to map with handle %i. Map size: %zu", name.c_str(), handleToReturn, m_modelMap.size());
    m_modelMap.emplace(handleToReturn, m);
    m_modelNameToHandle.emplace(name, handleToReturn);
    return handleToReturn;
}

std::shared_ptr<Model> ResourceManager::Internal_GetModel(const n32& index) { return m_modelMap.at(index); }

std::shared_ptr<Skybox> ResourceManager::Internal_LoadSkybox(const std::string& name)
{
    SkyboxCreateInfo info{.logicalDevice = m_logicalDevice,
                          .cubemapPath = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::TEXTURE, name),
                          .descriptorSetLayout = *m_skyboxLayout,
                          .imagePool = *m_descriptorPools.imagePool,
                          .uniformPool = *m_descriptorPools.uniformPool,
                          .storageImagePool = *m_descriptorPools.storageImagePool};
    auto             s = std::make_shared<Skybox>(info);
    return s;
}

n32 ResourceManager::Internal_LoadAudioSource(const std::string& name)
{
    HGINFO("Loading audio %s with handle %i", name.c_str(), m_nextaudioID);

    std::shared_ptr<AudioSourceComponent> a =
        std::make_shared<AudioSourceComponent>(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::AUDIO, name));

    n32 handleToReturn = m_nextModelID;
    m_audioMap.emplace(handleToReturn, std::move(a));

    HGINFO("audio %s loaded. Added to map with handle %i. Map size: %zu", name.c_str(), handleToReturn, m_modelMap.size());
    m_nextaudioID++;
    return handleToReturn;
}

std::shared_ptr<AudioSourceComponent> ResourceManager::Internal_GetAudioSource(const n32& index) { return m_audioMap.at(index); }

std::string GenerateImageKey(const tinygltf::Image& img)
{
    size_t      dataHash = std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const char*>(img.image.data()), img.image.size()));
    std::string key = "img_" + std::to_string(dataHash);
    return key;
}

std::string GenerateMaterialKey(const Model::ShaderMaterial& mat)
{
    const char* data = reinterpret_cast<const char*>(&mat);
    size_t      dataSize = sizeof(Model::ShaderMaterial);

    size_t dataHash = std::hash<std::string_view>{}(std::string_view(data, dataSize));
    return "mat_" + std::to_string(dataHash);
}

n32 ResourceManager::Internal_RequestTexture(class tinygltf::Image img, struct Texture::TexSamplerInfo sampler)
{
    std::string key = GenerateImageKey(img);

    auto it = m_textureMap.find(key);
    if(it != m_textureMap.end()) { return it->second.bindlessIndex; }

    Texture tex{};
    tex.CreateFromGLTFImage(img, sampler, m_logicalDevice, m_logicalDevice->GetGraphicsQueue());

    uint32_t bindlessIndex = m_nextBindlessIndex++;

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.imageView = tex.GetRawImageViewHandle();
    imageInfo.sampler = tex.GetRawSamplerHandle();
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    if(m_bindlessImageInfos.size() <= bindlessIndex) { m_bindlessImageInfos.resize(bindlessIndex + 1); }
    m_bindlessImageInfos[bindlessIndex] = imageInfo;

    vk::WriteDescriptorSet write{};
    write.dstSet = m_bindlessSet;
    write.dstBinding = 0;
    write.dstArrayElement = bindlessIndex;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &m_bindlessImageInfos[bindlessIndex];

    m_logicalDevice->GetVkDevice().updateDescriptorSets(1, &write, 0, nullptr);

    m_textureMap[key] = TextureBinding{std::move(tex), bindlessIndex};
    return bindlessIndex;
}

n32 ResourceManager::Internal_RequestMaterial(const Model::ShaderMaterial& mat)
{
    std::string key = GenerateMaterialKey(mat);

    auto it = m_materialMap.find(key);
    if(it != m_materialMap.end()) { return it->second; }

    n32 bindlessIndex = static_cast<n32>(m_materials.size());
    m_materials.push_back({mat, bindlessIndex});
    m_materialMap[key] = bindlessIndex;

    std::vector<Model::ShaderMaterial> rawMaterials;
    rawMaterials.reserve(m_materials.size());

    for(const auto& binding: m_materials) { rawMaterials.push_back(binding.material); }

    if(!m_materialDataBuffer || m_materialDataBuffer->GetBufferSize() < rawMaterials.size() * sizeof(Model::ShaderMaterial))
    {
        m_materialDataBuffer = std::make_unique<Buffer>(
            m_logicalDevice, rawMaterials.size() * sizeof(Model::ShaderMaterial), 1, vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, VMA_MEMORY_USAGE_CPU_TO_GPU);

        auto bufInfo = m_materialDataBuffer->DescriptorInfo();

        vk::WriteDescriptorSet write{};
        write.dstSet = m_bindlessSet;
        write.dstBinding = 1;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        write.pBufferInfo = &bufInfo;

        m_logicalDevice->GetVkDevice().updateDescriptorSets(1, &write, 0, nullptr);
    }

    m_materialDataBuffer->Map();
    m_materialDataBuffer->WriteToBuffer(rawMaterials.data(), rawMaterials.size() * sizeof(Model::ShaderMaterial));
    m_materialDataBuffer->UnMap();

    return bindlessIndex;
}

void ResourceManager::Internal_BindGlobalDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout layout)
{
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 3, 1, &m_bindlessSet, 0, nullptr);
}

} // namespace Humongous
