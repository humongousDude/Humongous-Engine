#include "render_systems/simple_render_system.hpp"
#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#include "scene_handler.hpp"

namespace Humongous
{

IRenderSystem::~IRenderSystem()
{
    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); ++i) { m_pool[i].reset(); }
    m_pipeline.reset();
};

void IRenderSystem::CreatePipeline(const RenderPipeline::PipelineConfigInfo& configInfo)
{
    HGINFO("Creating render pipeline...");
    m_pipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);
    HGINFO("Created render pipeline");
}

void IRenderSystem::RenderObjectsToData(RenderData& renderData, std::vector<DrawData>& opaqueDrawData,
                                        std::vector<vk::DrawIndexedIndirectCommand>& opaqueCommands, std::vector<DrawData>& transparentDrawData,
                                        std::vector<vk::DrawIndexedIndirectCommand>& transparentCommands, std::vector<InstanceData>& instanceData)
{
    u32 instanceOffset = 0;

    std::unordered_map<u32, std::vector<u32>> staticModelIDToEntityIDs;

    for(const auto& entity: *renderData.entities)
    {
        auto modelComp = SceneHandler::GetWorld()->GetComponent<ModelComponent>(entity.id);
        if(modelComp && modelComp->instance && modelComp->instance->GetModel())
        {
            staticModelIDToEntityIDs[modelComp->instance->GetModel()->GetHandle()].push_back(entity.id);
        }
    }

    for(const auto& [staticModelID, entityIDs]: staticModelIDToEntityIDs)
    {
        if(entityIDs.empty()) { continue; }

        auto someModelComponent = SceneHandler::GetWorld()->GetComponent<ModelComponent>(entityIDs[0]);
        if(!someModelComponent || !someModelComponent->instance || !someModelComponent->instance->GetModel()) { continue; }
        const auto& staticModel = someModelComponent->instance->GetModel();

        for(const auto& [materialId, primitivesInBatch]: staticModel->GetMaterialBatches())
        {
            for(const auto& primitive: primitivesInBatch)
            {

                vk::DrawIndexedIndirectCommand cmd{};
                cmd.indexCount = primitive->indexCount;
                cmd.instanceCount = static_cast<u32>(entityIDs.size());
                cmd.firstIndex = primitive->globalFirstIndex;
                cmd.vertexOffset = primitive->globalVertexOffset;
                cmd.firstInstance = 0;

                DrawData draw{};
                draw.vertexOffset = primitive->globalVertexOffset;
                draw.materialID = primitive->material->index;
                draw.localNodeIndex = primitive->owner->index;
                draw.isSkinned = staticModel->HasSkins();
                draw.isMorphed =
                    !primitive->morphTargetPositions.empty() || !primitive->morphTargetNormals.empty() || !primitive->morphTargetTangents.empty();

                draw.instanceOffset = instanceOffset;

                if(primitive->material->alphaMode == Material::ALPHAMODE_MASK)
                {
                    transparentCommands.push_back(cmd);
                    transparentDrawData.push_back(draw);
                }
                else
                {
                    opaqueCommands.push_back(cmd);
                    opaqueDrawData.push_back(draw);
                }
            }
        }

        for(const auto& entityId: entityIDs)
        {
            const auto modelComp = SceneHandler::GetWorld()->GetComponent<ModelComponent>(entityId);
            if(!modelComp || !modelComp->instance) { continue; }

            const auto& currentModelInstance = modelComp->instance;

            InstanceData instance{};
            instance.modelMatrix = SceneHandler::GetWorld()->GetComponent<TransformComponent>(entityId)->Mat4();
            instance.modelID = currentModelInstance->GetInstanceID();
            instance.globalNodeIndex = m_resourceManager.GetModelHandleToMatrixStart(currentModelInstance->GetInstanceID());

            // TODO: fix this
            // if(drawData.back().isSkinned)
            // {
            //     instance.jointMatrixStart = m_resourceManager.Get().m_modelHandleToJointStart[currentModelInstance->GetInstanceID()].first;
            // }
            // if(drawData.back().isMorphed)
            // {
            //     instance.morphTargetStart = m_resourceManager.Get().m_modelHandleToMorphStart[currentModelInstance->GetInstanceID()].first;
            // }

            instanceData.push_back(instance);
        }
        instanceOffset += static_cast<u32>(entityIDs.size());
    }
}

void IRenderSystem::AllocateDescriptorSet()
{
    DescriptorPool::Builder poolBuilder{m_logicalDevice};
    poolBuilder.AddPoolSize(vk::DescriptorType::eStorageBuffer, 10);
    poolBuilder.SetMaxSets(10);

    auto layout = m_resourceManager.GetModelDescriptors().traditionalDrawData->GetDescriptorSetLayout();
    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); ++i)
    {
        m_pool[i] = poolBuilder.Build();
        m_pool[i]->AllocateDescriptor(layout, m_set[i]);
    }
}

TraditionalRenderSystem::TraditionalRenderSystem(const ILogicalDevice& logicalDevice, ResourceManager& resourceManager,
                                                 const IAssetManager& assetManager, const RenderPipeline::PipelineConfigInfo& configInfo)
    : IRenderSystem{logicalDevice, resourceManager, assetManager, configInfo}
{
    HGINFO("Creating simple render system...");
    AllocateDescriptorSet();
    CreatePipeline(configInfo);

    HGINFO("Created simple render system");
}

TraditionalRenderSystem::~TraditionalRenderSystem()
{
    HGINFO("Destroying simple render system...");
    m_pipeline.reset();
    m_debugBuffer.reset();

    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); ++i)
    {
        m_indirectDrawBuffers[i].reset();
        m_drawDataBuffers[i].reset();
        m_drawInstanceBuffers[i].reset();
    }

    HGINFO("Destroyed Simple render system");
}

void TraditionalRenderSystem::Render(RenderData& renderData)
{
    std::vector<DrawData>                       opaqueDrawData;
    std::vector<vk::DrawIndexedIndirectCommand> opaqueCommands;
    std::vector<DrawData>                       transparentDrawData;
    std::vector<vk::DrawIndexedIndirectCommand> transparentCommands;
    std::vector<InstanceData>                   instanceData;
    u32                                         instanceOffset = 0;

    RenderObjectsToData(renderData, opaqueDrawData, opaqueCommands, transparentDrawData, transparentCommands, instanceData);

    vk::DeviceSize indirectCommandsSize = sizeof(vk::DrawIndexedIndirectCommand) * opaqueCommands.size();
    vk::DeviceSize drawDataSize = sizeof(DrawData) * opaqueDrawData.size();
    vk::DeviceSize instanceDataSize = sizeof(InstanceData) * instanceData.size();

    if(indirectCommandsSize == 0) { return; }

    DescriptorPool* poolToUse = m_pool[renderData.frameIndex].get();

    auto& indirectBufferToUse = m_indirectDrawBuffers[renderData.frameIndex];
    auto& drawDataBufferToUse = m_drawDataBuffers[renderData.frameIndex];
    auto& instanceBufferToUse = m_drawInstanceBuffers[renderData.frameIndex];

    if(indirectBufferToUse && indirectBufferToUse->GetBufferSize() < indirectCommandsSize) { indirectBufferToUse.reset(); }
    if(drawDataBufferToUse && drawDataBufferToUse->GetBufferSize() < drawDataSize) { drawDataBufferToUse.reset(); }
    if(instanceBufferToUse && instanceBufferToUse->GetBufferSize() < instanceDataSize) { instanceBufferToUse.reset(); }

    // uploading data
    // indirect buffer

    {
        Buffer stagingBuffer{m_logicalDevice,
                             indirectCommandsSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             4,
                             "indirect staging buffer"};

        indirectBufferToUse = std::make_unique<Buffer>(
            m_logicalDevice, indirectCommandsSize, 1, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 1, "draw indirect commmand buffer");

        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)opaqueCommands.data(), opaqueCommands.size() * sizeof(vk::DrawIndexedIndirectCommand));
        stagingBuffer.UnMap();

        Buffer::CopyBuffer(m_logicalDevice, stagingBuffer, *indirectBufferToUse, indirectCommandsSize);
    }
    // draw data buffer

    {
        Buffer stagingBuffer{m_logicalDevice,
                             drawDataSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             1,
                             "draw data staging buffer"};

        drawDataBufferToUse = std::make_unique<Buffer>(
            m_logicalDevice, drawDataSize, 1, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 1, "draw data buffer");

        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)opaqueDrawData.data(), opaqueDrawData.size() * sizeof(DrawData));
        stagingBuffer.UnMap();

        Buffer::CopyBuffer(m_logicalDevice, stagingBuffer, *drawDataBufferToUse, opaqueDrawData.size() * sizeof(DrawData));
    }
    // instance data buffer

    {
        Buffer stagingBuffer{m_logicalDevice,
                             instanceDataSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             1,
                             "instance data staging buffer"};

        instanceBufferToUse = std::make_unique<Buffer>(
            m_logicalDevice, instanceDataSize, 1, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 1, "instance data buffer");

        stagingBuffer.Map();
        stagingBuffer.WriteToBuffer((void*)instanceData.data(), instanceData.size() * sizeof(InstanceData));
        stagingBuffer.UnMap();

        Buffer::CopyBuffer(m_logicalDevice, stagingBuffer, *instanceBufferToUse, instanceDataSize);
    }

    m_pipeline->Bind(renderData.commandBuffer);

    vk::DescriptorSet debugSet;
    auto              debugBufferInfo = m_debugBuffer->DescriptorInfo();

    DescriptorWriter debugWriter{*m_resourceManager.GetModelDescriptors().debugLayout, m_resourceManager.GetDescriptorPools().debugPool.get()};

    debugWriter.WriteBuffer(0, &debugBufferInfo);

    if(debugSet == VK_NULL_HANDLE) { debugWriter.Build(debugSet); }
    else { debugWriter.Overwrite(debugSet); }

    poolToUse->ResetPool();

    vk::DescriptorSet setToUse = m_set[renderData.frameIndex];

    auto buferInfo = drawDataBufferToUse->DescriptorInfo();

    auto instBufInfo = instanceBufferToUse->DescriptorInfo();

    DescriptorWriter writer{*m_resourceManager.GetModelDescriptors().traditionalDrawData, poolToUse};

    writer.WriteBuffer(0, &buferInfo).WriteBuffer(1, &instBufInfo);

    writer.Build(setToUse);

    auto globalIndexBuffer = &m_resourceManager.GetModelIndexBuffer();

    renderData.commandBuffer.bindIndexBuffer(globalIndexBuffer->GetBuffer(), 0, vk::IndexType::eUint32);

    m_resourceManager.BindGlobalDescriptorSets(renderData.commandBuffer, m_pipeline->GetPipelineLayout());

    if(!renderData.uboSets.empty())
    {
        renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->GetPipelineLayout(),
                                                    static_cast<u32>(Globals::ModelDescriptorIndices::Camera),
                                                    static_cast<u32>(renderData.uboSets.size()), renderData.uboSets.data(), 0, nullptr);
    }
    std::vector<vk::DescriptorSet> sets{setToUse};

    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->GetPipelineLayout(),
                                                static_cast<u32>(Globals::ModelDescriptorIndices::Model) + 1, sets.size(), sets.data(), 0, nullptr);

    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->GetPipelineLayout(),
                                                static_cast<u32>(Globals::ModelDescriptorIndices::Debug), 1, &debugSet, 0, nullptr);

    renderData.commandBuffer.drawIndexedIndirect(indirectBufferToUse->GetBuffer(), 0, opaqueCommands.size(),
                                                 sizeof(vk::DrawIndexedIndirectCommand));

    // m_transparentGeometryPipeline->Bind(renderData.commandBuffer);
    //
    // renderData.commandBuffer.drawIndexedIndirect(indirectBufferToUse->GetBuffer(), 0, opaqueCommands.size(),
    //                                              sizeof(vk::DrawIndexedIndirectCommand));
}

MeshRenderSystem::MeshRenderSystem(const ILogicalDevice& logicalDevice, ResourceManager& resourceManager, const IAssetManager& assetManager,
                                   const RenderPipeline::PipelineConfigInfo& configInfo)
    : IRenderSystem{logicalDevice, resourceManager, assetManager, configInfo}
{
    HGINFO("Creating mesh render system...");
    AllocateDescriptorSet();
    CreatePipeline(configInfo);

    HGINFO("Created mesh render system");
}

MeshRenderSystem::~MeshRenderSystem()
{
    HGINFO("Destroying mesh render system...");
    m_pipeline.reset();

    for(u32 i = 0; i < static_cast<u32>(Globals::Limits::MaxFramesInFlight); ++i)
    {
        m_pool[i].reset();

        m_drawDataBuffers[i].reset();
        m_instanceBuffers[i].reset();
        m_meshDataBuffers[i].reset();
        m_indirectBuffers[i].reset();
        m_shaderbufferVisibleIndices[i].reset();
        m_shaderbufferVisibleCounter[i].reset();
    }

    HGINFO("Destroyed mesh render system");
}

void MeshRenderSystem::ReadyBuffers(RenderData& renderData)
{
    std::vector<DrawData>                            drawDataVec;
    std::vector<InstanceData>                        instanceDataVec;
    std::vector<MeshletDrawInfo>                     meshletDrawInfo;
    std::vector<vk::DrawMeshTasksIndirectCommandEXT> drawCalls;
    drawDataVec.reserve(256);
    instanceDataVec.reserve(renderData.entities->size());
    u32 instanceOffset = 0;

    std::unordered_map<u32, std::vector<u32>> staticModelIDToEntityIDs;

    for(const auto& e: *renderData.entities)
    {
        auto mc = SceneHandler::GetWorld()->GetComponent<ModelComponent>(e.id);
        if(mc && mc->instance && mc->instance->GetModel()) { staticModelIDToEntityIDs[mc->instance->GetModel()->GetHandle()].push_back(e.id); }
    }

    u32 totalPossible = 0;
    for(const auto& [modelHandle, entityIDs]: staticModelIDToEntityIDs)
    {
        if(entityIDs.empty()) { continue; }

        auto someModelComponent = SceneHandler::GetWorld()->GetComponent<ModelComponent>(entityIDs[0]);

        if(!someModelComponent || !someModelComponent->instance) { continue; }

        const auto& staticModel = someModelComponent->instance->GetModel();

        for(const auto& [materialId, primitivesInBatch]: staticModel->GetMaterialBatches())
        {
            for(const auto& primitive: primitivesInBatch)
            {
                if(primitive->meshletCount == 0) { continue; }

                DrawData draw{};
                draw.vertexOffset = primitive->globalVertexOffset;
                draw.materialID = primitive->material->index;
                draw.localNodeIndex = primitive->owner->index;
                draw.isSkinned = staticModel->HasSkins();
                draw.isMorphed = staticModel->HasMorphs();
                draw.instanceOffset = instanceOffset;

                u32 drawIndex = static_cast<u32>(drawDataVec.size());
                drawDataVec.push_back(draw);

                MeshletDrawInfo dc{};
                dc.drawDataIndex = drawIndex;
                dc.meshletOffset = primitive->globalMeshletOffset;
                dc.meshletCount = primitive->meshletCount;
                dc.instanceOffset = instanceOffset;
                dc.instanceCount = static_cast<u32>(entityIDs.size());
                meshletDrawInfo.push_back(dc);

                totalPossible += entityIDs.size() * primitive->meshletCount;

                vk::DrawMeshTasksIndirectCommandEXT drawCall{};
                drawCall.groupCountX = 1;
                drawCall.groupCountY = 1;
                drawCall.groupCountZ = 1;
                drawCalls.push_back(drawCall);
            }
        }

        for(u32 entId: entityIDs)
        {
            const auto mc = SceneHandler::GetWorld()->GetComponent<ModelComponent>(entId);
            if(!mc || !mc->instance) { continue; }

            InstanceData inst{};
            inst.modelMatrix = SceneHandler::GetWorld()->GetComponent<TransformComponent>(entId)->Mat4();
            inst.modelID = mc->instance->GetInstanceID();
            inst.globalNodeIndex = m_resourceManager.GetModelHandleToMatrixStart(mc->instance->GetInstanceID());
            if(staticModel->HasSkins()) { inst.jointMatrixStart = m_resourceManager.GetModelHandleToJointStart(mc->instance->GetInstanceID()); }
            if(staticModel->HasMorphs()) { inst.morphTargetStart = m_resourceManager.GetModelHandleToMorphStart(mc->instance->GetInstanceID()); }

            instanceDataVec.push_back(inst);
        }

        instanceOffset += static_cast<u32>(entityIDs.size());
    }

    vk::DeviceSize drawDataSize = sizeof(DrawData) * drawDataVec.size();
    vk::DeviceSize instanceDataSize = sizeof(InstanceData) * instanceDataVec.size();
    vk::DeviceSize meshDataSize = sizeof(MeshletDrawInfo) * meshletDrawInfo.size();
    vk::DeviceSize meshDataIndirectSize = sizeof(vk::DrawMeshTasksIndirectCommandEXT) * drawCalls.size();
    vk::DeviceSize meshShaderBufferSize = sizeof(u32) * totalPossible;
    vk::DeviceSize meshShaderCounterSize = sizeof(u32);

    auto ensureBuffer = [&](std::unique_ptr<Buffer>& buf, vk::DeviceSize needed, vk::BufferUsageFlags usage, const char* name) {
        if(!buf || buf->GetBuffer() == VK_NULL_HANDLE || buf->GetBufferSize() < needed)
        {
            buf.reset();

            Buffer::BufferCreateInfo createInfo{.device = m_logicalDevice};
            createInfo.size = needed;
            createInfo.instanceCount = 1;
            createInfo.bufferUsage = usage;
            createInfo.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            createInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            createInfo.minOffsetAlignment = 1;
            createInfo.name = name;

            buf = std::make_unique<Buffer>(createInfo);
        }
    };

    auto& drawDataBuffer = m_drawDataBuffers[renderData.frameIndex];
    auto& instanceDataBuffer = m_instanceBuffers[renderData.frameIndex];
    auto& meshDataBuffer = m_meshDataBuffers[renderData.frameIndex];
    auto& meshDataIndirectBuffer = m_indirectBuffers[renderData.frameIndex];
    auto& meshShaderBuffer = m_shaderbufferVisibleIndices[renderData.frameIndex];
    auto& meshShaderCounterBuffer = m_shaderbufferVisibleCounter[renderData.frameIndex];

    ensureBuffer(drawDataBuffer, drawDataSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, "draw data buffer");
    ensureBuffer(instanceDataBuffer, instanceDataSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                 "instance data buffer");
    ensureBuffer(meshDataBuffer, meshDataSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, "mesh data buffer");
    ensureBuffer(meshDataIndirectBuffer, meshDataIndirectSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndirectBuffer,
                 "mesh data indirect buffer");
    ensureBuffer(meshShaderBuffer, meshShaderBufferSize,
                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
                 "mesh shader buffer");
    ensureBuffer(meshShaderCounterBuffer, meshShaderCounterSize,
                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
                 "mesh shader counter buffer");

    auto uploadToDeviceBuffer = [&](Buffer& deviceBuf, void* src, vk::DeviceSize size, const char* tmpName) {
        Buffer staging{m_logicalDevice,
                       size,
                       1,
                       vk::BufferUsageFlagBits::eTransferSrc,
                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                       VMA_MEMORY_USAGE_CPU_TO_GPU,
                       1,
                       tmpName};
        staging.Map();
        staging.WriteToBuffer(src, size);
        staging.UnMap();
        Buffer::CopyBuffer(m_logicalDevice, staging, deviceBuf, size);
    };

    if(drawDataSize > 0) { uploadToDeviceBuffer(*drawDataBuffer, drawDataVec.data(), drawDataSize, "drawdata-staging"); }
    if(instanceDataSize > 0) { uploadToDeviceBuffer(*instanceDataBuffer, instanceDataVec.data(), instanceDataSize, "instdata-staging"); }
    if(meshDataSize > 0) { uploadToDeviceBuffer(*meshDataBuffer, meshletDrawInfo.data(), meshDataSize, "meshdata-staging"); }
    if(meshDataIndirectSize > 0)
    {
        uploadToDeviceBuffer(*meshDataIndirectBuffer, drawCalls.data(), meshDataIndirectSize, "meshdata-indirect-staging");
    }

    renderData.commandBuffer.fillBuffer(meshShaderBuffer->GetBuffer(), 0, meshShaderBuffer->GetBufferSize(), 0);
    renderData.commandBuffer.fillBuffer(meshShaderCounterBuffer->GetBuffer(), 0, meshShaderCounterBuffer->GetBufferSize(), 0);

    m_drawCount[renderData.frameIndex] = static_cast<u32>(drawCalls.size());
}

void MeshRenderSystem::ReadyDescriptors(RenderData& renderData)
{
    DescriptorPool* poolToUse = m_pool[renderData.frameIndex].get();
    poolToUse->ResetPool();

    DescriptorWriter writer{*m_resourceManager.GetModelDescriptors().traditionalDrawData, poolToUse};

    auto drawInfo = m_drawDataBuffers[renderData.frameIndex]->DescriptorInfo();
    auto instInfo = m_instanceBuffers[renderData.frameIndex]->DescriptorInfo();
    auto meshInfo = m_meshDataBuffers[renderData.frameIndex]->DescriptorInfo();
    auto meshShaderInfo = m_shaderbufferVisibleIndices[renderData.frameIndex]->DescriptorInfo();
    auto meshShaderCounterInfo = m_shaderbufferVisibleCounter[renderData.frameIndex]->DescriptorInfo();

    writer.WriteBuffer(0, &drawInfo)
        .WriteBuffer(1, &instInfo)
        .WriteBuffer(2, &meshInfo)
        .WriteBuffer(3, &meshShaderInfo)
        .WriteBuffer(4, &meshShaderCounterInfo);
    writer.Build(m_set[renderData.frameIndex]);
}

void MeshRenderSystem::Render(RenderData& renderData)
{
    if(!m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders)
    {
        HGWARN("Mesh shaders are not supported on this device");
        return;
    }
    if(renderData.entities->empty()) { return; }

    m_pipeline->Bind(renderData.commandBuffer);

    m_resourceManager.BindGlobalDescriptorSets(renderData.commandBuffer, m_pipeline->GetPipelineLayout());

    if(!renderData.uboSets.empty())
    {
        renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->GetPipelineLayout(),
                                                    static_cast<u32>(Globals::ModelDescriptorIndices::Camera),
                                                    static_cast<u32>(renderData.uboSets.size()), renderData.uboSets.data(), 0, nullptr);
    }

    std::vector<vk::DescriptorSet> sets{m_set[renderData.frameIndex]};
    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->GetPipelineLayout(),
                                                static_cast<u32>(Globals::ModelDescriptorIndices::Model) + 1, (u32)sets.size(), sets.data(), 0,
                                                nullptr);

    m_logicalDevice.RecordDrawMeshIndirect(renderData.commandBuffer, m_indirectBuffers[renderData.frameIndex]->GetBuffer(), 0,
                                           m_drawCount[renderData.frameIndex], sizeof(vk::DrawMeshTasksIndirectCommandEXT));
}

} // namespace Humongous
