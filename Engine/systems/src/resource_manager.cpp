#include "resource_manager.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "audio_source.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include "model.hpp"
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

    m_modelDescriptors.vertices.reset();
    m_modelDescriptors.rendererBuffer.reset();
    m_modelDescriptors.debugLayout.reset();

    m_materialDataBuffer.reset();
    m_bindlessTexturePool.reset();
    m_bindlessLayout.reset();
    m_modelNodeMatriciesBuffer.reset();
    m_modelIndexBuffer.reset();
    m_modelVertexBuffer.reset();

    m_descriptorPools.imagePool.reset();
    m_descriptorPools.uniformPool.reset();
    m_descriptorPools.storageBufferPool.reset();
    m_descriptorPools.storageImagePool.reset();
    m_descriptorPools.debugPool.reset();

    m_skyboxLayout.reset();
    m_skyboxCompLayout.reset();

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
        *m_logicalDevice, 128, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, t1);

    DescriptorSetLayout::Builder bindlessBuilder{*m_logicalDevice};
    bindlessBuilder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 128)
        .AddBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment, 1)
        .AddBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex, 1);
    m_bindlessLayout = bindlessBuilder.Build();

    m_bindlessSet = m_bindlessTexturePool->AllocateDescriptor(m_bindlessLayout->GetDescriptorSetLayout());

    DescriptorSetLayout::Builder nodeBuilder{*m_logicalDevice};
    nodeBuilder.AddBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex);
    m_modelDescriptors.vertices = nodeBuilder.Build();

    m_descriptorPools.storageBufferPool->AllocateDescriptor(m_modelDescriptors.vertices->GetDescriptorSetLayout(),
                                                            m_modelDescriptors.vertexDescriptor);

    DescriptorSetLayout::Builder debugBuilder{*m_logicalDevice};
    debugBuilder.AddBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex);
    m_modelDescriptors.debugLayout = debugBuilder.Build();

    DescriptorSetLayout::Builder builder{*m_logicalDevice};
    builder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    m_skyboxLayout = builder.Build();

    DescriptorSetLayout::Builder builder2{*m_logicalDevice};
    builder2.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    m_skyboxCompLayout = builder2.Build();
}

n32 ResourceManager::Internal_RequestModel(const std::string& name)
{
    auto it = m_modelNameToHandle.find(name);
    if(it != m_modelNameToHandle.end()) { return it->second; }

    HGINFO("Loading model %s with handle %i", name.c_str(), m_nextModelID);
    auto m = std::make_shared<Model>(m_logicalDevice, Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::MODEL, name), 1.0f);

    n32 handleToReturn = m_nextModelID++;

    HGINFO("Model %s loaded. Added to map with handle %i. Map size: %zu", name.c_str(), handleToReturn, m_modelMap.size());

    // Node Matricies
    {
        const auto& nodeMats = m->GetMatrixVector();

        size_t newModelMatrixStartIndex = m_modelNodeMatricesFlat.size();

        m_modelNodeMatricesFlat.insert(m_modelNodeMatricesFlat.end(), nodeMats.begin(), nodeMats.end());

        m_modelHandleToMatrixStart.emplace(handleToReturn, newModelMatrixStartIndex);

        if(!m_modelNodeMatriciesBuffer || m_modelNodeMatriciesBuffer->GetBufferSize() < m_modelNodeMatricesFlat.size() * sizeof(glm::mat4))
        {
            m_modelNodeMatriciesBuffer = std::make_unique<Buffer>(
                m_logicalDevice, m_modelNodeMatricesFlat.size() * sizeof(glm::mat4), 1, vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, VMA_MEMORY_USAGE_CPU_TO_GPU, 1,
                "model node matricies buffer");
        }

        m_modelNodeMatriciesBuffer->Map();
        m_modelNodeMatriciesBuffer->WriteToBuffer(m_modelNodeMatricesFlat.data(), m_modelNodeMatricesFlat.size() * sizeof(glm::mat4));
        m_modelNodeMatriciesBuffer->UnMap();

        vk::WriteDescriptorSet write{};
        write.dstSet = m_bindlessSet;
        write.dstBinding = 2;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        auto info = m_modelNodeMatriciesBuffer->DescriptorInfo();
        write.pBufferInfo = &info;
        m_logicalDevice->GetVkDevice().updateDescriptorSets(1, &write, 0, nullptr);
    }

    m_modelMap.emplace(handleToReturn, std::move(m));
    m_modelNameToHandle.emplace(name, handleToReturn);

    return handleToReturn;
}

void Internal_AddMatriciesToModel(const std::vector<glm::mat4>& matricies) {}

void ResourceManager::Internal_AddIndicesToModel(const std::vector<n32>& modelIndices, std::vector<Primitive*>& modelPrimitives)
{
    size_t globalOffsetForNewModel = m_modelIndicies.size();

    m_modelIndicies.insert(m_modelIndicies.end(), modelIndices.begin(), modelIndices.end());

    for(Primitive* primitive: modelPrimitives)
    {
        if(primitive->hasIndices) { primitive->firstIndex = static_cast<n32>(globalOffsetForNewModel + primitive->firstIndex); }
    }

    vk::DeviceSize requiredBufferSize = m_modelIndicies.size() * sizeof(n32);

    if(!m_modelIndexBuffer || m_modelIndexBuffer->GetBufferSize() < requiredBufferSize)
    {
        m_modelIndexBuffer =
            std::make_unique<Buffer>(m_logicalDevice, requiredBufferSize,
                                     1, // Alignment
                                     vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                     vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY, 1, "global index buffer");
    }

    Buffer stagingBuffer(m_logicalDevice, requiredBufferSize, 1, vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_CPU_TO_GPU);

    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer(m_modelIndicies.data(), requiredBufferSize);
    stagingBuffer.UnMap();

    Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_modelIndexBuffer, requiredBufferSize);
}

void ResourceManager::Internal_AddVerticesToModel(const std::vector<Model::Vertex>& modelVertices, const std::vector<Mesh*>& modelMeshes)
{
    const size_t baseVertex = m_modelVertices.size();

    m_modelVertices.insert(m_modelVertices.end(), modelVertices.begin(), modelVertices.end());

    for(Mesh* mesh: modelMeshes)
    {
        mesh->baseVertex = static_cast<n32>(baseVertex);

        for(Primitive* prim: mesh->primitives) { prim->vertexOffset = static_cast<n32>(baseVertex + prim->localVertexStart); }
    }

    HGINFO("We now have %i vertices", modelVertices.size());

    const vk::DeviceSize requiredBufferSize = m_modelVertices.size() * sizeof(Model::Vertex);

    if(!m_modelVertexBuffer || m_modelVertexBuffer->GetBufferSize() < requiredBufferSize)
    {
        m_modelVertexBuffer.reset();

        m_modelVertexBuffer =
            std::make_unique<Buffer>(m_logicalDevice, requiredBufferSize, 1,
                                     vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                         vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                     vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY, 16, "global vertex buffer");
    }

    Buffer stagingBuffer(m_logicalDevice, requiredBufferSize, 1, vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_CPU_TO_GPU);

    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer(m_modelVertices.data(), requiredBufferSize);
    stagingBuffer.UnMap();

    Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, *m_modelVertexBuffer, requiredBufferSize);

    auto             bufInfo = m_modelVertexBuffer->DescriptorInfo();
    DescriptorWriter writer{*m_modelDescriptors.vertices, m_descriptorPools.storageBufferPool.get()};
    writer.WriteBuffer(0, &bufInfo);
    writer.Overwrite(m_modelDescriptors.vertexDescriptor);
}

std::shared_ptr<Model> ResourceManager::Internal_GetModel(const n32& index) { return m_modelMap.at(index); }

n32 ResourceManager::Internal_RequestModelNodeMatriciesIndex(const n32& modelHandle) { return m_modelHandleToMatrixStart[modelHandle]; }

std::shared_ptr<Skybox> ResourceManager::Internal_LoadSkybox(const std::string& name)
{
    SkyboxCreateInfo info{.logicalDevice = m_logicalDevice,
                          .cubemapPath = Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::TEXTURE, name),
                          .descriptorSetLayout = *m_skyboxLayout,
                          .compDescriptorSetLayout = *m_skyboxCompLayout,
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

    n32 handleToReturn = m_nextaudioID;
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
        m_materialDataBuffer = std::make_unique<Buffer>(m_logicalDevice, rawMaterials.size() * sizeof(Model::ShaderMaterial), 1,
                                                        vk::BufferUsageFlagBits::eStorageBuffer,
                                                        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                                        VMA_MEMORY_USAGE_CPU_TO_GPU, 1, "global material data buffer");
    }

    m_materialDataBuffer->Map();
    m_materialDataBuffer->WriteToBuffer(rawMaterials.data(), rawMaterials.size() * sizeof(Model::ShaderMaterial));
    m_materialDataBuffer->UnMap();

    auto bufInfo = m_materialDataBuffer->DescriptorInfo();

    vk::WriteDescriptorSet write{};
    write.dstSet = m_bindlessSet;
    write.dstBinding = 1;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eStorageBuffer;
    write.pBufferInfo = &bufInfo;

    m_logicalDevice->GetVkDevice().updateDescriptorSets(1, &write, 0, nullptr);

    return bindlessIndex;
}

void ResourceManager::Internal_BindGlobalDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout layout)
{
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, static_cast<n32>(Globals::ModelDescriptorIndices::Model), 1, &m_bindlessSet, 0,
                           nullptr);
}

} // namespace Humongous
