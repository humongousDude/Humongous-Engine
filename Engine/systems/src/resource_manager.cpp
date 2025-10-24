#include "resource_manager.hpp"
#include "asset_manager.hpp"
#include "audio_source.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include "skybox.hpp"
#include <AL/al.h>

namespace Humongous
{

ResourceManager::ResourceManager(const ILogicalDevice& logicalDevice, const IAssetManager& assetManager)
    : m_logicalDevice(logicalDevice), m_assetManager(assetManager)
{
    HGINFO("Initializing Resource manager...");
    HGINFO("Initializing descriptors...");
    InitDescriptors();
    HGINFO("Initializing initials...");
    InitializeInitials();
    HGINFO("Resource manager initialized");
}

ResourceManager::~ResourceManager()
{
    HGINFO("Shutting down resource manager...");

    HGINFO("Destroying %i models", m_modelMap.size());
    HGINFO("Destroying %i textures", m_textureMap.size());

    for(auto& [key, model]: m_modelMap) { model.reset(); }
    for(auto& [key, texture]: m_textureMap)
    {
        if(!texture.texture) { continue; }
        texture.texture->Destroy();
    }

    if(m_modelDescriptors.traditionalDrawData) { m_modelDescriptors.traditionalDrawData.reset(); }
    if(m_modelDescriptors.rendererBuffer) { m_modelDescriptors.rendererBuffer.reset(); }
    if(m_modelDescriptors.debugLayout) { m_modelDescriptors.debugLayout.reset(); }

    if(m_materialDataBuffer) { m_materialDataBuffer.reset(); }
    if(m_bindlessTexturePool) { m_bindlessTexturePool.reset(); }
    if(m_bindlessLayout) { m_bindlessLayout.reset(); }
    if(m_modelNodeMatriciesBuffer) { m_modelNodeMatriciesBuffer.reset(); }
    if(m_modelIndexBuffer) { m_modelIndexBuffer.reset(); }
    if(m_modelVertexBuffer) { m_modelVertexBuffer.reset(); }

    if(m_descriptorPools.imagePool) { m_descriptorPools.imagePool.reset(); }
    if(m_descriptorPools.uniformPool) { m_descriptorPools.uniformPool.reset(); }
    if(m_descriptorPools.storageBufferPool) { m_descriptorPools.storageBufferPool.reset(); }
    if(m_descriptorPools.storageImagePool) { m_descriptorPools.storageImagePool.reset(); }
    if(m_descriptorPools.debugPool) { m_descriptorPools.debugPool.reset(); }

    if(m_modelMorphTargetsBuffer) { m_modelMorphTargetsBuffer.reset(); }
    if(m_modelJointMatriciesBuffer) { m_modelJointMatriciesBuffer.reset(); }

    if(m_skyboxLayout) { m_skyboxLayout.reset(); }
    if(m_skyboxCompLayout) { m_skyboxCompLayout.reset(); }

    HGINFO("Resource manager shutdown");
}

void ResourceManager::InitDescriptors()
{
    std::vector<vk::DescriptorType> t1 = {vk::DescriptorType::eCombinedImageSampler, vk::DescriptorType::eStorageBuffer};
    std::vector<vk::DescriptorType> t2 = {vk::DescriptorType::eUniformBuffer};
    std::vector<vk::DescriptorType> t3 = {vk::DescriptorType::eStorageBuffer};
    std::vector<vk::DescriptorType> t4 = {vk::DescriptorType::eStorageImage};

    m_descriptorPools.imagePool =
        std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t1);
    m_descriptorPools.uniformPool =
        std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t2);
    m_descriptorPools.storageBufferPool =
        std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t3);
    m_descriptorPools.storageImagePool =
        std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t4);
    m_descriptorPools.debugPool =
        std::make_unique<DescriptorPoolGrowable>(m_logicalDevice, 10, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, t3);

    m_bindlessTexturePool = std::make_unique<DescriptorPoolGrowable>(
        m_logicalDevice, 64, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, t1);

    b8                   canUseMeshShaders = m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders;
    vk::ShaderStageFlags vertexStage;
    if(canUseMeshShaders) { vertexStage = vk::ShaderStageFlagBits::eTaskEXT | vk::ShaderStageFlagBits::eMeshEXT; }
    else
    {
        vertexStage = vk::ShaderStageFlagBits::eVertex;
    }

    DescriptorSetLayout::Builder bindlessBuilder{m_logicalDevice};
    bindlessBuilder
        .AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 64) // textures
        .AddBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment, 1)         // materials
        .AddBinding(2, vk::DescriptorType::eStorageBuffer, vertexStage, 1)                                // nodes
        .AddBinding(3, vk::DescriptorType::eStorageBuffer, vertexStage, 1)                                // vertices
        .AddBinding(4, vk::DescriptorType::eStorageBuffer, vertexStage, 1)                                // joints
        .AddBinding(5, vk::DescriptorType::eStorageBuffer, vertexStage, 1);                               // morphs

    if(canUseMeshShaders)
    {
        // meshlets
        bindlessBuilder
            .AddBinding(6, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eTaskEXT | vk::ShaderStageFlagBits::eMeshEXT, 1)
            // meshlet vertices
            .AddBinding(7, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eTaskEXT | vk::ShaderStageFlagBits::eMeshEXT, 1)
            // meshlet primitives
            .AddBinding(8, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eTaskEXT | vk::ShaderStageFlagBits::eMeshEXT, 1);
    }

    m_bindlessLayout = bindlessBuilder.Build();

    m_bindlessSet = m_bindlessTexturePool->AllocateDescriptor(m_bindlessLayout->GetDescriptorSetLayout());

    DescriptorSetLayout::Builder drawDataBuilder{m_logicalDevice};
    drawDataBuilder.AddBinding(0, vk::DescriptorType::eStorageBuffer, vertexStage); // DrawData
    drawDataBuilder.AddBinding(1, vk::DescriptorType::eStorageBuffer, vertexStage); // InstanceData
    if(m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders)
    {
        // MeshletDrawInfo
        drawDataBuilder.AddBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eTaskEXT | vk::ShaderStageFlagBits::eMeshEXT, 1);
    }
    m_modelDescriptors.traditionalDrawData = drawDataBuilder.Build();

    DescriptorSetLayout::Builder debugBuilder{m_logicalDevice};
    debugBuilder.AddBinding(0, vk::DescriptorType::eStorageBuffer, vertexStage);
    m_modelDescriptors.debugLayout = debugBuilder.Build();

    DescriptorSetLayout::Builder builder{m_logicalDevice};
    builder.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    builder.AddBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    m_skyboxLayout = builder.Build();

    DescriptorSetLayout::Builder builder2{m_logicalDevice};
    builder2.AddBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    builder2.AddBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eCompute);
    m_skyboxCompLayout = builder2.Build();
}

void ResourceManager::InitializeInitials()
{
    {
        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = 32;
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "model node matricies buffer";
        m_modelNodeMatriciesBuffer = std::make_unique<Buffer>(createInfo);

        vk::WriteDescriptorSet write{};
        write.dstSet = m_bindlessSet;
        write.dstBinding = 2;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        auto info = m_modelNodeMatriciesBuffer->DescriptorInfo();
        write.pBufferInfo = &info;
        m_logicalDevice.UpdateDescriptorSets({write});
    }

    {
        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = 32;
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "global jointMatricies buffer";
        m_modelJointMatriciesBuffer = std::make_unique<Buffer>(createInfo);

        vk::WriteDescriptorSet write{};
        write.dstSet = m_bindlessSet;
        write.dstBinding = 4;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        auto info = m_modelJointMatriciesBuffer->DescriptorInfo();
        write.pBufferInfo = &info;
        m_logicalDevice.UpdateDescriptorSets({write});
    }

    {
        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = 32;
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "global morphTargets buffer";
        m_modelMorphTargetsBuffer = std::make_unique<Buffer>(createInfo);

        vk::WriteDescriptorSet write{};
        write.dstSet = m_bindlessSet;
        write.dstBinding = 5;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        auto info = m_modelMorphTargetsBuffer->DescriptorInfo();
        write.pBufferInfo = &info;
        m_logicalDevice.UpdateDescriptorSets({write});
    }

    auto newTex = std::make_unique<Texture>(m_logicalDevice);
    newTex->FillWithEmpty(m_logicalDevice, 512, 512, false);

    u32 bindlessIndex = m_nextBindlessIndex++;

    m_textures.push_back(std::move(newTex));

    Texture* actualTexturePtr = m_textures[bindlessIndex].get();

    vk::DescriptorImageInfo imageInfo = actualTexturePtr->GetDescriptorInfo();

    if(m_bindlessImageInfos.size() <= bindlessIndex) { m_bindlessImageInfos.resize(bindlessIndex + 1); }
    m_bindlessImageInfos[bindlessIndex] = imageInfo;

    vk::WriteDescriptorSet write{};
    write.dstSet = m_bindlessSet;
    write.dstBinding = 0;
    write.dstArrayElement = bindlessIndex;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &m_bindlessImageInfos[bindlessIndex];

    m_logicalDevice.UpdateDescriptorSets({write});

    std::string key =
        "img_0_emptyifthissomehowcausesacachehitthenimgonnascreamnowaythishappensright?"
        "alsothisisverylongiwonderiftheresalimitimagineifthelengthofthiskeycausedthelookuptoslowdownthatwouldbeveryfunnyalsoiatesomegoodfoodtodayit"
        "wasverytastyhighlyrecommendanywayhowasyourdayimcurrentlywatchingavtuberreacttogabrieliglesiassracistgiftbasketvideoalsojapaneseisveryfunhi"
        "ghlyrecommendyoulearnitandthememesoverinjapanarealsoveryfunyoushouldlookthemupevenifyoudontunderstandjapaneseyoumightstillenjoythemwealthf"
        "amepowerthelegendarypirategolDrogerobtainedallofthisandmoreyouwantmytreasureyoucanhaveitileftallthereattheendoftheworldandsopeopleracedtot"
        "heseasinsearchoftheirdreamsthisistrulythebeginningofthegreatpirateera";
    m_textureMap[key] = TextureBinding{actualTexturePtr, bindlessIndex};
}

u32 ResourceManager::LoadModel(const std::string& name)
{
    auto it = m_modelNameToHandle.find(name);
    if(it != m_modelNameToHandle.end()) { return it->second; }
    m_prevModelID = m_nextModelID;

    u32 handleToReturn = m_nextModelID++;

    auto path = m_assetManager.GetAsset(AssetManager::AssetType::MODEL, name);
    HGINFO("Loading model %s with handle %i", name.c_str(), handleToReturn);

    auto m = std::make_shared<Model>(*this, path, 1.0f, handleToReturn);

    HGINFO("Model %s loaded. Added to map with handle %i. Map size: %zu", name.c_str(), handleToReturn, m_modelMap.size());

    auto& vertices = m->GetVertices();
    auto& indices = m->GetIndices();
    auto& meshes = m->GetMeshes();
    auto& primitives = m->GetPrimitives();
    auto& meshlets = m->GetMeshlets();
    auto& meshletVertices = m->GetMeshletVertices();
    auto& meshletPrimitives = m->GetMeshletPrimitives();

    AddVerticesToModel(vertices, meshes);
    AddIndicesToModel(indices, primitives);

    if(!meshlets.empty()) { AddMeshletsToModel(primitives, meshlets, meshletVertices, meshletPrimitives, handleToReturn); }

    m_modelMap.emplace(handleToReturn, std::move(m));
    m_modelNameToHandle.emplace(name, handleToReturn);

    return handleToReturn;
}

void ResourceManager::AddMeshletsToModel(std::vector<Primitive*>& primitives, std::vector<Meshlet>& meshlets,
                                         const std::vector<u32>& meshletVertices, const std::vector<u8>& meshletPrimitives, const u32& handle)
{
    if(!m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders)
    {
        HGWARN("Tried to add meshlets to a model, but the device does not support them! Skipping...");
        return;
    }

    const u32 meshletStartIndex = m_meshlets.size();
    const u32 meshletVertexStartIndex = m_meshletVertices.size();
    const u32 meshletPrimitiveStartIndex = m_meshletPrimitives.size();

    for(auto* primitive: primitives) { primitive->globalMeshletOffset = static_cast<u32>(meshletStartIndex + primitive->localMeshletOffset); }
    for(auto& meshlet: meshlets)
    {
        meshlet.globalVertexOffset = static_cast<u32>(meshletVertexStartIndex + meshlet.localVertexOffset);
        meshlet.globalIndexOffset = static_cast<u32>(meshletPrimitiveStartIndex + meshlet.localIndexOffset);
    }

    m_modelHandleToMeshletStart.emplace(handle, std::pair<u32, u32>(meshletStartIndex, meshlets.size()));
    m_meshlets.insert(m_meshlets.end(), meshlets.begin(), meshlets.end());
    m_meshletVertices.insert(m_meshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
    m_meshletPrimitives.insert(m_meshletPrimitives.end(), meshletPrimitives.begin(), meshletPrimitives.end());

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();

    // Meshlets
    std::vector<std::unique_ptr<Buffer>> stagingBuffers;
    {
        if(!m_meshletBuffer || m_meshletBuffer->GetBufferSize() < m_meshlets.size() * sizeof(Meshlet))
        {
            Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
            createInfo.size = m_meshlets.size() * sizeof(Meshlet);
            createInfo.instanceCount = 1;
            createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
            createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            createInfo.minOffsetAlignment = 16;
            createInfo.name = "meshlet buffer";
            m_meshletBuffer.reset();
            m_meshletBuffer = std::make_unique<Buffer>(createInfo);

            vk::WriteDescriptorSet write{};
            write.dstSet = m_bindlessSet;
            write.dstBinding = 6;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = vk::DescriptorType::eStorageBuffer;
            auto info = m_meshletBuffer->DescriptorInfo();
            write.pBufferInfo = &info;
            m_logicalDevice.UpdateDescriptorSets({write});
        }

        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = m_meshlets.size() * sizeof(Meshlet);
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "meshlet staging buffer";
        std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>(createInfo);

        stagingBuffer->Map();
        stagingBuffer->WriteToBuffer(m_meshlets.data(), m_meshlets.size() * sizeof(Meshlet));
        stagingBuffer->UnMap();

        Buffer::CopyBuffer(m_logicalDevice, cmd, *stagingBuffer, *m_meshletBuffer, m_meshlets.size() * sizeof(Meshlet));
        stagingBuffers.push_back(std::move(stagingBuffer));
    }

    // Meshlet vertices
    {
        if(!m_meshletVertexBuffer || m_meshletVertexBuffer->GetBufferSize() < m_meshletVertices.size() * sizeof(u32))
        {
            Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
            createInfo.size = m_meshletVertices.size() * sizeof(u32);
            createInfo.instanceCount = 1;
            createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
            createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            createInfo.minOffsetAlignment = 16;
            createInfo.name = "meshlet vertex buffer";
            m_meshletVertexBuffer.reset();
            m_meshletVertexBuffer = std::make_unique<Buffer>(createInfo);

            vk::WriteDescriptorSet write{};
            write.dstSet = m_bindlessSet;
            write.dstBinding = 7;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = vk::DescriptorType::eStorageBuffer;
            auto info = m_meshletVertexBuffer->DescriptorInfo();
            write.pBufferInfo = &info;
            m_logicalDevice.UpdateDescriptorSets({write});
        }

        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = m_meshletVertices.size() * sizeof(u32);
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "meshlet vertex staging buffer";
        std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>(createInfo);

        stagingBuffer->Map();
        stagingBuffer->WriteToBuffer(m_meshletVertices.data(), m_meshletVertices.size() * sizeof(u32));
        stagingBuffer->UnMap();

        Buffer::CopyBuffer(m_logicalDevice, cmd, *stagingBuffer, *m_meshletVertexBuffer, m_meshletVertices.size() * sizeof(u32));
        stagingBuffers.push_back(std::move(stagingBuffer));
    }

    // Meshlet primitives
    {
        if(!m_meshletPrimitiveBuffer || m_meshletPrimitiveBuffer->GetBufferSize() < m_meshletPrimitives.size() * sizeof(u8))
        {
            Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
            createInfo.size = m_meshletPrimitives.size() * sizeof(u8);
            createInfo.instanceCount = 1;
            createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
            createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            createInfo.minOffsetAlignment = 16;
            createInfo.name = "meshlet primitive buffer";
            m_meshletPrimitiveBuffer.reset();
            m_meshletPrimitiveBuffer = std::make_unique<Buffer>(createInfo);

            vk::WriteDescriptorSet write{};
            write.dstSet = m_bindlessSet;
            write.dstBinding = 8;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = vk::DescriptorType::eStorageBuffer;
            auto info = m_meshletPrimitiveBuffer->DescriptorInfo();
            write.pBufferInfo = &info;
            m_logicalDevice.UpdateDescriptorSets({write});
        }

        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = m_meshletPrimitives.size() * sizeof(u8);
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "meshlet primitive staging buffer";
        std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>(createInfo);

        stagingBuffer->Map();
        stagingBuffer->WriteToBuffer(m_meshletPrimitives.data(), m_meshletPrimitives.size() * sizeof(u8));
        stagingBuffer->UnMap();

        Buffer::CopyBuffer(m_logicalDevice, cmd, *stagingBuffer, *m_meshletPrimitiveBuffer, m_meshletPrimitives.size() * sizeof(u8));
        stagingBuffers.push_back(std::move(stagingBuffer));
    }

    cmd.end();
    m_logicalDevice.GetWorkScheduler().AddWork(cmd, m_logicalDevice.GetGraphicsQueue());
    m_logicalDevice.GetWorkScheduler().AddStagingBuffers(stagingBuffers);
}

std::shared_ptr<ModelInstance> ResourceManager::RequestModel(const std::string& name)
{
    auto id = LoadModel(name);
    auto m = GetModel(id);
    auto inst = std::make_shared<ModelInstance>(m, *this, m_nextInstanceID);

    size_t newModelMatrixStartIndex = m_modelNodeMatricesFlat.size();

    m_modelNodeMatricesFlat.insert(m_modelNodeMatricesFlat.end(), inst->GetNodeMatrices().begin(), inst->GetNodeMatrices().end());

    m_modelHandleToMatrixStart.emplace(m_nextInstanceID, std::pair<u32, u32>(newModelMatrixStartIndex, inst->GetNodeMatrices().size()));

    if(!m_modelNodeMatriciesBuffer || m_modelNodeMatriciesBuffer->GetBufferSize() < m_modelNodeMatricesFlat.size() * sizeof(Eigen::Matrix4f))
    {
        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = m_modelNodeMatricesFlat.size() * sizeof(Eigen::Matrix4f);
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "model node matricies buffer";
        m_modelNodeMatriciesBuffer = std::make_unique<Buffer>(createInfo);

        vk::WriteDescriptorSet write{};
        write.dstSet = m_bindlessSet;
        write.dstBinding = 2;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        auto info = m_modelNodeMatriciesBuffer->DescriptorInfo();
        write.pBufferInfo = &info;
        m_logicalDevice.UpdateDescriptorSets({write});
    }

    m_modelNodeMatriciesBuffer->Map();
    m_modelNodeMatriciesBuffer->WriteToBuffer(m_modelNodeMatricesFlat.data(), m_modelNodeMatricesFlat.size() * sizeof(Eigen::Matrix4f));
    m_modelNodeMatriciesBuffer->UnMap();

    if(inst->HasJoints()) { AddJointMatriciesToModel(inst->GetJointMatrices(), m_nextInstanceID); }
    if(inst->HasMorphs()) { AddMorphTargetsToModel(inst->GetMorphWeights(), m_nextInstanceID); }

    m_nextInstanceID++;

    auto [it, inserted] = m_modelInstanceMap.emplace(m_nextInstanceID, std::move(inst));
    return it->second;
}

void ResourceManager::AddIndicesToModel(const std::vector<u32>& modelIndices, std::vector<Primitive*>& modelPrimitives)
{
    HGINFO("Adding indices to model...");
    size_t globalOffsetForNewModel = m_modelIndicies.size();

    m_modelIndicies.insert(m_modelIndicies.end(), modelIndices.begin(), modelIndices.end());

    for(Primitive* primitive: modelPrimitives)
    {
        if(primitive->hasIndices) { primitive->globalFirstIndex = static_cast<u32>(globalOffsetForNewModel + primitive->localFirstIndex); }
    }

    vk::DeviceSize requiredBufferSize = m_modelIndicies.size() * sizeof(u32);

    if(!m_modelIndexBuffer || m_modelIndexBuffer->GetBufferSize() < requiredBufferSize)
    {
        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = requiredBufferSize;
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "global index buffer";
        m_modelIndexBuffer = std::make_unique<Buffer>(createInfo);
    }

    Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
    createInfo.size = requiredBufferSize;
    createInfo.instanceCount = 1;
    createInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
    createInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    createInfo.minOffsetAlignment = 16;
    createInfo.name = "global index staging buffer";
    std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>(createInfo);

    stagingBuffer->Map();
    stagingBuffer->WriteToBuffer(m_modelIndicies.data(), requiredBufferSize);
    stagingBuffer->UnMap();

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();
    Buffer::CopyBuffer(m_logicalDevice, cmd, *stagingBuffer, *m_modelIndexBuffer, requiredBufferSize);
    cmd.end();
    m_logicalDevice.GetWorkScheduler().AddWork(cmd, m_logicalDevice.GetGraphicsQueue());

    std::vector<std::unique_ptr<Buffer>> stagingBuffers;
    stagingBuffers.push_back(std::move(stagingBuffer));
    m_logicalDevice.GetWorkScheduler().AddStagingBuffers(stagingBuffers);

    HGINFO("Indices added to model");
}

void ResourceManager::AddVerticesToModel(const std::vector<Model::Vertex>& modelVertices, const std::vector<Mesh*>& modelMeshes)
{
    const size_t baseVertex = m_modelVertices.size();

    for(Mesh* mesh: modelMeshes)
    {
        mesh->baseVertex = static_cast<u32>(baseVertex);

        for(auto& prim: mesh->primitives) { prim->globalVertexOffset = static_cast<u32>(baseVertex + prim->localVertexOffset); }
    }

    m_modelVertices.insert(m_modelVertices.end(), modelVertices.begin(), modelVertices.end());

    HGINFO("We now have %i vertices", modelVertices.size());

    const vk::DeviceSize requiredBufferSize = m_modelVertices.size() * sizeof(Model::Vertex);

    if(!m_modelVertexBuffer || m_modelVertexBuffer->GetBufferSize() < requiredBufferSize)
    {
        m_modelVertexBuffer.reset();

        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = requiredBufferSize;
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                 vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "global vertex buffer";
        m_modelVertexBuffer = std::make_unique<Buffer>(createInfo);

        vk::WriteDescriptorSet write{};
        write.dstSet = m_bindlessSet;
        write.dstBinding = 3;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        auto info = m_modelVertexBuffer->DescriptorInfo();
        write.pBufferInfo = &info;
        m_logicalDevice.UpdateDescriptorSets({write});
    }

    Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
    createInfo.size = requiredBufferSize;
    createInfo.instanceCount = 1;
    createInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
    createInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    createInfo.minOffsetAlignment = 16;
    createInfo.name = "global vertex staging buffer";
    std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>(createInfo);

    stagingBuffer->Map();
    stagingBuffer->WriteToBuffer(m_modelVertices.data(), requiredBufferSize);
    stagingBuffer->UnMap();

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();
    Buffer::CopyBuffer(m_logicalDevice, cmd, *stagingBuffer, *m_modelVertexBuffer, requiredBufferSize);
    cmd.end();
    m_logicalDevice.GetWorkScheduler().AddWork(cmd, m_logicalDevice.GetGraphicsQueue());
    std::vector<std::unique_ptr<Buffer>> stagingBuffers;
    stagingBuffers.push_back(std::move(stagingBuffer));
    m_logicalDevice.GetWorkScheduler().AddStagingBuffers(stagingBuffers);
}

void ResourceManager::UpdateNodeMatrices(const std::vector<Eigen::Matrix4f>& nodeMatrices, const u32& handle)
{
    u32 startPos = m_modelHandleToMatrixStart.at(handle).first;

    std::copy(nodeMatrices.begin(), nodeMatrices.end(), m_modelNodeMatricesFlat.begin() + startPos);
}

void ResourceManager::AddJointMatriciesToModel(const std::vector<Eigen::Matrix4f>& jointMatricies, const u32& handle)
{
    u32 startPos = m_modelJointMatricies.size();

    m_modelJointMatricies.insert(m_modelJointMatricies.end(), jointMatricies.begin(), jointMatricies.end());

    m_modelHandleToJointStart.emplace(handle, std::pair<u32, u32>(startPos, jointMatricies.size()));
}

void ResourceManager::UpdateJointMatrices(const std::vector<Eigen::Matrix4f>& jointMatricies, const u32& handle)
{
    u32 startPos = m_modelHandleToJointStart.at(handle).first;

    std::copy(jointMatricies.begin(), jointMatricies.end(), m_modelJointMatricies.begin() + startPos);
}

void ResourceManager::AddMorphTargetsToModel(const std::vector<f32>& morphTargets, const u32& handle)
{
    u32 startPos = m_modelMorphTargets.size();

    m_modelMorphTargets.insert(m_modelMorphTargets.end(), morphTargets.begin(), morphTargets.end());

    m_modelHandleToMorphStart.emplace(handle, std::pair<u32, u32>(startPos, morphTargets.size()));
}

void ResourceManager::UpdateMorphTargets(const std::vector<f32>& morphTargets, const u32& handle)
{
    u32 startPos = m_modelHandleToMorphStart.at(handle).first;

    std::copy(morphTargets.begin(), morphTargets.end(), m_modelMorphTargets.begin() + startPos);
}

void ResourceManager::FinalizeGPUData()
{
    auto                                 cmd = m_logicalDevice.BeginSingleTimeCommands();
    std::vector<std::unique_ptr<Buffer>> stagingBuffers;
    if(!m_modelJointMatricies.empty())
    {
        const vk::DeviceSize requiredBufferSize = m_modelJointMatricies.size() * sizeof(Eigen::Matrix4f);

        if(!m_modelJointMatriciesBuffer || m_modelJointMatriciesBuffer->GetBufferSize() < requiredBufferSize)
        {
            m_modelJointMatriciesBuffer.reset();

            Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
            createInfo.size = requiredBufferSize;
            createInfo.instanceCount = 1;
            createInfo.bufferUsage =
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress;
            createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            createInfo.minOffsetAlignment = 16;
            createInfo.name = "global jointMatricies buffer";
            m_modelJointMatriciesBuffer = std::make_unique<Buffer>(createInfo);

            vk::WriteDescriptorSet write{};
            write.dstSet = m_bindlessSet;
            write.dstBinding = 4;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = vk::DescriptorType::eStorageBuffer;
            auto info = m_modelJointMatriciesBuffer->DescriptorInfo();
            write.pBufferInfo = &info;
            m_logicalDevice.UpdateDescriptorSets({write});
        }

        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = requiredBufferSize;
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "global jointMatricies staging buffer";
        std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>(createInfo);

        stagingBuffer->Map();
        stagingBuffer->WriteToBuffer(m_modelJointMatricies.data(), requiredBufferSize);
        stagingBuffer->UnMap();

        Buffer::CopyBuffer(m_logicalDevice, cmd, *stagingBuffer, *m_modelJointMatriciesBuffer, requiredBufferSize);
        stagingBuffers.push_back(std::move(stagingBuffer));
    }

    if(!m_modelMorphTargets.empty())
    {
        const vk::DeviceSize requiredBufferSize = m_modelMorphTargets.size() * sizeof(f32);

        if(!m_modelMorphTargetsBuffer || m_modelMorphTargetsBuffer->GetBufferSize() < requiredBufferSize)
        {
            m_modelMorphTargetsBuffer.reset();

            Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
            createInfo.size = requiredBufferSize;
            createInfo.instanceCount = 1;
            createInfo.bufferUsage =
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress;
            createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            createInfo.minOffsetAlignment = 16;
            createInfo.name = "global morphTargets buffer";
            m_modelMorphTargetsBuffer = std::make_unique<Buffer>(createInfo);

            vk::WriteDescriptorSet write{};
            write.dstSet = m_bindlessSet;
            write.dstBinding = 5;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = vk::DescriptorType::eStorageBuffer;
            auto info = m_modelMorphTargetsBuffer->DescriptorInfo();
            write.pBufferInfo = &info;
            m_logicalDevice.UpdateDescriptorSets({write});
        }

        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = requiredBufferSize;
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "global morphTargets staging buffer";
        std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>(createInfo);

        stagingBuffer->Map();
        stagingBuffer->WriteToBuffer(m_modelMorphTargets.data(), requiredBufferSize);
        stagingBuffer->UnMap();

        Buffer::CopyBuffer(m_logicalDevice, cmd, *stagingBuffer, *m_modelMorphTargetsBuffer, requiredBufferSize);
        stagingBuffers.push_back(std::move(stagingBuffer));
    }

    if(!m_modelNodeMatricesFlat.empty())
    {
        const vk::DeviceSize requiredBufferSize = m_modelNodeMatricesFlat.size() * sizeof(Eigen::Matrix4f);

        if(!m_modelNodeMatriciesBuffer || m_modelNodeMatriciesBuffer->GetBufferSize() < requiredBufferSize)
        {
            m_modelNodeMatriciesBuffer.reset();

            Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
            createInfo.size = requiredBufferSize;
            createInfo.instanceCount = 1;
            createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
            createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            createInfo.minOffsetAlignment = 16;
            createInfo.name = "global nodeMatrices buffer";
            m_modelNodeMatriciesBuffer = std::make_unique<Buffer>(createInfo);

            vk::WriteDescriptorSet write{};
            write.dstSet = m_bindlessSet;
            write.dstBinding = 2;
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = vk::DescriptorType::eStorageBuffer;
            auto info = m_modelNodeMatriciesBuffer->DescriptorInfo();
            write.pBufferInfo = &info;
            m_logicalDevice.UpdateDescriptorSets({write});
        }

        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = requiredBufferSize;
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eTransferSrc;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "global nodeMatrices staging buffer";
        std::unique_ptr<Buffer> stagingBuffer = std::make_unique<Buffer>(createInfo);

        stagingBuffer->Map();
        stagingBuffer->WriteToBuffer(m_modelNodeMatricesFlat.data(), requiredBufferSize);
        stagingBuffer->UnMap();

        Buffer::CopyBuffer(m_logicalDevice, cmd, *stagingBuffer, *m_modelNodeMatriciesBuffer, requiredBufferSize);
        stagingBuffers.push_back(std::move(stagingBuffer));
    }
    cmd.end();
    m_logicalDevice.GetWorkScheduler().AddWork(cmd, m_logicalDevice.GetGraphicsQueue());

    m_logicalDevice.GetWorkScheduler().AddStagingBuffers(stagingBuffers);
}

std::shared_ptr<Model> ResourceManager::GetModel(const u32& index)
{
    auto i = m_modelMap.at(index);
    return i;
}

u32 ResourceManager::RequestModelNodeMatriciesIndex(const u32& modelHandle)
{
    auto it = m_modelHandleToMatrixStart.find(modelHandle);
    if(it == m_modelHandleToMatrixStart.end()) { return -1; }
    return it->second.first;
}

std::shared_ptr<Skybox> ResourceManager::LoadSkybox(const std::string& name)
{
    HGINFO("Loading skybox %s", name.c_str());
    SkyboxCreateInfo info{.logicalDevice = m_logicalDevice,
                          .cubemapPath = m_assetManager.GetAsset(AssetManager::AssetType::TEXTURE, name),
                          .descriptorSetLayout = *m_skyboxLayout,
                          .compDescriptorSetLayout = *m_skyboxCompLayout,
                          .imagePool = *m_descriptorPools.imagePool,
                          .uniformPool = *m_descriptorPools.uniformPool,
                          .storageImagePool = *m_descriptorPools.storageImagePool};
    auto             s = std::make_shared<Skybox>(info, m_assetManager);
    HGINFO("Skybox %s loaded", name.c_str());
    return s;
}

u32 ResourceManager::LoadAudioSource(const std::string& name)
{
    HGINFO("Loading audio %s with handle %i", name.c_str(), m_nextaudioID);

    std::shared_ptr<AudioSourceComponent> a = std::make_shared<AudioSourceComponent>(m_assetManager.GetAsset(AssetManager::AssetType::AUDIO, name));

    u32 handleToReturn = m_nextaudioID;
    m_audioMap.emplace(handleToReturn, std::move(a));

    HGINFO("audio %s loaded. Added to map with handle %i. Map size: %zu", name.c_str(), handleToReturn, m_modelMap.size());
    m_nextaudioID++;
    return handleToReturn;
}

std::shared_ptr<AudioSourceComponent> ResourceManager::GetAudioSource(const u32& index) { return m_audioMap.at(index); }

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

u32 ResourceManager::RequestTexture(tinygltf::Image img, struct Texture::TexSamplerInfo sampler)
{
    std::string key = GenerateImageKey(img);

    auto it = m_textureMap.find(key);
    if(it != m_textureMap.end()) { return it->second.bindlessIndex; }

    auto newTex = std::make_unique<Texture>(m_logicalDevice);
    newTex->CreateFromGLTFImage(img, sampler);

    u32 bindlessIndex = m_nextBindlessIndex++;

    m_textures.push_back(std::move(newTex));

    Texture* actualTexturePtr = m_textures[bindlessIndex].get();

    vk::DescriptorImageInfo imageInfo = actualTexturePtr->GetDescriptorInfo();

    if(m_bindlessImageInfos.size() <= bindlessIndex) { m_bindlessImageInfos.resize(bindlessIndex + 1); }
    m_bindlessImageInfos[bindlessIndex] = imageInfo;

    vk::WriteDescriptorSet write{};
    write.dstSet = m_bindlessSet;
    write.dstBinding = 0;
    write.dstArrayElement = bindlessIndex;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &m_bindlessImageInfos[bindlessIndex];

    m_logicalDevice.UpdateDescriptorSets({write});

    m_textureMap[key] = TextureBinding{actualTexturePtr, bindlessIndex};

    return bindlessIndex;
}

u32 ResourceManager::RequestMaterial(const Model::ShaderMaterial& mat)
{
    std::string key = GenerateMaterialKey(mat);

    auto it = m_materialMap.find(key);
    if(it != m_materialMap.end()) { return it->second; }

    u32 bindlessIndex = static_cast<u32>(m_materials.size());
    m_materials.push_back({mat, bindlessIndex});
    m_materialMap[key] = bindlessIndex;

    std::vector<Model::ShaderMaterial> rawMaterials;
    rawMaterials.reserve(m_materials.size());

    for(const auto& binding: m_materials) { rawMaterials.push_back(binding.material); }

    if(!m_materialDataBuffer || m_materialDataBuffer->GetBufferSize() < rawMaterials.size() * sizeof(Model::ShaderMaterial))
    {
        Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
        createInfo.size = rawMaterials.size() * sizeof(Model::ShaderMaterial);
        createInfo.instanceCount = 1;
        createInfo.bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer;
        createInfo.properties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
        createInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        createInfo.minOffsetAlignment = 16;
        createInfo.name = "global material data buffer";
        m_materialDataBuffer = std::make_unique<Buffer>(createInfo);
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

    m_logicalDevice.UpdateDescriptorSets({write});

    return bindlessIndex;
}

void ResourceManager::BindGlobalDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout layout)
{
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, static_cast<u32>(Globals::ModelDescriptorIndices::Model), 1, &m_bindlessSet, 0,
                           nullptr);
}

} // namespace Humongous
