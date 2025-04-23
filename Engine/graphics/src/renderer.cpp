#include "abstractions/descriptor_writer.hpp"
#include "asset_manager.hpp"
#include "extra.hpp"
#include "gameobject.hpp"
#include "images.hpp"
#include "logger.hpp"
#include "render_systems/simple_render_system.hpp"
#include <array>
#include <renderer.hpp>
#include <vulkan/vk_enum_string_helper.h>

namespace Humongous
{
Renderer::Renderer(Window& window, LogicalDevice& logicalDevice, PhysicalDevice& physicalDevice, VmaAllocator allocator, VkFormat drawFormat,
                   VkFormat depthFormat)
    : m_window{window}, m_logicalDevice{logicalDevice}, m_physicalDevice{physicalDevice}, m_allocator{allocator}
{
    m_drawImage.imageFormat = drawFormat;
    m_depthImage.imageFormat = depthFormat;

    RecreateSwapChain();
    CreateCommandPool();
    AllocateCommandBuffers();
    InitSyncStructures();
    CreateComputePipeline();
}

Renderer::~Renderer()
{
    HGINFO("Destroying renderer...");
    if(m_commandPool) { vkDestroyCommandPool(m_logicalDevice.GetVkDevice(), m_commandPool, nullptr); }

    for(Frame& frame: m_frames)
    {
        vkDestroySemaphore(m_logicalDevice.GetVkDevice(), frame.imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(m_logicalDevice.GetVkDevice(), frame.renderFinishedSemaphore, nullptr);
        vkDestroyFence(m_logicalDevice.GetVkDevice(), frame.inFlightFence, nullptr);
    }

    if(m_drawImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_drawImage.image, m_drawImage.allocation); }
    if(m_depthImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), m_drawImage.imageView, nullptr); }
    if(m_depthImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation); }
    if(m_depthImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), m_depthImage.imageView, nullptr); }

    vkDestroyPipeline(m_logicalDevice.GetVkDevice(), m_computePipeline, nullptr);
    vkDestroyPipelineLayout(m_logicalDevice.GetVkDevice(), m_computePipelineLayout, nullptr);

    m_computeLayout.reset();
    m_computePool.reset();

    vkDestroySampler(m_logicalDevice.GetVkDevice(), m_depthImageSampler, nullptr);

    m_swapChain.reset();
    HGINFO("Destroyed renderer");
}

void Renderer::RecreateSwapChain()
{
    HGINFO("Recreating swap chain...");
    m_logicalDevice.GetVkDevice().waitIdle();

    auto extent = m_window.GetExtent();
    while(extent.width == 0 || extent.height == 0) { extent = m_window.GetExtent(); }

    if(m_swapChain == nullptr) { m_swapChain = std::make_unique<SwapChain>(m_window, m_physicalDevice, m_logicalDevice); }
    else
    {
        std::shared_ptr<SwapChain> oldSwapChain = std::move(m_swapChain);
        m_swapChain = std::make_unique<SwapChain>(m_window, m_physicalDevice, m_logicalDevice, std::move(m_swapChain));
    }
    // recreate the image views

    HGINFO("Recreated swap chain");

    InitImagesAndViews();
    InitDepthImage();
}

void Renderer::InitImagesAndViews()
{
    if(m_drawImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), m_drawImage.imageView, nullptr); }
    if(m_drawImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_drawImage.image, m_drawImage.allocation); }

    HGINFO("Creating draw image and view...");

    VkExtent3D drawImageExtent = {m_swapChain->GetExtent().width, m_swapChain->GetExtent().height, 1};

    // m_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice, .allocatedImage = m_drawImage};
    imgCI.layerCount = 1;
    imgCI.flags = 0;
    imgCI.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    imgCI.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    imgCI.height = m_drawImage.imageExtent.height;
    imgCI.width = m_drawImage.imageExtent.width;
    imgCI.mipLevels = 1;
    imgCI.usage = drawImageUsages;
    imgCI.layerCount = 1;
    imgCI.format = m_drawImage.imageFormat == VK_FORMAT_UNDEFINED ? VK_FORMAT_R16G16B16A16_SFLOAT : m_drawImage.imageFormat;
    imgCI.imagePool = VK_NULL_HANDLE;
    imgCI.samples = VK_SAMPLE_COUNT_1_BIT;

    Utils::CreateAllocatedImage(imgCI);

    HGINFO("Created draw image and view");
}

void Renderer::InitDepthImage()
{
    if(m_depthImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), m_depthImage.imageView, nullptr); }
    if(m_depthImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation); }

    HGINFO("Creating depth image and view...");

    VkExtent3D depthImageExtent = {m_swapChain->GetExtent().width, m_swapChain->GetExtent().height, 1};

    // hardcoding the draw format to 32 bit float
    // m_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_depthImage.imageExtent = depthImageExtent;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice, .allocatedImage = m_depthImage};
    imgCI.layerCount = 1;
    imgCI.flags = 0;
    imgCI.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    imgCI.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    imgCI.height = m_depthImage.imageExtent.height;
    imgCI.width = m_depthImage.imageExtent.width;
    imgCI.mipLevels = 1;
    imgCI.usage = depthImageUsages;
    imgCI.format = VK_FORMAT_D32_SFLOAT;
    imgCI.imagePool = VK_NULL_HANDLE;
    imgCI.samples = VK_SAMPLE_COUNT_1_BIT;

    auto cmd = m_logicalDevice.BeginSingleTimeCommands();

    Utils::CreateAllocatedImage(imgCI);

    Utils::ImageTransitionInfo postComputeDepthTransition{cmd,
                                                          VK_IMAGE_LAYOUT_UNDEFINED,
                                                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                          &m_logicalDevice,
                                                          m_depthImage.image,
                                                          VK_IMAGE_ASPECT_DEPTH_BIT};

    Utils::TransitionImageLayout(postComputeDepthTransition);

    m_logicalDevice.EndSingleTimeCommands(cmd);

    HGINFO("Created depth image and view");
}

void Renderer::CreateCommandPool()
{
    HGINFO("Creating command pool...");
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = m_physicalDevice.FindQueueFamilies(m_physicalDevice.GetVkPhysicalDevice()).graphicsFamily.value();

    if(m_logicalDevice.GetVkDevice().createCommandPool(&poolInfo, nullptr, &m_commandPool) != vk::Result::eSuccess)
    {
        HGERROR("Failed to create command pool");
    }

    HGINFO("Created command pool");
}

void Renderer::AllocateCommandBuffers()
{
    HGINFO("Allocating command buffers...");

    m_frames.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = static_cast<n32>(m_frames.size());

    for(Frame& frame: m_frames)
    {
        if(m_logicalDevice.GetVkDevice().allocateCommandBuffers(&allocInfo, &frame.commandBuffer) != vk::Result::eSuccess)
        {
            HGERROR("Failed to allocate command buffers");
        }
    }

    HGINFO("Allocated command buffers");
}

void Renderer::InitSyncStructures()
{
    HGINFO("Initializing synchronization structures...");

    // one fence to control when the gpu has finished rendering the frame
    // and 2 semaphores to synchronize rendering with swapchain
    vk::FenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    vk::SemaphoreCreateInfo semaphoreCreateInfo{};

    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
    {

        if(m_logicalDevice.GetVkDevice().createFence(&fenceCreateInfo, nullptr, &m_frames[i].inFlightFence) != vk::Result::eSuccess)
        {
            HGERROR("Failed to create fence");
        }

        if(m_logicalDevice.GetVkDevice().createSemaphore(&semaphoreCreateInfo, nullptr, &m_frames[i].imageAvailableSemaphore) !=
           vk::Result::eSuccess)
        {
            HGERROR("Failed to create image available semaphore");
        }

        if(m_logicalDevice.GetVkDevice().createSemaphore(&semaphoreCreateInfo, nullptr, &m_frames[i].renderFinishedSemaphore) !=
           vk::Result::eSuccess)
        {
            HGERROR("Failed to render finished semaphore");
        }
    }

    HGINFO("Initialized synchronization structures");
}

void Renderer::CreateComputePipeline()
{
    DescriptorPool::Builder builder{m_logicalDevice};
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5);
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5);
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5);
    builder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 5);
    builder.SetMaxSets(100);
    m_computePool = builder.Build();

    DescriptorSetLayout::Builder builder2{m_logicalDevice};
    builder2.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder2.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder2.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder2.addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder2.addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder2.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    m_computeLayout = builder2.Build();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0;
    samplerInfo.compareEnable = VK_FALSE;

    if(vkCreateSampler(m_logicalDevice.GetVkDevice(), &samplerInfo, nullptr, &m_depthImageSampler) != VK_SUCCESS)
    {
        HGERROR("Failed to create texture sampler");
    }

    auto layout = m_computeLayout->GetDescriptorSetLayout();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &layout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if(vkCreatePipelineLayout(m_logicalDevice.GetVkDevice(), &pipelineLayoutInfo, nullptr, &m_computePipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    auto compCode = Utils::ReadFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::SHADER, "occlusion.comp"));

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = compCode.size();
    createInfo.pCode = reinterpret_cast<const n32*>(compCode.data());

    VkShaderModule compModule;

    if(vkCreateShaderModule(m_logicalDevice.GetVkDevice(), &createInfo, nullptr, &compModule) != VK_SUCCESS)
    {
        HGERROR("Failed to create shader module!");
    }

    VkPipelineShaderStageCreateInfo why{};
    why.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    why.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    why.pName = "main";
    why.module = compModule;

    VkComputePipelineCreateInfo compInfo{};
    compInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compInfo.layout = m_computePipelineLayout;
    compInfo.stage = why;

    if(vkCreateComputePipelines(m_logicalDevice.GetVkDevice(), nullptr, 1, &compInfo, nullptr, &m_computePipeline) != VK_SUCCESS)
    {
        HGFATAL("Failed to create renderer compute pipeline!");
    }

    vkDestroyShaderModule(m_logicalDevice.GetVkDevice(), compModule, nullptr);
}

VkCommandBuffer Renderer::BeginFrame()
{
    vk::Result result = m_logicalDevice.GetVkDevice().waitForFences(1, &GetCurrentFrame().inFlightFence, vk::True, std::numeric_limits<n64>::max());
    if(result != vk::Result::eSuccess) { HGINFO("Failed to wait for fences: ", string_VkResult(static_cast<VkResult>(result))); }

    result = m_logicalDevice.GetVkDevice().resetFences(1, &GetCurrentFrame().inFlightFence);
    if(result != vk::Result::eSuccess) { HGINFO("Failed to reset fences: ", string_VkResult(static_cast<VkResult>(result))); }

    result = m_logicalDevice.GetVkDevice().acquireNextImageKHR(m_swapChain->GetSwapChain(), 1000000000, GetCurrentFrame().imageAvailableSemaphore,
                                                               VK_NULL_HANDLE, &m_currentImageIndex);
    if(result != vk::Result::eSuccess) { HGINFO("Failed to acquire swapchain image: ", string_VkResult(static_cast<VkResult>(result))); }

    if(result == vk::Result::eErrorOutOfDateKHR)
    {
        RecreateSwapChain();
        return nullptr;
    }

    if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) { HGERROR("failed to acquire swap chain image!"); }

    vk::CommandBuffer cmd = GetCurrentFrame().commandBuffer;
    cmd.reset();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

    if(cmd.begin(&beginInfo) != vk::Result::eSuccess) { HGERROR("Failed to begin recording command buffer"); }

    return static_cast<VkCommandBuffer>(cmd);
}

void Renderer::EndFrame()
{
    if(vkEndCommandBuffer(GetCurrentFrame().commandBuffer) != VK_SUCCESS) { HGERROR("Failed to record command buffer"); }

    vk::CommandBufferSubmitInfo cmdInfo{};
    cmdInfo.deviceMask = 0;
    cmdInfo.setCommandBuffer(GetCurrentFrame().commandBuffer);

    vk::SemaphoreSubmitInfo waitInfo{};
    waitInfo.semaphore = GetCurrentFrame().imageAvailableSemaphore;
    waitInfo.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;

    vk::SemaphoreSubmitInfo signalInfo{};
    signalInfo.semaphore = GetCurrentFrame().renderFinishedSemaphore;
    signalInfo.stageMask = vk::PipelineStageFlagBits2::eAllGraphics;

    vk::SubmitInfo2 submit{};
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &waitInfo;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signalInfo;

    vk::Result result = m_logicalDevice.GetGraphicsQueue().submit2(1, &submit, GetCurrentFrame().inFlightFence);

    if(result != vk::Result::eSuccess) { HGERROR("Failed to submit command buffer"); }

    auto s = m_swapChain->GetSwapChain();

    vk::PresentInfoKHR presentInfo{};
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &s;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderFinishedSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &m_currentImageIndex;

    result = m_logicalDevice.GetPresentQueue().presentKHR(&presentInfo);
    if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_window.WasWindowResized())
    {
        m_window.ResetWindowResizedFlag();
        RecreateSwapChain();
    }

    else if(result != vk::Result::eSuccess) { HGERROR("failed to present swap chain image"); }

    m_currentFrameIndex = (m_currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::BeginRendering(VkCommandBuffer cmd)
{
    m_drawImageExtent.width = m_drawImage.imageExtent.width;
    m_drawImageExtent.height = m_drawImage.imageExtent.height;
    m_depthImageExtent.width = m_depthImage.imageExtent.width;
    m_depthImageExtent.height = m_depthImage.imageExtent.height;

    Utils::ImageTransitionInfo transInfo{};
    transInfo.image = m_drawImage.image;
    transInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transInfo.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    transInfo.cmd = cmd;

    Utils::TransitionImageLayout(transInfo);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.3f, 0.3f, 0.3f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_drawImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.clearValue = clearValues[0];

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_depthImage.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue = clearValues[1];

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    renderingInfo.flags = 0;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_drawImageExtent.width);
    viewport.height = static_cast<float>(m_drawImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = m_drawImageExtent.width;
    scissor.extent.height = m_drawImageExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Renderer::EndRendering(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);

    Utils::ImageTransitionInfo drawInfo{};
    drawInfo.image = m_drawImage.image;
    drawInfo.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    drawInfo.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    drawInfo.cmd = cmd;

    Utils::ImageTransitionInfo swapInfo{};
    swapInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    swapInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapInfo.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapInfo.cmd = cmd;

    Utils::TransitionImageLayout(drawInfo);
    Utils::TransitionImageLayout(swapInfo);

    Utils::CopyImageToImage(cmd, m_drawImage.image, m_swapChain->GetImages()[m_currentImageIndex], m_drawImageExtent, m_swapChain->GetExtent());

    Utils::ImageTransitionInfo presentInfo{};
    presentInfo.image = m_swapChain->GetImages()[m_currentImageIndex];
    presentInfo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    presentInfo.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentInfo.cmd = cmd;

    Utils::TransitionImageLayout(presentInfo);
}

void Renderer::BeginDepthPrePass(VkCommandBuffer cmd)
{
    m_drawImageExtent.width = m_drawImage.imageExtent.width;
    m_drawImageExtent.height = m_drawImage.imageExtent.height;
    m_depthImageExtent.width = m_depthImage.imageExtent.width;
    m_depthImageExtent.height = m_depthImage.imageExtent.height;

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_depthImage.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue = clearValue;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pColorAttachments = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    renderingInfo.flags = 0;

    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_drawImageExtent.width);
    viewport.height = static_cast<float>(m_drawImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = m_drawImageExtent.width;
    scissor.extent.height = m_drawImageExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Renderer::EndDepthPrePass(VkCommandBuffer cmd) { vkCmdEndRendering(cmd); }

struct alignas(16) RendererData
{
    glm::vec2 screenSize;
    float     padding[2];
};

void Renderer::DoGPUOcclusionCulling(VkCommandBuffer cmd, RenderData& data, const Camera& cam)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);

    std::vector<BoundingBox> boundingBoxes;

    for(auto& [id, object]: data.gameObjects) { boundingBoxes.push_back(object->GetBoundingBox()); }
    if(boundingBoxes.data() == nullptr) { return; }

    auto waitCmd = m_logicalDevice.BeginSingleTimeCommands();
    WaitForCompute(waitCmd);
    m_logicalDevice.EndSingleTimeCommands(waitCmd);

    m_boundingBoxBuffer.reset();
    m_visibilityResults.reset();
    m_rendererDataBuffer.reset();

    m_boundingBoxBuffer = std::make_unique<Buffer>();
    m_visibilityResults = std::make_unique<Buffer>();
    m_rendererDataBuffer = std::make_unique<Buffer>();

    auto alignment = m_logicalDevice.GetPhysicalDevice().GetProperties().properties.limits.minStorageBufferOffsetAlignment;

    auto   bbSize = boundingBoxes.size();
    size_t bufferSize = bbSize * sizeof(BoundingBox);
    size_t alignedSize = std::max(bufferSize, alignment) & ~(alignment - 1);

    m_boundingBoxBuffer->Init(&m_logicalDevice, bufferSize, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_AUTO);

    m_visibilityResults->Init(&m_logicalDevice, bbSize * sizeof(b32), 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_AUTO);

    m_rendererDataBuffer->Init(&m_logicalDevice, sizeof(RendererData), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_AUTO);

    VkMemoryBarrier2 memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    Utils::ImageTransitionInfo preComputeReadTransition{
        cmd,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        &m_logicalDevice,
        m_depthImage.image,
        VK_IMAGE_ASPECT_DEPTH_BIT,
    };

    Utils::TransitionImageLayout(preComputeReadTransition);

    m_boundingBoxBuffer->Map();
    m_boundingBoxBuffer->WriteToBuffer((void*)boundingBoxes.data());
    m_boundingBoxBuffer->UnMap();

    RendererData renderData{{m_swapChain->GetExtent().width, m_swapChain->GetExtent().height}};
    m_rendererDataBuffer->Map();
    m_rendererDataBuffer->WriteToBuffer((void*)&renderData);
    m_rendererDataBuffer->UnMap();

    VkDescriptorImageInfo  depthInfo = {m_depthImageSampler, m_depthImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo boundingBoxInfo = m_boundingBoxBuffer->DescriptorInfo();
    VkDescriptorBufferInfo visiblityInfo = m_visibilityResults->DescriptorInfo();
    VkDescriptorBufferInfo projectionInfo = cam.GetCombinedDataBufferHandle(m_currentFrameIndex).DescriptorInfo();
    VkDescriptorBufferInfo rendererDataInfo = m_rendererDataBuffer->DescriptorInfo();

    if(m_computeSet == VK_NULL_HANDLE)
    {
        DescriptorWriter writer{*m_computeLayout, m_computePool.get()};

        writer.WriteImage(0, &depthInfo)
            .WriteBuffer(1, &boundingBoxInfo)
            .WriteBuffer(2, &visiblityInfo)
            .WriteBuffer(3, &projectionInfo)
            .WriteBuffer(4, &rendererDataInfo);
        writer.Build(m_computeSet);
    }
    else
    {
        DescriptorWriter writer{*m_computeLayout, m_computePool.get()};

        writer.WriteImage(0, &depthInfo)
            .WriteBuffer(1, &boundingBoxInfo)
            .WriteBuffer(2, &visiblityInfo)
            .WriteBuffer(3, &projectionInfo)
            .WriteBuffer(4, &rendererDataInfo);
        writer.Overwrite(m_computeSet);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeSet, 0, nullptr);

    vkCmdDispatch(cmd, (n32)(data.gameObjects.size() + 63) / 64, 1, 1);

    WaitForCompute(cmd);

    Utils::ImageTransitionInfo postComputeDepthTransition{cmd,
                                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                          &m_logicalDevice,
                                                          m_depthImage.image,
                                                          VK_IMAGE_ASPECT_DEPTH_BIT};

    Utils::TransitionImageLayout(postComputeDepthTransition);

    m_visibilityResults->Map();

    n32* visibilityResults = static_cast<n32*>(m_visibilityResults->GetMappedMemory());

    for(int i = 0; i < boundingBoxes.size(); i++)
    {
        if(!visibilityResults[i]) { data.gameObjects.erase(data.gameObjects.begin() + i); }
    }
    m_visibilityResults->UnMap();
}

void Renderer::WaitForCompute(VkCommandBuffer cmd)
{
    VkMemoryBarrier2 visibilityBarrier{};
    visibilityBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    visibilityBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    visibilityBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    visibilityBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    visibilityBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo visibilityDependencyInfo{};
    visibilityDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    visibilityDependencyInfo.memoryBarrierCount = 1;
    visibilityDependencyInfo.pMemoryBarriers = &visibilityBarrier;

    vkCmdPipelineBarrier2(cmd, &visibilityDependencyInfo);
}

} // namespace Humongous
