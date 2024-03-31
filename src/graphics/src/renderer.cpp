#include "allocator.hpp"
#include "images.hpp"
#include "logger.hpp"
#include <array>
#include <renderer.hpp>

namespace Humongous
{
Renderer::Renderer(Window& window, LogicalDevice& logicalDevice, PhysicalDevice& physicalDevice, VmaAllocator allocator)
    : m_window{window}, m_logicalDevice{logicalDevice}, m_physicalDevice{physicalDevice}, m_allocator{allocator}
{
    RecreateSwapChain();
    CreateCommandPool();
    AllocateCommandBuffers();
    InitSyncStructures();
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

    m_swapChain.reset();
    HGINFO("Destroyed renderer");
}

void Renderer::RecreateSwapChain()
{
    HGINFO("Recreating swap chain...");

    auto extent = m_window.GetExtent();
    while(extent.width == 0 || extent.height == 0)
    {
        extent = m_window.GetExtent();
        glfwWaitEvents();

        if(m_window.ShouldWindowClose()) { return; }
    }
    vkDeviceWaitIdle(m_logicalDevice.GetVkDevice());

    if(m_swapChain == nullptr) { m_swapChain = std::make_unique<SwapChain>(m_window, m_physicalDevice, m_logicalDevice); }
    else
    {
        std::shared_ptr<SwapChain> oldSwapChain = std::move(m_swapChain);
        m_swapChain = std::make_unique<SwapChain>(m_window, m_physicalDevice, m_logicalDevice, std::move(m_swapChain));

        if(!oldSwapChain->CompareSwapFormats(*m_swapChain.get())) { HGERROR("Swap chain image(or depth) format has changed"); }
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

    VkExtent3D drawImageExtent = {m_window.GetExtent().width, m_window.GetExtent().height, 1};

    m_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // VkImageCreateInfo imgInfo{};
    // imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    // imgInfo.imageType = VK_IMAGE_TYPE_2D;
    // imgInfo.format = m_drawImage.imageFormat;
    // imgInfo.extent = m_drawImage.imageExtent;
    // imgInfo.mipLevels = 1;
    // imgInfo.arrayLayers = 1;
    // imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    // imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    // imgInfo.usage = drawImageUsages;
    //
    // VmaAllocationCreateInfo imgAllocInfo{};
    // imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    // imgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //
    // vmaCreateImage(m_allocator, &imgInfo, &imgAllocInfo, &m_drawImage.image, &m_drawImage.allocation, nullptr);
    //
    // VkImageViewCreateInfo viewInfo{};
    // viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    // viewInfo.image = m_drawImage.image;
    // viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    // viewInfo.format = m_drawImage.imageFormat;
    // viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // viewInfo.subresourceRange.baseMipLevel = 0;
    // viewInfo.subresourceRange.levelCount = 1;
    // viewInfo.subresourceRange.baseArrayLayer = 0;
    // viewInfo.subresourceRange.layerCount = 1;
    //
    // if(vkCreateImageView(m_logicalDevice.GetVkDevice(), &viewInfo, nullptr, &m_drawImage.imageView) != VK_SUCCESS)
    // {
    //     HGERROR("Failed to create image view");
    // }

    Utils::AllocatedImageCreateInfo imgCI{.logicalDevice = m_logicalDevice, .allocatedImage = m_drawImage};
    imgCI.layerCount = 1;
    imgCI.flags = 0;
    imgCI.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    imgCI.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    imgCI.height = m_drawImage.imageExtent.height;
    imgCI.width = m_drawImage.imageExtent.width;
    imgCI.mipLevels = 0;
    imgCI.usage = drawImageUsages;
    imgCI.layerCount = 1;
    imgCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    Utils::CreateAllocatedImage(imgCI);

    HGINFO("Created draw image and view");
}

void Renderer::InitDepthImage()
{
    if(m_depthImage.imageView != VK_NULL_HANDLE) { vkDestroyImageView(m_logicalDevice.GetVkDevice(), m_depthImage.imageView, nullptr); }
    if(m_depthImage.image != VK_NULL_HANDLE) { vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation); }

    HGINFO("Creating depth image and view...");

    VkExtent3D drawImageExtent = {m_window.GetExtent().width, m_window.GetExtent().height, 1};

    // hardcoding the draw format to 32 bit float
    m_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_depthImage.imageExtent = drawImageExtent;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = m_depthImage.imageFormat;
    imgInfo.extent = m_depthImage.imageExtent;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = depthImageUsages;

    VmaAllocationCreateInfo imgAllocInfo{};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(m_allocator, &imgInfo, &imgAllocInfo, &m_depthImage.image, &m_depthImage.allocation, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_depthImage.imageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if(vkCreateImageView(m_logicalDevice.GetVkDevice(), &viewInfo, nullptr, &m_depthImage.imageView) != VK_SUCCESS)
    {
        HGERROR("Failed to create image view");
    }

    HGINFO("Created depth image and view");
}

void Renderer::CreateCommandPool()
{
    HGINFO("Creating command pool...");
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_physicalDevice.FindQueueFamilies(m_physicalDevice.GetVkPhysicalDevice()).graphicsFamily.value();

    if(vkCreateCommandPool(m_logicalDevice.GetVkDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        HGERROR("Failed to create command pool");
    }

    HGINFO("Created command pool");
}

void Renderer::AllocateCommandBuffers()
{
    HGINFO("Allocating command buffers...");

    m_frames.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<u32>(m_frames.size());

    for(Frame& frame: m_frames)
    {
        if(vkAllocateCommandBuffers(m_logicalDevice.GetVkDevice(), &allocInfo, &frame.commandBuffer) != VK_SUCCESS)
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
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.flags = 0;

    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
    {
        if(vkCreateFence(m_logicalDevice.GetVkDevice(), &fenceCreateInfo, nullptr, &m_frames[i].inFlightFence) != VK_SUCCESS)
        {
            HGERROR("Failed to create fence");
        }

        if(vkCreateSemaphore(m_logicalDevice.GetVkDevice(), &semaphoreCreateInfo, nullptr, &m_frames[i].imageAvailableSemaphore) != VK_SUCCESS)
        {
            HGERROR("Failed to create image available semaphore");
        }

        if(vkCreateSemaphore(m_logicalDevice.GetVkDevice(), &semaphoreCreateInfo, nullptr, &m_frames[i].renderFinishedSemaphore) != VK_SUCCESS)
        {
            HGERROR("Failed to render finished semaphore");
        }
    }

    HGINFO("Initialized synchronization structures");
}

VkCommandBuffer Renderer::BeginFrame()
{
    vkWaitForFences(m_logicalDevice.GetVkDevice(), 1, &GetCurrentFrame().inFlightFence, VK_TRUE, std::numeric_limits<u64>::max());
    vkResetFences(m_logicalDevice.GetVkDevice(), 1, &GetCurrentFrame().inFlightFence);

    VkResult result = vkAcquireNextImageKHR(m_logicalDevice.GetVkDevice(), m_swapChain->GetSwapChain(), 1000000000,
                                            GetCurrentFrame().imageAvailableSemaphore, VK_NULL_HANDLE, &m_currentImageIndex);

    if(result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return nullptr;
    }

    if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { HGERROR("failed to acquire swap chain image!"); }

    VkCommandBuffer cmd = GetCurrentFrame().commandBuffer;
    if(vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) { HGERROR("Failed to reset command buffer"); }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if(vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) { HGERROR("Failed to begin recording command buffer"); }

    return cmd;
}

void Renderer::EndFrame()
{
    if(vkEndCommandBuffer(GetCurrentFrame().commandBuffer) != VK_SUCCESS) { HGERROR("Failed to record command buffer"); }

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = GetCurrentFrame().commandBuffer;
    cmdInfo.deviceMask = 0;

    VkSemaphoreSubmitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitInfo.semaphore = GetCurrentFrame().imageAvailableSemaphore;
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalInfo.semaphore = GetCurrentFrame().renderFinishedSemaphore;
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.pNext = nullptr;
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &waitInfo;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signalInfo;

    if(vkQueueSubmit2(m_logicalDevice.GetGraphicsQueue(), 1, &submit, GetCurrentFrame().inFlightFence) != VK_SUCCESS)
    {
        HGERROR("Failed to submit command buffer");
    }

    auto s = m_swapChain->GetSwapChain();

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &s;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderFinishedSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &m_currentImageIndex;

    auto result = vkQueuePresentKHR(m_logicalDevice.GetPresentQueue(), &presentInfo);
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window.WasWindowResized())
    {
        m_window.ResetWindowResizedFlag();
        RecreateSwapChain();
    }

    else if(result != VK_SUCCESS) { HGERROR("failed to present swap chain image"); }

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

} // namespace Humongous
