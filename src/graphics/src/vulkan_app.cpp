// okay listen, this class is currently a mess
// just bear with me, i swear it'll be clean in the next commit

#include <logger.hpp>
#include <thread>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <vulkan_app.hpp>

namespace Humongous
{

VulkanApp::VulkanApp()
{
    Init();
    CreateCommandPool();
    AllocateCommandBuffers();
    InitSyncStructures();
}

VulkanApp::~VulkanApp() { m_mainDeletionQueue.Flush(); }

void VulkanApp::Init()
{
    m_window = std::make_unique<Window>();
    m_instance = std::make_unique<Instance>();
    m_physicalDevice = std::make_unique<PhysicalDevice>(*m_instance, *m_window);
    m_logicalDevice = std::make_unique<LogicalDevice>(*m_instance, *m_physicalDevice);
    m_swapChain = std::make_unique<SwapChain>(*m_window, *m_physicalDevice, *m_logicalDevice);

    CreatePipelineLayout();

    RenderPipeline::PipelineConfigInfo info;
    HGINFO("default configuring pipeline layout...");
    info = RenderPipeline::DefaultPipelineConfigInfo();
    HGINFO("success");
    info.pipelineLayout = pipelineLayout;
    m_renderPipeline = std::make_unique<RenderPipeline>(*m_logicalDevice, info);

    m_mainDeletionQueue.PushDeletor([&]() {
        m_swapChain.reset();
        m_logicalDevice.reset();
        m_physicalDevice.reset();
        m_window.reset();
        m_instance.reset();
    });

    m_mainDeletionQueue.PushDeletor([&]() {
        vkDestroyPipelineLayout(m_logicalDevice->GetVkDevice(), pipelineLayout, nullptr);
        m_renderPipeline.reset();
    });

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = m_physicalDevice->GetVkPhysicalDevice();
    allocatorInfo.device = m_logicalDevice->GetVkDevice();
    allocatorInfo.instance = m_instance->GetVkInstance();
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.pAllocationCallbacks = nullptr;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    VkExtent3D drawImageExtent = {m_window->GetExtent().width, m_window->GetExtent().height, 1};

    // hardcoding the draw format to 32 bit float
    m_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // VkImageCreateInfo imgInfo = vkinit::ImageCreateInfo(m_drawImage.imageFormat, drawImageUsages, drawImageExtent);
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = m_drawImage.imageFormat;
    imgInfo.extent = m_drawImage.imageExtent;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = drawImageUsages;

    // for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo imgAllocInfo{};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    vmaCreateImage(m_allocator, &imgInfo, &imgAllocInfo, &m_drawImage.image, &m_drawImage.allocation, nullptr);

    // build an image-view for the draw image to use for rendering
    // VkImageViewCreateInfo viewInfo = vkinit::ImageViewCreateInfo(m_drawImage.imageFormat, m_drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_drawImage.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_drawImage.imageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if(vkCreateImageView(m_logicalDevice->GetVkDevice(), &viewInfo, nullptr, &m_drawImage.imageView) != VK_SUCCESS)
    {
        HGERROR("Failed to create image view");
    }

    // add to deletion queues
    m_mainDeletionQueue.PushDeletor([&]() {
        vkDestroyImageView(m_logicalDevice->GetVkDevice(), m_drawImage.imageView, nullptr);
        vmaDestroyImage(m_allocator, m_drawImage.image, m_drawImage.allocation);
        vmaDestroyAllocator(m_allocator);
    });
}

void VulkanApp::CreatePipelineLayout()
{
    HGINFO("Creating pipeline layout...");
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if(vkCreatePipelineLayout(m_logicalDevice->GetVkDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        HGERROR("Failed to create pipeline layout");
    }

    HGINFO("Created pipeline layout");
}

void VulkanApp::CreateCommandPool()
{
    HGINFO("Creating command pool...");
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_physicalDevice->FindQueueFamilies(m_physicalDevice->GetVkPhysicalDevice()).graphicsFamily.value();

    if(vkCreateCommandPool(m_logicalDevice->GetVkDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        HGERROR("Failed to create command pool");
    }

    m_mainDeletionQueue.PushDeletor([&]() { vkDestroyCommandPool(m_logicalDevice->GetVkDevice(), m_commandPool, nullptr); });

    HGINFO("Created command pool");
}

void VulkanApp::AllocateCommandBuffers()
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
        if(vkAllocateCommandBuffers(m_logicalDevice->GetVkDevice(), &allocInfo, &frame.commandBuffer) != VK_SUCCESS)
        {
            HGERROR("Failed to allocate command buffers");
        }
    }

    HGINFO("Allocated command buffers");

    m_mainDeletionQueue.PushDeletor([&]() {
        for(Frame& frame: m_frames) { vkFreeCommandBuffers(m_logicalDevice->GetVkDevice(), m_commandPool, 1, &frame.commandBuffer); }
    });
}

void VulkanApp::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    m_drawImageExtent.width = m_drawImage.imageExtent.width;
    m_drawImageExtent.height = m_drawImage.imageExtent.height;

    if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) { HGERROR("Failed to begin recording command buffer"); }

    SwapChain::TransitionImageLayout(commandBuffer, m_drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_drawImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {0, 0, m_swapChain->GetExtent().width, m_swapChain->GetExtent().height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    renderingInfo.pNext = nullptr;
    renderingInfo.viewMask = 0;
    renderingInfo.flags = 0;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_renderPipeline->GetPipeline());

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_drawImageExtent.width);
    viewport.height = static_cast<float>(m_drawImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = m_drawImageExtent.width;
    scissor.extent.height = m_drawImageExtent.height;

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRendering(commandBuffer);

    SwapChain::TransitionImageLayout(commandBuffer, m_drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    SwapChain::TransitionImageLayout(commandBuffer, m_swapChain->GetImages()[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    SwapChain::CopyImageToImage(commandBuffer, m_drawImage.image, m_swapChain->GetImages()[imageIndex], m_drawImageExtent,
                                m_swapChain->GetExtent());

    SwapChain::TransitionImageLayout(commandBuffer, m_swapChain->GetImages()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) { HGERROR("Failed to record command buffer"); }
}

void VulkanApp::InitSyncStructures()
{
    HGINFO("Initializing syncronization structures...");

    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame
    // and 2 semaphores to synchronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.flags = 0;

    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
    {
        if(vkCreateFence(m_logicalDevice->GetVkDevice(), &fenceCreateInfo, nullptr, &m_frames[i].inFlightFence) != VK_SUCCESS)
        {
            HGERROR("Failed to create fence");
        }

        if(vkCreateSemaphore(m_logicalDevice->GetVkDevice(), &semaphoreCreateInfo, nullptr, &m_frames[i].imageAvailableSemaphore) != VK_SUCCESS)
        {
            HGERROR("Failed to create image available semaphore");
        }

        if(vkCreateSemaphore(m_logicalDevice->GetVkDevice(), &semaphoreCreateInfo, nullptr, &m_frames[i].renderFinishedSemaphore) != VK_SUCCESS)
        {
            HGERROR("Failed to render finished semaphore");
        }
    }

    HGINFO("Initialized syncronization structures");

    m_mainDeletionQueue.PushDeletor([&]() {
        for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroyFence(m_logicalDevice->GetVkDevice(), m_frames[i].inFlightFence, nullptr);
            vkDestroySemaphore(m_logicalDevice->GetVkDevice(), m_frames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_logicalDevice->GetVkDevice(), m_frames[i].renderFinishedSemaphore, nullptr);
        }
    });
}

void VulkanApp::SubmitCommandBuffer(VkCommandBuffer commandBuffer, u32 imageIndex)
{
    // TODO: implement
    // prepare the submission to the queue.
    // we want to wait on the presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the renderSemaphore, to signal the rendering has finishedo

    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = commandBuffer;
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

    // submit command buffer to the queue and execute it.
    // renderFence will now block until the graphic commands finish execution
    if(vkQueueSubmit2(m_logicalDevice->GetGraphicsQueue(), 1, &submit, GetCurrentFrame().inFlightFence) != VK_SUCCESS)
    {
        HGERROR("Failed to submit command buffer");
    }

    // prepare present
    // this will put the image we just rendered into the visible window
    // we want to wait on the renderSemaphore for that
    // as its necessary that drawing commands have finished before the image is displayed to the user
    auto s = m_swapChain->GetSwapChain();

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &s;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderFinishedSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &imageIndex;

    if(vkQueuePresentKHR(m_logicalDevice->GetPresentQueue(), &presentInfo) != VK_SUCCESS) { HGERROR("Failed to present image"); }
}

void VulkanApp::DrawFrame()
{
    // wait until the gpu has finished rendering the last. frame, Timeout of 1 second
    if(vkWaitForFences(m_logicalDevice->GetVkDevice(), 1, &GetCurrentFrame().inFlightFence, VK_TRUE, 1000000000) != VK_SUCCESS)
    {
        HGERROR("Failed to wait for fence");
    }
    if(vkResetFences(m_logicalDevice->GetVkDevice(), 1, &GetCurrentFrame().inFlightFence) != VK_SUCCESS) { HGERROR("Failed to reset fence"); }

    uint32_t swapchainImageIndex;
    if(vkAcquireNextImageKHR(m_logicalDevice->GetVkDevice(), m_swapChain->GetSwapChain(), 1000000000, GetCurrentFrame().imageAvailableSemaphore,
                             VK_NULL_HANDLE, &swapchainImageIndex) != VK_SUCCESS)
    {
        HGERROR("Failed to acquire next image");
    }

    VkCommandBuffer cmd = GetCurrentFrame().commandBuffer;
    if(vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) { HGERROR("Failed to reset command buffer"); }

    RecordCommandBuffer(cmd, swapchainImageIndex);
    SubmitCommandBuffer(cmd, swapchainImageIndex);

    frameNumber++;
}

void VulkanApp::Run()
{
    while(!m_window->ShouldWindowClose())
    {
        glfwPollEvents();

        if(glfwGetKey(m_window->GetWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwGetKey(m_window->GetWindow(), GLFW_KEY_Q) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(m_window->GetWindow(), true);
        }

        if(!m_window->IsFocused() || m_window->IsMinimized()) { std::this_thread::sleep_for(std::chrono::milliseconds(300)); }
        else { DrawFrame(); }
    }
    vkDeviceWaitIdle(m_logicalDevice->GetVkDevice());
}

} // namespace Humongous
