#define VMA_IMPLEMENTATION

#include "logical_device.hpp"
#include "asserts.hpp"
#include "logger.hpp"

#include "vk_mem_alloc.h"
#include <set>

namespace Humongous
{
VulkanLogicalDevice::VulkanLogicalDevice(IInstance& instance, IPhysicalDevice& physicalDevice)
    : m_logicalDevice{VK_NULL_HANDLE}, m_instance{instance}, m_physicalDevice{&physicalDevice}
{
    HGINFO("Creating logical device...");
    CreateLogicalDevice(instance, physicalDevice);
    CreateCommandPool(physicalDevice);
    m_allocator = std::make_unique<Allocator>(*this, instance);
    HGINFO("Created logical device");
}

VulkanLogicalDevice::~VulkanLogicalDevice()
{
    HGINFO("Destroying logical device...");
    vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
    m_allocator.reset();
    vkDestroyDevice(m_logicalDevice, nullptr);
    HGINFO("Destroyed logical device");
}

void VulkanLogicalDevice::CreateLogicalDevice(IInstance& instance, IPhysicalDevice& physicalDevice)
{
    HGASSERT(m_logicalDevice == VK_NULL_HANDLE && "Logical device has already been made!");
    HGASSERT(physicalDevice.GetVkPhysicalDevice() != VK_NULL_HANDLE && "Can't create a logical device with a null physical device!");

    IPhysicalDevice::QueueFamilyData indices = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice());

    HGASSERT(indices.IsComplete() && "Incomplete queue family indices!");
    m_graphicsQueueIndex = indices.graphicsFamily.value();
    m_presentQueueIndex = indices.presentFamily.value();

    vk::PhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.shaderDrawParameters = VK_TRUE;

    // vulkan 1.2 features
    vk::PhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    vulkan12Features.samplerFilterMinmax = VK_TRUE;
    vulkan12Features.pNext = &vulkan11Features;

    // vulkan 1.3 features
    vk::PhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.pNext = &vulkan12Features;

    vk::PhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.features.multiDrawIndirect = true;
    deviceFeatures2.features.samplerAnisotropy = true;
    deviceFeatures2.pNext = &vulkan13Features;

    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;

    const f32 prio = 1.0f;

    auto queueCreateInfos = CreateQueues(physicalDevice);
    queueCreateInfos[0].pQueuePriorities = &prio;
    queueCreateInfos[1].pQueuePriorities = &prio;

    auto extensions = physicalDevice.GetDeviceExtensions();

    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size()); // queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.pNext = &deviceFeatures2;
    createInfo.pEnabledFeatures = nullptr;

    auto result = physicalDevice.GetVkPhysicalDevice().createDevice(&createInfo, nullptr, &m_logicalDevice);
    if(result != vk::Result::eSuccess)
    {
        HGFATAL("Failed to create logical device! Error: %s", vk::to_string(result).c_str());
        return;
    }

    HGINFO("logical device created");

    if(m_graphicsQueueIndex == m_presentQueueIndex)
    {
        // If graphics and present families are the same, they both use queue family at m_graphicsQueueIndex
        // We request queue index 0 from that family for both graphics and present operations.
        vk::DeviceQueueInfo2 graphicsQueueInfo{};
        graphicsQueueInfo.sType = vk::StructureType::eDeviceQueueInfo2;
        graphicsQueueInfo.queueFamilyIndex = m_graphicsQueueIndex;
        graphicsQueueInfo.queueIndex = 0; // Request the first (and likely only) queue from this family

        m_logicalDevice.getQueue2(&graphicsQueueInfo, &m_graphicsQueue);
        m_presentQueue = m_graphicsQueue; // They are the same queue
    }
    else
    {
        HGINFO("For whatever reason, they weren't equal");
        // Graphics and present families are different
        vk::DeviceQueueInfo2 graphicsQueueInfo{};
        graphicsQueueInfo.sType = vk::StructureType::eDeviceQueueInfo2;
        graphicsQueueInfo.queueFamilyIndex = m_graphicsQueueIndex;
        graphicsQueueInfo.queueIndex = 0; // Request the first queue from the graphics family

        vk::DeviceQueueInfo2 presentQueueInfo{};
        presentQueueInfo.sType = vk::StructureType::eDeviceQueueInfo2;
        presentQueueInfo.queueFamilyIndex = m_presentQueueIndex;
        presentQueueInfo.queueIndex = 0; // Request the first queue from the present family

        m_logicalDevice.getQueue2(&graphicsQueueInfo, &m_graphicsQueue);
        m_logicalDevice.getQueue2(&presentQueueInfo, &m_presentQueue);
    }

    HGINFO("logical device queues acquired");
}

std::vector<vk::DeviceQueueCreateInfo> VulkanLogicalDevice::CreateQueues(IPhysicalDevice& physicalDevice)
{
    HGINFO("acquiring queue handles...");

    IPhysicalDevice::QueueFamilyData indices = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice());

    // Use std::set to get unique queue family indices
    std::set<u32> uniqueQueueFamilyIndices; // Use u32 for Vulkan indices
    if(indices.graphicsFamily.has_value()) { uniqueQueueFamilyIndices.insert(indices.graphicsFamily.value()); }
    if(indices.presentFamily.has_value()) { uniqueQueueFamilyIndices.insert(indices.presentFamily.value()); }

    if(uniqueQueueFamilyIndices.empty()) { HGFATAL("Failed to find any suitable queue families!"); }

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float                                  queuePriority = 1.0f;

    for(u32 queueFamilyIndex: uniqueQueueFamilyIndices)
    {
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = vk::StructureType::eDeviceQueueCreateInfo;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    return queueCreateInfos;
}

void VulkanLogicalDevice::CreateCommandPool(IPhysicalDevice& physicalDevice)
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = physicalDevice.FindQueueFamilies(physicalDevice.GetVkPhysicalDevice()).graphicsFamily.value();

    if(m_logicalDevice.createCommandPool(&poolInfo, nullptr, &m_commandPool) != vk::Result::eSuccess) { HGFATAL("Failed to create command pool!"); }
}

vk::CommandBuffer VulkanLogicalDevice::BeginSingleTimeCommands() const
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer commandBuffer{};
    auto              result = m_logicalDevice.allocateCommandBuffers(&allocInfo, &commandBuffer);
    if(result != vk::Result::eSuccess) { HGERROR("Failed to allocate single time command! Error: %s", vk::to_string(result).c_str()); }

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    result = commandBuffer.begin(&beginInfo);
    if(result != vk::Result::eSuccess) { HGERROR("Failed to start single time command! Error: %s", vk::to_string(result).c_str()); }
    return commandBuffer;
}

void VulkanLogicalDevice::EndSingleTimeCommands(vk::CommandBuffer commandBuffer) const
{
    vkEndCommandBuffer(commandBuffer);

    vk::CommandBufferSubmitInfo commandBufferInfo{};
    commandBufferInfo.setCommandBuffer(commandBuffer);
    commandBufferInfo.deviceMask = 0;

    vk::SubmitInfo2 submitInfo{};
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferInfo;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.pSignalSemaphoreInfos = nullptr;
    submitInfo.waitSemaphoreInfoCount = 0;
    submitInfo.pWaitSemaphoreInfos = nullptr;
    auto r = m_graphicsQueue.submit2(1, &submitInfo, VK_NULL_HANDLE);
    if(r != vk::Result::eSuccess) { HGERROR("Failed to submit single time command! Error: %s", vk::to_string(r).c_str()); }

    m_graphicsQueue.waitIdle();
    m_logicalDevice.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
}

vk::DescriptorPool VulkanLogicalDevice::CreateDescriptorPool(const vk::DescriptorPoolCreateInfo& info) const
{
    vk::DescriptorPool pool;
    auto               result = m_logicalDevice.createDescriptorPool(&info, nullptr, &pool);
    if(result != vk::Result::eSuccess) { HGERROR("Failed to create descriptor pool! Error: %s", vk::to_string(result).c_str()); }
    return pool;
}

void VulkanLogicalDevice::DestroyDescriptorPool(vk::DescriptorPool pool) const { m_logicalDevice.destroyDescriptorPool(pool); }

void VulkanLogicalDevice::FreeDescriptorSets(vk::DescriptorPool pool, std::vector<vk::DescriptorSet>& descriptors) const
{
    m_logicalDevice.freeDescriptorSets(pool, static_cast<u32>(descriptors.size()), descriptors.data());
}

void VulkanLogicalDevice::ResetDescriptorPool(vk::DescriptorPool pool) const { m_logicalDevice.resetDescriptorPool(pool); }

void VulkanLogicalDevice::UpdateDescriptorSets(const std::vector<vk::WriteDescriptorSet>& writes) const
{
    m_logicalDevice.updateDescriptorSets(static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

vk::Result VulkanLogicalDevice::AllocateDescriptorSets(const vk::DescriptorSetAllocateInfo* pAllocateInfo, vk::DescriptorSet* pDescriptorSets) const
{
    auto res = m_logicalDevice.allocateDescriptorSets(pAllocateInfo, pDescriptorSets);
    if(res != vk::Result::eSuccess) { HGERROR("Failed to allocate descriptor sets! Error: %s", vk::to_string(res).c_str()); }
    return res;
}

void VulkanLogicalDevice::CreateDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& info, vk::DescriptorSetLayout* layout) const
{
    auto ret = m_logicalDevice.createDescriptorSetLayout(&info, nullptr, layout);
    if(ret != vk::Result::eSuccess) { HGERROR("Failed to create descriptor set layout! Error: %s", vk::to_string(ret).c_str()); }
}

void VulkanLogicalDevice::DestroyDescriptorSetLayout(vk::DescriptorSetLayout layout) const
{
    m_logicalDevice.destroyDescriptorSetLayout(layout, nullptr);
}

vk::Sampler VulkanLogicalDevice::CreateSampler(const vk::SamplerCreateInfo& info) const
{
    vk::Sampler sampler;
    auto        ret = m_logicalDevice.createSampler(&info, nullptr, &sampler);

    if(ret != vk::Result::eSuccess) { HGERROR("Failed to create sampler! Error: %s", vk::to_string(ret).c_str()); }

    return sampler;
}

void VulkanLogicalDevice::DestroySampler(vk::Sampler sampler) const { m_logicalDevice.destroySampler(sampler); }

vk::DeviceAddress VulkanLogicalDevice::GetDeviceAddress(const vk::BufferDeviceAddressInfo& bufferDeviceAddressInfo) const
{
    return m_logicalDevice.getBufferAddress(&bufferDeviceAddressInfo);
}

vk::Result VulkanLogicalDevice::CreateImageView(const vk::ImageViewCreateInfo& info, vk::ImageView* view) const
{
    auto ret = m_logicalDevice.createImageView(&info, nullptr, view);
    if(ret != vk::Result::eSuccess) { HGERROR("Failed to create image view! Error: %s", vk::to_string(ret).c_str()); }

    return ret;
}

void VulkanLogicalDevice::DestroyImageView(vk::ImageView view) const { m_logicalDevice.destroyImageView(view); }

vk::Result VulkanLogicalDevice::CreatePipelineLayout(const vk::PipelineLayoutCreateInfo& info, vk::PipelineLayout* layout) const
{
    auto ret = m_logicalDevice.createPipelineLayout(&info, nullptr, layout);
    if(ret != vk::Result::eSuccess) { HGERROR("Failed to create pipeline layout! Error: %s", vk::to_string(ret).c_str()); }

    return ret;
}

void VulkanLogicalDevice::DestroyPipelineLayout(vk::PipelineLayout layout) const { m_logicalDevice.destroyPipelineLayout(layout); }

vk::Result VulkanLogicalDevice::CreateComputePipeline(const vk::ComputePipelineCreateInfo& info, vk::Pipeline* pipeline) const
{
    auto ret = m_logicalDevice.createComputePipelines(nullptr, 1, &info, nullptr, pipeline);
    if(ret != vk::Result::eSuccess) { HGERROR("Failed to create compute pipeline! Error: %s", vk::to_string(ret).c_str()); }

    return ret;
}

void VulkanLogicalDevice::DestroyComputePipeline(vk::Pipeline pipeline) const { m_logicalDevice.destroyPipeline(pipeline, nullptr); }

vk::Result VulkanLogicalDevice::CreateShaderModule(const vk::ShaderModuleCreateInfo& info, vk::ShaderModule* shaderModule) const
{
    auto ret = m_logicalDevice.createShaderModule(&info, nullptr, shaderModule);
    if(ret != vk::Result::eSuccess) { HGERROR("Failed to create shader module! Error: %s", vk::to_string(ret).c_str()); }

    return ret;
}

void VulkanLogicalDevice::DestroyShaderModule(vk::ShaderModule shaderModule) const { m_logicalDevice.destroyShaderModule(shaderModule, nullptr); }

vk::Result VulkanLogicalDevice::CreateGraphicsPipeline(const vk::GraphicsPipelineCreateInfo& info, vk::Pipeline* pipeline) const
{
    auto ret = m_logicalDevice.createGraphicsPipelines(VK_NULL_HANDLE, 1, &info, nullptr, pipeline);
    if(ret != vk::Result::eSuccess) { HGERROR("Failed to create graphics pipeline! Error: %s", vk::to_string(ret).c_str()); }

    return ret;
}

void VulkanLogicalDevice::DestroyPipeline(vk::Pipeline pipeline) const { m_logicalDevice.destroyPipeline(pipeline, nullptr); }

void VulkanLogicalDevice::RecordCopyBuffer(vk::CommandBuffer commandBuffer, const vk::CopyBufferInfo2& copyInfo) const
{
    commandBuffer.copyBuffer2(&copyInfo);
}

void VulkanLogicalDevice::RecordPipelineBarrier(vk::CommandBuffer commandBuffer, vk::DependencyInfo& dependencyInfo) const
{
    commandBuffer.pipelineBarrier2(dependencyInfo);
}

void VulkanLogicalDevice::RecordComputeDispatch(vk::CommandBuffer commandBuffer, u32 groupCountX, u32 groupCountY, u32 groupCountZ) const
{
    commandBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
}

void VulkanLogicalDevice::RecordBindDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout pipelineLayout,
                                                   vk::PipelineBindPoint pipelineBindPoint, const u32& firstSet,
                                                   const std::vector<vk::DescriptorSet>& descriptorSets) const
{
    cmd.bindDescriptorSets(pipelineBindPoint, pipelineLayout, firstSet, static_cast<u32>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
}

void VulkanLogicalDevice::RecordPushConstants(vk::CommandBuffer cmd, vk::PipelineLayout layout, vk::ShaderStageFlagBits shaderFlags,
                                              const void* data, size_t size) const
{
    cmd.pushConstants(layout, shaderFlags, 0, size, data);
}

void VulkanLogicalDevice::RecordBindPipeline(vk::CommandBuffer cmd, vk::PipelineBindPoint pipelineBindPoint, vk::Pipeline pipeline) const
{
    cmd.bindPipeline(pipelineBindPoint, pipeline);
}

void VulkanLogicalDevice::RecordCopyBufferToImage(vk::CommandBuffer cmd, vk::Buffer buffer, vk::Image image, vk::ImageLayout imageLayout,
                                                  const std::vector<vk::BufferImageCopy>& regions) const
{
    cmd.copyBufferToImage(buffer, image, imageLayout, static_cast<u32>(regions.size()), regions.data());
}

void VulkanLogicalDevice::RecordBlitImage(vk::CommandBuffer cmd, vk::BlitImageInfo2 blit) const { cmd.blitImage2(blit); }

vk::Result VulkanLogicalDevice::FlushMappedMemoryRanges(const std::vector<vk::MappedMemoryRange>& ranges) const
{
    return m_logicalDevice.flushMappedMemoryRanges(static_cast<u32>(ranges.size()), ranges.data());
}

} // namespace Humongous
