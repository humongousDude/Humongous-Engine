#include "render_systems/simple_render_system.hpp"
#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#include "scene_handler.hpp"
#include "swapchain.hpp"

namespace Humongous
{
static const uint WORKGROUP_SIZE = 64;

SimpleRenderSystem::SimpleRenderSystem(const ILogicalDevice& logicalDevice, ResourceManager& resourceManager, const IAssetManager& assetManager,
                                       const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, const ShaderSet& shaderSet)
    : m_logicalDevice{logicalDevice}, m_resourceManager{resourceManager}, m_assetManager{assetManager}, m_pipelineLayout{VK_NULL_HANDLE}
{
    HGINFO("Creating simple render system...");
    AllocateDescriptorSet();
    CreatePipelineLayout(descriptorSetLayouts);
    CreatePipeline(shaderSet);

    m_indirectDrawBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_drawInstanceBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthIndirectDrawBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_drawDataBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthDrawDataBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthInstanceBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_meshDataDrawBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthMeshDataDrawBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_meshIndirectDrawBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthMeshIndirectDrawBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    HGINFO("Created simple render system");
}

SimpleRenderSystem::~SimpleRenderSystem()
{
    HGINFO("Destroying simple render system...");
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_pipelineLayout, nullptr);
    m_opaqueGeometryPipeline.reset();
    m_depthOnlyPipeline.reset();
    m_debugBuffer.reset();

    for(u32 i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_indirectDrawBuffers[i].reset();
        m_drawInstanceBuffers[i].reset();
        m_depthIndirectDrawBuffers[i].reset();
        m_drawDataBuffers[i].reset();
        m_depthInstanceBuffers[i].reset();
        m_depthDrawDataBuffers[i].reset();
        m_meshDataDrawBuffers[i].reset();
        m_depthMeshDataDrawBuffers[i].reset();
        m_meshIndirectDrawBuffers[i].reset();
        m_depthMeshIndirectDrawBuffers[i].reset();
    }

    HGINFO("Destroyed Simple render system");
}

void SimpleRenderSystem::AllocateDescriptorSet()
{
    m_pool.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthPool.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_set.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    auto targetVertexStage = m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders ? vk::ShaderStageFlagBits::eMeshEXT
                                                                                                              : vk::ShaderStageFlagBits::eVertex;
    DescriptorSetLayout::Builder layoutBuilder{m_logicalDevice};
    layoutBuilder.AddBinding(0, vk::DescriptorType::eStorageBuffer, targetVertexStage, 1);
    layoutBuilder.AddBinding(1, vk::DescriptorType::eStorageBuffer, targetVertexStage, 1);

    if(m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders)
    {
        layoutBuilder.AddBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eTaskEXT, 1);
    }

    m_layout = layoutBuilder.Build();

    DescriptorPool::Builder poolBuilder{m_logicalDevice};
    poolBuilder.AddPoolSize(vk::DescriptorType::eStorageBuffer, 3);
    poolBuilder.SetMaxSets(3);

    for(u32 i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_pool[i] = poolBuilder.Build();
        m_depthPool[i] = poolBuilder.Build();
        m_pool[i]->AllocateDescriptor(m_layout->GetDescriptorSetLayout(), m_set[i]);
        m_depthPool[i]->AllocateDescriptor(m_layout->GetDescriptorSetLayout(), m_depthSet[i]);
    }
}

void SimpleRenderSystem::CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& layouts)
{
    HGINFO("Creating pipeline layout...");
    auto descriptorSetLayouts = m_resourceManager.GetLayoutVector();
    descriptorSetLayouts.insert(descriptorSetLayouts.begin(), layouts.begin(), layouts.end());

    // vk::PushConstantRange pushConstantRange{};
    // pushConstantRange.offset = 0;
    // pushConstantRange.size = sizeof(MeshletPushConstants);
    // pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eTaskEXT | vk::ShaderStageFlagBits::eMeshEXT;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<u32>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    // if(m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders)
    // {
    //     pipelineLayoutInfo.pushConstantRangeCount = 1;
    //     pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    // }

    if(m_logicalDevice.GetVkDevice().createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pipelineLayout) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create pipeline layout");
    }

    HGINFO("Created pipeline layout");
}

void SimpleRenderSystem::CreatePipeline(const ShaderSet& shaderSet)
{
    HGINFO("Creating geometry pipeline...");
    RenderPipeline::PipelineConfigInfo configInfo = RenderPipeline::DefaultPipelineConfigInfo();
    configInfo.pipelineLayout = m_pipelineLayout;
    configInfo.rasterizationInfo.cullMode = vk::CullModeFlagBits::eNone;
    configInfo.inputAssemblyInfo.primitiveRestartEnable = false;

    configInfo.multisampleInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
    configInfo.multisampleInfo.sampleShadingEnable = false;
    configInfo.multisampleInfo.minSampleShading = 1.0;

    configInfo.vertShaderPath = shaderSet.vertShaderPath;
    configInfo.fragShaderPath = shaderSet.fragShaderPath;

    configInfo.colorBlendAttachment.blendEnable = false;
    configInfo.colorAttachmentFormat = vk::Format::eR8G8B8A8Unorm;

    configInfo.colorBlendAttachments.clear();
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);

    configInfo.colorAttachmentFormats.clear();
    configInfo.colorAttachmentFormats.push_back(configInfo.colorAttachmentFormat);
    configInfo.colorAttachmentFormats.push_back(configInfo.colorAttachmentFormat);
    configInfo.colorAttachmentFormats.push_back(configInfo.colorAttachmentFormat);
    configInfo.colorBlendInfo.attachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.colorAttachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.pColorAttachmentFormats = configInfo.colorAttachmentFormats.data();
    configInfo.colorBlendInfo.pAttachments = configInfo.colorBlendAttachments.data();

    configInfo.renderingInfo.depthAttachmentFormat = vk::Format::eD32SfloatS8Uint;
    configInfo.colorBlendInfo.logicOpEnable = false;
    configInfo.depthStencilInfo.depthCompareOp = vk::CompareOp::eEqual;
    configInfo.depthStencilInfo.depthWriteEnable = false;
    configInfo.depthStencilInfo.stencilTestEnable = true;

    configInfo.useMeshShaders = true;
    configInfo.meshShaderPath = m_assetManager.GetAsset(AssetManager::AssetType::SHADER, "simple.mesh");
    configInfo.taskShaderPath = m_assetManager.GetAsset(AssetManager::AssetType::SHADER, "simple.task");

    vk::StencilOpState stencilState{};
    stencilState.compareOp = vk::CompareOp::eAlways;
    stencilState.passOp = vk::StencilOp::eReplace;
    stencilState.reference = static_cast<u32>(Globals::StencilMasks::Model);
    stencilState.compareMask = 0xFF;
    stencilState.writeMask = 0xFF;
    configInfo.depthStencilInfo.front = stencilState;
    configInfo.depthStencilInfo.back = stencilState;
    configInfo.renderingInfo.stencilAttachmentFormat = vk::Format::eD32SfloatS8Uint;

    m_opaqueGeometryPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);

    for(auto& colorBlendAttachment: configInfo.colorBlendAttachments) { colorBlendAttachment.blendEnable = true; }

    m_transparentGeometryPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);

    HGINFO("Created geometry pipeline, now creating depth pipeline...");

    configInfo.colorAttachmentFormat = vk::Format::eUndefined;
    configInfo.colorAttachmentFormats.clear();
    configInfo.colorBlendAttachments.clear();
    configInfo.colorBlendInfo.attachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.colorAttachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.pColorAttachmentFormats = configInfo.colorAttachmentFormats.data();
    configInfo.colorBlendInfo.pAttachments = configInfo.colorBlendAttachments.data();

    configInfo.depthStencilInfo.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
    configInfo.depthStencilInfo.depthTestEnable = true;
    configInfo.depthStencilInfo.depthWriteEnable = true;
    configInfo.depthStencilInfo.stencilTestEnable = false;

    configInfo.renderingInfo.stencilAttachmentFormat = vk::Format::eUndefined;

    configInfo.depthStencilInfo.front = vk::StencilOpState{};
    configInfo.depthStencilInfo.front = vk::StencilOpState{};

    configInfo.useFragmentShader = false;

    m_depthOnlyPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);
    HGINFO("Created depth pipeline");

    m_debugBuffer = std::make_unique<Buffer>(m_logicalDevice, 1, sizeof(u32), vk::BufferUsageFlagBits::eStorageBuffer,
                                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                             VMA_MEMORY_USAGE_AUTO, 1, "Render system debug buffer");
    m_debugBuffer->Map();
    m_debugBuffer->WriteToBuffer(&m_verticesDrawn);
    m_debugBuffer->UnMap();
}

struct MeshletDrawCallInfo
{
    u32 meshletOffset;
    u32 drawDataIndex;
    u32 instanceOffset;
    u32 instanceCount;
    u32 meshletCount;
};

void SimpleRenderSystem::RenderObjectsMesh(RenderData& renderData, const b8& depthOnly)
{
    if(!m_logicalDevice.GetPhysicalDevice().GetCurrentCapabilities().supportsMeshShaders)
    {
        RenderObjectsTraditional(renderData, depthOnly);
        return;
    }
    if(renderData.visibleEntities->empty()) { return; }

    std::vector<DrawData>                            drawDataVec;
    std::vector<InstanceData>                        instanceDataVec;
    std::vector<MeshletDrawCallInfo>                 meshletDrawCalls;
    std::vector<vk::DrawMeshTasksIndirectCommandEXT> drawCalls;
    drawDataVec.reserve(256);
    instanceDataVec.reserve(renderData.visibleEntities->size());
    u32 instanceOffset = 0;

    std::unordered_map<u32, std::vector<u32>> staticModelIDToEntityIDs;
    for(const auto& e: *renderData.visibleEntities)
    {
        auto mc = SceneHandler::GetWorld()->GetComponent<ModelComponent>(e.id);
        if(mc && mc->instance && mc->instance->GetModel()) { staticModelIDToEntityIDs[mc->instance->GetModel()->GetHandle()].push_back(e.id); }
    }

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
                draw.isSkinned = staticModel->HasSkins() ? 1u : 0u;
                draw.isMorphed =
                    (!primitive->morphTargetPositions.empty() || !primitive->morphTargetNormals.empty() || !primitive->morphTargetTangents.empty())
                        ? 1u
                        : 0u;
                draw.instanceOffset = instanceOffset;

                u32 drawIndex = static_cast<u32>(drawDataVec.size());
                drawDataVec.push_back(draw);

                MeshletDrawCallInfo dc{};
                dc.drawDataIndex = drawIndex;
                dc.meshletOffset = primitive->globalMeshletOffset;
                dc.meshletCount = primitive->meshletCount;
                dc.instanceOffset = instanceOffset;
                dc.instanceCount = static_cast<u32>(entityIDs.size());
                meshletDrawCalls.push_back(dc);

                vk::DrawMeshTasksIndirectCommandEXT drawCall{};
                // The key change is here: one command per meshlet-instance group
                drawCall.groupCountX = (primitive->meshletCount + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
                drawCall.groupCountY = static_cast<u32>(entityIDs.size());
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

    if(meshletDrawCalls.empty()) { return; }

    vk::DeviceSize drawDataSize = sizeof(DrawData) * drawDataVec.size();
    vk::DeviceSize instanceDataSize = sizeof(InstanceData) * instanceDataVec.size();
    vk::DeviceSize meshDataSize = sizeof(MeshletDrawCallInfo) * meshletDrawCalls.size();
    vk::DeviceSize meshDataIndirectSize = sizeof(vk::DrawMeshTasksIndirectCommandEXT) * meshletDrawCalls.size();

    auto ensureBuffer = [&](std::unique_ptr<Buffer>& buf, vk::DeviceSize needed, const char* name) {
        if(!buf || buf->GetBuffer() == VK_NULL_HANDLE || buf->GetBufferSize() < needed)
        {
            buf.reset();
            buf = std::make_unique<Buffer>(m_logicalDevice, needed, 1,
                                           vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                           vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY, 1, name);
        }
    };

    auto& drawDataBuffer = depthOnly ? m_depthDrawDataBuffers[renderData.frameIndex] : m_drawDataBuffers[renderData.frameIndex];
    auto& instanceDataBuffer = depthOnly ? m_depthInstanceBuffers[renderData.frameIndex] : m_drawInstanceBuffers[renderData.frameIndex];
    auto& meshDataBuffer = depthOnly ? m_depthMeshIndirectDrawBuffers[renderData.frameIndex] : m_meshDataDrawBuffers[renderData.frameIndex];
    auto& meshDataIndirectBuffer =
        depthOnly ? m_depthMeshIndirectDrawBuffers[renderData.frameIndex] : m_meshIndirectDrawBuffers[renderData.frameIndex];

    ensureBuffer(drawDataBuffer, drawDataSize, "draw data buffer");
    ensureBuffer(instanceDataBuffer, instanceDataSize, "instance data buffer");
    ensureBuffer(meshDataBuffer, meshDataSize, "mesh data buffer");
    ensureBuffer(meshDataIndirectBuffer, meshDataIndirectSize, "mesh data indirect buffer");

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
    if(meshDataSize > 0) { uploadToDeviceBuffer(*meshDataBuffer, meshletDrawCalls.data(), meshDataSize, "meshdata-staging"); }
    if(meshDataIndirectSize > 0)
    {
        uploadToDeviceBuffer(*meshDataIndirectBuffer, meshletDrawCalls.data(), meshDataIndirectSize, "meshdata-indirect-staging");
    }

    DescriptorPool* poolToUse = depthOnly ? m_depthPool[renderData.frameIndex].get() : m_pool[renderData.frameIndex].get();
    poolToUse->ResetPool();

    vk::DescriptorSet setToUse = depthOnly ? m_depthSet[renderData.frameIndex] : m_set[renderData.frameIndex];
    DescriptorWriter  writer{*m_layout, poolToUse};
    auto              drawInfo = drawDataBuffer->DescriptorInfo();
    auto              instInfo = instanceDataBuffer->DescriptorInfo();
    auto              meshInfo = meshDataBuffer->DescriptorInfo();
    writer.WriteBuffer(0, &drawInfo).WriteBuffer(1, &instInfo).WriteBuffer(2, &meshInfo);
    writer.Build(setToUse);

    if(depthOnly) { m_depthOnlyPipeline->Bind(renderData.commandBuffer); }
    else { m_opaqueGeometryPipeline->Bind(renderData.commandBuffer); }

    m_resourceManager.BindGlobalDescriptorSets(renderData.commandBuffer, m_pipelineLayout);

    if(!renderData.uboSets.empty())
    {
        renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                    static_cast<u32>(Globals::ModelDescriptorIndices::Camera),
                                                    static_cast<u32>(renderData.uboSets.size()), renderData.uboSets.data(), 0, nullptr);
    }

    std::vector<vk::DescriptorSet> sets{setToUse};
    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                static_cast<u32>(Globals::ModelDescriptorIndices::Model) + 1, (uint32_t)sets.size(), sets.data(), 0,
                                                nullptr);

    m_logicalDevice.RecordDrawMeshIndirect(
        renderData.commandBuffer, meshDataIndirectBuffer->GetBuffer(),
        0,                                          // offset
        static_cast<u32>(drawCalls.size()),         // The number of commands in the buffer
        sizeof(vk::DrawMeshTasksIndirectCommandEXT) // Stride between commands

        // renderData.commandBuffer.drawMeshTasksIndirectEXT(meshDataIndirectBuffer->GetBuffer(),
        //                                                   0,                                          // offset
        //                                                   static_cast<u32>(drawCalls.size()),         // The number of commands in the buffer
        //                                                   sizeof(vk::DrawMeshTasksIndirectCommandEXT) // Stride between commands
    );
}

void SimpleRenderSystem::RenderObjectsToData(RenderData& renderData, std::vector<DrawData>& opaqueDrawData,
                                             std::vector<vk::DrawIndexedIndirectCommand>& opaqueCommands,
                                             std::vector<DrawData>&                       transparentDrawData,
                                             std::vector<vk::DrawIndexedIndirectCommand>& transparentCommands,
                                             std::vector<InstanceData>&                   instanceData)
{
    u32 instanceOffset = 0;

    std::unordered_map<u32, std::vector<u32>> staticModelIDToEntityIDs;

    for(const auto& entity: *renderData.visibleEntities)
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

void SimpleRenderSystem::RenderObjectsTraditional(RenderData& renderData, const b8& depthOnly)
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
    m_verticesDrawn = opaqueCommands.size();

    if(indirectCommandsSize == 0) { return; }

    DescriptorPool* poolToUse = depthOnly ? m_depthPool[renderData.frameIndex].get() : m_pool[renderData.frameIndex].get();

    auto& indirectBufferToUse = depthOnly ? m_depthIndirectDrawBuffers[renderData.frameIndex] : m_indirectDrawBuffers[renderData.frameIndex];
    auto& drawDataBufferToUse = depthOnly ? m_depthDrawDataBuffers[renderData.frameIndex] : m_drawDataBuffers[renderData.frameIndex];
    auto& instanceBufferToUse = depthOnly ? m_depthInstanceBuffers[renderData.frameIndex] : m_drawInstanceBuffers[renderData.frameIndex];

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

    if(depthOnly) { m_depthOnlyPipeline->Bind(renderData.commandBuffer); }
    else { m_opaqueGeometryPipeline->Bind(renderData.commandBuffer); }

    vk::DescriptorSet debugSet;
    auto              debugBufferInfo = m_debugBuffer->DescriptorInfo();

    DescriptorWriter debugWriter{*m_resourceManager.GetModelDescriptors().debugLayout, m_resourceManager.GetDescriptorPools().debugPool.get()};

    debugWriter.WriteBuffer(0, &debugBufferInfo);

    if(debugSet == VK_NULL_HANDLE) { debugWriter.Build(debugSet); }
    else { debugWriter.Overwrite(debugSet); }

    poolToUse->ResetPool();

    vk::DescriptorSet setToUse = depthOnly ? m_depthSet[renderData.frameIndex] : m_set[renderData.frameIndex];

    auto buferInfo = drawDataBufferToUse->DescriptorInfo();

    auto instBufInfo = instanceBufferToUse->DescriptorInfo();

    DescriptorWriter writer{*m_layout, poolToUse};

    writer.WriteBuffer(0, &buferInfo).WriteBuffer(1, &instBufInfo);

    writer.Build(setToUse);

    auto globalIndexBuffer = &m_resourceManager.GetModelIndexBuffer();

    renderData.commandBuffer.bindIndexBuffer(globalIndexBuffer->GetBuffer(), 0, vk::IndexType::eUint32);

    m_resourceManager.BindGlobalDescriptorSets(renderData.commandBuffer, m_pipelineLayout);

    if(!renderData.uboSets.empty())
    {
        renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                    static_cast<u32>(Globals::ModelDescriptorIndices::Camera),
                                                    static_cast<u32>(renderData.uboSets.size()), renderData.uboSets.data(), 0, nullptr);
    }
    std::vector<vk::DescriptorSet> sets{setToUse};

    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                static_cast<u32>(Globals::ModelDescriptorIndices::Model) + 1, sets.size(), sets.data(), 0, nullptr);

    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                static_cast<u32>(Globals::ModelDescriptorIndices::Debug), 1, &debugSet, 0, nullptr);

    renderData.commandBuffer.drawIndexedIndirect(indirectBufferToUse->GetBuffer(), 0, opaqueCommands.size(),
                                                 sizeof(vk::DrawIndexedIndirectCommand));

    // m_transparentGeometryPipeline->Bind(renderData.commandBuffer);
    //
    // renderData.commandBuffer.drawIndexedIndirect(indirectBufferToUse->GetBuffer(), 0, opaqueCommands.size(),
    //                                              sizeof(vk::DrawIndexedIndirectCommand));
}

} // namespace Humongous
