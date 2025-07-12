#include "render_systems/simple_render_system.hpp"
#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include "model_instance.hpp"
#include "resource_manager.hpp"
#include "scene_handler.hpp"
#include "swapchain.hpp"

namespace Humongous
{
SimpleRenderSystem::SimpleRenderSystem(LogicalDevice& logicalDevice, const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
                                       const ShaderSet& shaderSet)
    : m_logicalDevice{logicalDevice}, m_pipelineLayout{VK_NULL_HANDLE}
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

    for(n32 i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_indirectDrawBuffers[i] = std::make_unique<Buffer>();
        m_drawInstanceBuffers[i] = std::make_unique<Buffer>();
        m_depthIndirectDrawBuffers[i] = std::make_unique<Buffer>();
        m_drawDataBuffers[i] = std::make_unique<Buffer>();
        m_depthInstanceBuffers[i] = std::make_unique<Buffer>();
        m_depthDrawDataBuffers[i] = std::make_unique<Buffer>();
    }

    HGINFO("Created simple render system");
}

SimpleRenderSystem::~SimpleRenderSystem()
{
    HGINFO("Destroying simple render system...");
    m_logicalDevice.GetVkDevice().destroyPipelineLayout(m_pipelineLayout, nullptr);
    m_geometryPipeline.reset();
    m_depthOnlyPipeline.reset();
    m_debugBuffer.reset();

    for(n32 i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_indirectDrawBuffers[i].reset();
        m_drawInstanceBuffers[i].reset();
        m_depthIndirectDrawBuffers[i].reset();
        m_drawDataBuffers[i].reset();
        m_depthInstanceBuffers[i].reset();
        m_depthDrawDataBuffers[i].reset();
    }

    HGINFO("Destroyed Simple render system");
}

void SimpleRenderSystem::AllocateDescriptorSet()
{
    m_pool.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthPool.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_set.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_depthSet.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    DescriptorSetLayout::Builder layoutBuilder{m_logicalDevice};
    layoutBuilder.AddBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex, 1);
    layoutBuilder.AddBinding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex, 1);
    m_layout = layoutBuilder.Build();

    DescriptorPool::Builder poolBuilder{m_logicalDevice};
    poolBuilder.AddPoolSize(vk::DescriptorType::eStorageBuffer, 3);
    poolBuilder.SetMaxSets(3);

    for(n32 i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
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
    auto descriptorSetLayouts = ResourceManager::GetLayoutVector();
    descriptorSetLayouts.insert(descriptorSetLayouts.begin(), layouts.begin(), layouts.end());

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<n32>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

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

    configInfo.multisampleInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
    configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
    configInfo.multisampleInfo.minSampleShading = 1.0;

    configInfo.vertShaderPath = shaderSet.vertShaderPath;
    configInfo.fragShaderPath = shaderSet.fragShaderPath;

    configInfo.colorBlendAttachment.blendEnable = false;

    configInfo.colorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat;

    configInfo.colorBlendAttachments.clear();
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);
    configInfo.colorBlendAttachments.push_back(configInfo.colorBlendAttachment);

    configInfo.colorAttachmentFormats.clear();
    configInfo.colorAttachmentFormats.push_back(configInfo.colorAttachmentFormat);
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

    vk::StencilOpState stencilState{};
    stencilState.compareOp = vk::CompareOp::eAlways;
    stencilState.passOp = vk::StencilOp::eReplace;
    stencilState.reference = static_cast<n32>(Globals::StencilMasks::Model);
    stencilState.compareMask = 0xFF;
    stencilState.writeMask = 0xFF;

    configInfo.depthStencilInfo.front = stencilState;
    configInfo.depthStencilInfo.back = stencilState;
    configInfo.renderingInfo.stencilAttachmentFormat = vk::Format::eD32SfloatS8Uint;

    m_geometryPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);

    HGINFO("Created geometry pipeline, now creating depth pipeline...");

    ShaderSet depthSet{Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "depth.vert"),
                       Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "nothing.frag")};

    configInfo.vertShaderPath = depthSet.vertShaderPath;
    configInfo.fragShaderPath = depthSet.fragShaderPath;
    configInfo.rasterizationInfo.depthBiasEnable = false;
    configInfo.rasterizationInfo.depthBiasClamp = 0.0f;
    configInfo.rasterizationInfo.depthBiasConstantFactor = 0.01f;
    configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;

    configInfo.colorAttachmentFormat = vk::Format::eUndefined;
    configInfo.colorAttachmentFormats.clear();
    configInfo.colorBlendAttachments.clear();
    configInfo.colorBlendInfo.attachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.colorAttachmentCount = configInfo.colorBlendAttachments.size();
    configInfo.renderingInfo.pColorAttachmentFormats = configInfo.colorAttachmentFormats.data();
    configInfo.colorBlendInfo.pAttachments = configInfo.colorBlendAttachments.data();

    configInfo.depthStencilInfo.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
    configInfo.depthStencilInfo.depthWriteEnable = true;
    configInfo.depthStencilInfo.stencilTestEnable = false;

    configInfo.renderingInfo.stencilAttachmentFormat = vk::Format::eUndefined;

    configInfo.depthStencilInfo.front = vk::StencilOpState{};
    configInfo.depthStencilInfo.front = vk::StencilOpState{};

    m_depthOnlyPipeline = std::make_unique<RenderPipeline>(m_logicalDevice, configInfo);
    HGINFO("Created depth pipeline");

    m_debugBuffer = std::make_unique<Buffer>(&m_logicalDevice, 1, sizeof(n32), vk::BufferUsageFlagBits::eStorageBuffer,
                                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                             VMA_MEMORY_USAGE_AUTO, 1, "Render system debug buffer");
    m_debugBuffer->Map();
    m_debugBuffer->WriteToBuffer(&m_verticesDrawn);
    m_debugBuffer->Flush();
    m_debugBuffer->UnMap();
}

struct alignas(16) DrawData
{
    n32 materialID{0};
    n32 localNodeIndex{0};
    n32 isSkinned{0};
    n32 isMorphed{0};
    n32 instanceOffset;
};

struct alignas(16) InstanceData
{
    glm::mat4 modelMatrix;
    n32       modelID;
    n32       globalNodeIndex;
    n32       jointMatrixStart;
    n32       morphTargetStart;
};

void SimpleRenderSystem::RenderObjects(RenderData& renderData, const bool& depthOnly)
{
    std::vector<DrawData>                       drawDataVec;
    std::vector<InstanceData>                   instanceDataVec;
    std::vector<vk::DrawIndexedIndirectCommand> commands;
    n32                                         instanceOffset = 0;

    std::unordered_map<n32, std::vector<n32>> staticModelIDToEntityIDs;

    for(const auto& entity: *renderData.visibleEntities)
    {
        auto modelComp = SceneHandler::GetWorld()->GetComponent<ModelComponent>(entity.id);
        if(modelComp && modelComp->instance && modelComp->instance->GetModel())
        {
            staticModelIDToEntityIDs[modelComp->instance->GetModel()->GetHandle()].push_back(entity.id);
        }
    }

    drawDataVec.reserve(staticModelIDToEntityIDs.size() * 4);
    instanceDataVec.reserve(renderData.visibleEntities->size());
    commands.reserve(staticModelIDToEntityIDs.size() * 4);

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
                cmd.instanceCount = static_cast<n32>(entityIDs.size());
                cmd.firstIndex = primitive->globalFirstIndex;
                cmd.vertexOffset = primitive->vertexOffset;
                cmd.firstInstance = 0;
                commands.push_back(cmd);

                DrawData draw{};
                draw.materialID = primitive->material.index;
                draw.localNodeIndex = primitive->owner->index;
                draw.isSkinned = staticModel->HasSkins();
                draw.isMorphed =
                    !primitive->morphTargetPositions.empty() || !primitive->morphTargetNormals.empty() || !primitive->morphTargetTangents.empty();
                draw.instanceOffset = instanceOffset;
                drawDataVec.push_back(draw);
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
            instance.globalNodeIndex = ResourceManager::GetModelHandleToMatrixStart(currentModelInstance->GetInstanceID());

            if(drawDataVec.back().isSkinned)
            {
                instance.jointMatrixStart = ResourceManager::Get().m_modelHandleToJointStart[currentModelInstance->GetInstanceID()].first;
            }
            if(drawDataVec.back().isMorphed)
            {
                instance.morphTargetStart = ResourceManager::Get().m_modelHandleToMorphStart[currentModelInstance->GetInstanceID()].first;
            }

            instanceDataVec.push_back(instance);
        }
        instanceOffset += static_cast<n32>(entityIDs.size());
    }

    vk::DeviceSize indirectCommandsSize = sizeof(vk::DrawIndexedIndirectCommand) * commands.size();
    vk::DeviceSize drawDataSize = sizeof(DrawData) * drawDataVec.size();
    vk::DeviceSize instanceDataSize = sizeof(InstanceData) * instanceDataVec.size();
    m_verticesDrawn = commands.size();

    if(indirectCommandsSize == 0) { return; }

    DescriptorPool* poolToUse = depthOnly ? m_depthPool[renderData.frameIndex].get() : m_pool[renderData.frameIndex].get();

    Buffer* indirectBufferToUse = nullptr;
    Buffer* drawDataBufferToUse = nullptr;
    Buffer* instanceBufferToUse = nullptr;

    if(depthOnly)
    {
        if(m_depthIndirectDrawBuffers[renderData.frameIndex]->GetBuffer() != VK_NULL_HANDLE)
        {
            m_depthIndirectDrawBuffers[renderData.frameIndex].reset();
            m_depthIndirectDrawBuffers[renderData.frameIndex] = std::make_unique<Buffer>();
        }
        if(m_depthDrawDataBuffers[renderData.frameIndex]->GetBuffer() != VK_NULL_HANDLE)
        {
            m_depthDrawDataBuffers[renderData.frameIndex].reset();
            m_depthDrawDataBuffers[renderData.frameIndex] = std::make_unique<Buffer>();
        }
        if(m_depthInstanceBuffers[renderData.frameIndex]->GetBuffer() != VK_NULL_HANDLE)
        {

            m_depthInstanceBuffers[renderData.frameIndex].reset();
            m_depthInstanceBuffers[renderData.frameIndex] = std::make_unique<Buffer>();
        }
        indirectBufferToUse = m_depthIndirectDrawBuffers[renderData.frameIndex].get();
        drawDataBufferToUse = m_depthDrawDataBuffers[renderData.frameIndex].get();
        instanceBufferToUse = m_depthInstanceBuffers[renderData.frameIndex].get();
    }
    else
    {
        if(m_indirectDrawBuffers[renderData.frameIndex]->GetBuffer() != VK_NULL_HANDLE)
        {
            m_indirectDrawBuffers[renderData.frameIndex].reset();
            m_indirectDrawBuffers[renderData.frameIndex] = std::make_unique<Buffer>();
        }
        if(m_drawDataBuffers[renderData.frameIndex]->GetBuffer() != VK_NULL_HANDLE)
        {
            m_drawDataBuffers[renderData.frameIndex].reset();
            m_drawDataBuffers[renderData.frameIndex] = std::make_unique<Buffer>();
        }
        if(m_drawInstanceBuffers[renderData.frameIndex]->GetBuffer() != VK_NULL_HANDLE)
        {
            m_drawInstanceBuffers[renderData.frameIndex].reset();
            m_drawInstanceBuffers[renderData.frameIndex] = std::make_unique<Buffer>();
        }
        indirectBufferToUse = m_indirectDrawBuffers[renderData.frameIndex].get();
        drawDataBufferToUse = m_drawDataBuffers[renderData.frameIndex].get();
        instanceBufferToUse = m_drawInstanceBuffers[renderData.frameIndex].get();
    }

    // uploading data
    // indirect buffer
    {
        Buffer stagingBuffer{&m_logicalDevice,
                             indirectCommandsSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             4,
                             "indirect staging buffer"};

        indirectBufferToUse->Init(&m_logicalDevice, indirectCommandsSize, 1,
                                  vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                  vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 1, "draw indiret commmand buffer");

        stagingBuffer.Map();

        stagingBuffer.WriteToBuffer((void*)commands.data(), commands.size() * sizeof(vk::DrawIndexedIndirectCommand));

        stagingBuffer.Flush();

        stagingBuffer.UnMap();

        Buffer::CopyBuffer(m_logicalDevice, stagingBuffer, *indirectBufferToUse, indirectCommandsSize);
    }
    // draw data buffer
    {
        Buffer stagingBuffer{&m_logicalDevice,
                             drawDataSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             1,
                             "draw data staging buffer"};

        drawDataBufferToUse->Init(&m_logicalDevice, drawDataSize, 1,
                                  vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                  vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 1, "draw data buffer");

        stagingBuffer.Map();

        stagingBuffer.WriteToBuffer((void*)drawDataVec.data(), drawDataVec.size() * sizeof(DrawData));

        stagingBuffer.Flush();

        stagingBuffer.UnMap();

        Buffer::CopyBuffer(m_logicalDevice, stagingBuffer, *drawDataBufferToUse, drawDataVec.size() * sizeof(DrawData));
    }
    // instance data buffer
    {
        Buffer stagingBuffer{&m_logicalDevice,
                             instanceDataSize,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             1,
                             "instance data staging buffer"};

        instanceBufferToUse->Init(&m_logicalDevice, instanceDataSize, 1,
                                  vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                  vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 1, "instance data buffer");

        stagingBuffer.Map();

        stagingBuffer.WriteToBuffer((void*)instanceDataVec.data(), instanceDataVec.size() * sizeof(InstanceData));

        stagingBuffer.Flush();

        stagingBuffer.UnMap();

        Buffer::CopyBuffer(m_logicalDevice, stagingBuffer, *instanceBufferToUse, instanceDataSize);
    }

    if(depthOnly) { m_depthOnlyPipeline->Bind(renderData.commandBuffer); }
    else { m_geometryPipeline->Bind(renderData.commandBuffer); }

    vk::DescriptorSet debugSet;
    auto              debugBufferInfo = m_debugBuffer->DescriptorInfo();
    DescriptorWriter  debugWriter{*ResourceManager::GetModelDescriptors().debugLayout, ResourceManager::GetDescriptorPools().debugPool.get()};
    debugWriter.WriteBuffer(0, &debugBufferInfo);
    if(debugSet == VK_NULL_HANDLE) { debugWriter.Build(debugSet); }
    else { debugWriter.Overwrite(debugSet); }

    poolToUse->ResetPool();

    vk::DescriptorSet setToUse = depthOnly ? m_depthSet[renderData.frameIndex] : m_set[renderData.frameIndex];

    auto             buferInfo = drawDataBufferToUse->DescriptorInfo();
    auto             instBufInfo = instanceBufferToUse->DescriptorInfo();
    DescriptorWriter writer{*m_layout, poolToUse};
    writer.WriteBuffer(0, &buferInfo).WriteBuffer(1, &instBufInfo);
    writer.Build(setToUse);

    auto globalIndexBuffer = &ResourceManager::GetModelIndexBuffer();

    renderData.commandBuffer.bindIndexBuffer(globalIndexBuffer->GetBuffer(), 0, vk::IndexType::eUint32);

    ResourceManager::BindGlobalDescriptorSets(renderData.commandBuffer, m_pipelineLayout);

    if(!renderData.uboSets.empty())
    {
        renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                    static_cast<n32>(Globals::ModelDescriptorIndices::Camera),
                                                    static_cast<n32>(renderData.uboSets.size()), renderData.uboSets.data(), 0, nullptr);
    }
    std::vector<vk::DescriptorSet> sets{setToUse};

    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                static_cast<n32>(Globals::ModelDescriptorIndices::Model) + 1, sets.size(), sets.data(), 0, nullptr);

    renderData.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
                                                static_cast<n32>(Globals::ModelDescriptorIndices::Debug), 1, &debugSet, 0, nullptr);

    renderData.commandBuffer.drawIndexedIndirect(indirectBufferToUse->GetBuffer(), 0, commands.size(), sizeof(vk::DrawIndexedIndirectCommand));
}

} // namespace Humongous
