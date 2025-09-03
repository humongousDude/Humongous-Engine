#pragma once
#include "allocator.hpp"
#include "defines.hpp"
#include "instance.hpp"
#include "non_copyable.hpp"
#include "physical_device.hpp"
#include <memory>

namespace Humongous
{
class ILogicalDevice
{
public:
    virtual ~ILogicalDevice() = default;
    virtual vk::Device       GetVkDevice() const = 0;
    virtual IPhysicalDevice& GetPhysicalDevice() const = 0;

    virtual vk::Queue GetGraphicsQueue() const = 0;
    virtual vk::Queue GetPresentQueue() const = 0;

    virtual u32 GetGraphicsQueueIndex() const = 0;
    virtual u32 GetPresentQueueIndex() const = 0;

    virtual IAllocator& GetAllocator() const = 0;

    virtual vk::CommandBuffer BeginSingleTimeCommands() const = 0;
    virtual void              EndSingleTimeCommands(vk::CommandBuffer cmd) const = 0;

    virtual vk::DescriptorPool CreateDescriptorPool(const vk::DescriptorPoolCreateInfo& info) const = 0;
    virtual void               DestroyDescriptorPool(vk::DescriptorPool pool) const = 0;
    virtual void               FreeDescriptorSets(vk::DescriptorPool pool, std::vector<vk::DescriptorSet>& descriptors) const = 0;
    virtual void               ResetDescriptorPool(vk::DescriptorPool pool) const = 0;
    virtual void               UpdateDescriptorSets(const std::vector<vk::WriteDescriptorSet>& writes) const = 0;
    virtual vk::Result  AllocateDescriptorSets(const vk::DescriptorSetAllocateInfo* pAllocateInfo, vk::DescriptorSet* pDescriptorSets) const = 0;
    virtual void        CreateDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& info, vk::DescriptorSetLayout* layout) const = 0;
    virtual void        DestroyDescriptorSetLayout(vk::DescriptorSetLayout layout) const = 0;
    virtual vk::Sampler CreateSampler(const vk::SamplerCreateInfo& info) const = 0;
    virtual void        DestroySampler(vk::Sampler sampler) const = 0;
    virtual vk::DeviceAddress GetDeviceAddress(const vk::BufferDeviceAddressInfo& bufferDeviceAddressInfo) const = 0;
    virtual vk::Result        CreateImageView(const vk::ImageViewCreateInfo& info, vk::ImageView* view) const = 0;
    virtual void              DestroyImageView(vk::ImageView view) const = 0;
    virtual vk::Result        CreatePipelineLayout(const vk::PipelineLayoutCreateInfo& info, vk::PipelineLayout* layout) const = 0;
    virtual void              DestroyPipelineLayout(vk::PipelineLayout layout) const = 0;
    virtual vk::Result        CreateComputePipeline(const vk::ComputePipelineCreateInfo& info, vk::Pipeline* pipeline) const = 0;
    virtual void              DestroyComputePipeline(vk::Pipeline pipeline) const = 0;
    virtual vk::Result        CreateShaderModule(const vk::ShaderModuleCreateInfo& info, vk::ShaderModule* shaderModule) const = 0;
    virtual void              DestroyShaderModule(vk::ShaderModule shaderModule) const = 0;
    virtual vk::Result        CreateGraphicsPipeline(const vk::GraphicsPipelineCreateInfo& info, vk::Pipeline* pipeline) const = 0;
    virtual void              DestroyPipeline(vk::Pipeline pipeline) const = 0;
    virtual void              RecordCopyBuffer(vk::CommandBuffer cmd, const vk::CopyBufferInfo2& copyInfo) const = 0;
    virtual void              RecordPipelineBarrier(vk::CommandBuffer cmd, vk::DependencyInfo& dependencyInfo) const = 0;
    virtual void              RecordComputeDispatch(vk::CommandBuffer cmd, u32 groupCountX, u32 groupCountY, u32 groupCountZ) const = 0;
    virtual void       RecordBindDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint pipelineBindPoint,
                                                const u32& firstSet, const std::vector<vk::DescriptorSet>& descriptorSets) const = 0;
    virtual void       RecordPushConstants(vk::CommandBuffer cmd, vk::PipelineLayout layout, vk::ShaderStageFlagBits shaderFlags, const void* data,
                                           size_t size) const = 0;
    virtual void       RecordBindPipeline(vk::CommandBuffer cmd, vk::PipelineBindPoint pipelineBindPoint, vk::Pipeline pipeline) const = 0;
    virtual void       RecordCopyBufferToImage(vk::CommandBuffer cmd, vk::Buffer buffer, vk::Image image, vk::ImageLayout imageLayout,
                                               const std::vector<vk::BufferImageCopy>& regions) const = 0;
    virtual void       RecordBlitImage(vk::CommandBuffer cmd, vk::BlitImageInfo2 blit) const = 0;
    virtual vk::Result FlushMappedMemoryRanges(const std::vector<vk::MappedMemoryRange>& ranges) const = 0;
    virtual void       RecordDrawMesh(vk::CommandBuffer cmd, u32 taskCountx, u32 taskCounty, u32 taskCountz) const = 0;
};

class VulkanLogicalDevice : public ILogicalDevice, NonCopyable
{
public:
    VulkanLogicalDevice(IInstance& instance, IPhysicalDevice& physicalDevice);
    ~VulkanLogicalDevice() override;

    vk::Device       GetVkDevice() const override { return m_logicalDevice; }
    IPhysicalDevice& GetPhysicalDevice() const override { return *m_physicalDevice; }

    vk::Queue GetGraphicsQueue() const override { return m_graphicsQueue; }
    vk::Queue GetPresentQueue() const override { return m_presentQueue; }

    u32 GetGraphicsQueueIndex() const override { return m_graphicsQueueIndex; }
    u32 GetPresentQueueIndex() const override { return m_presentQueueIndex; }

    IAllocator& GetAllocator() const override { return *m_allocator; }

    vk::CommandBuffer BeginSingleTimeCommands() const override;
    void              EndSingleTimeCommands(vk::CommandBuffer cmd) const override;

    vk::DescriptorPool CreateDescriptorPool(const vk::DescriptorPoolCreateInfo& info) const override;
    void               DestroyDescriptorPool(vk::DescriptorPool pool) const override;
    void               FreeDescriptorSets(vk::DescriptorPool pool, std::vector<vk::DescriptorSet>& descriptors) const override;
    void               ResetDescriptorPool(vk::DescriptorPool pool) const override;
    void               UpdateDescriptorSets(const std::vector<vk::WriteDescriptorSet>& writes) const override;
    vk::Result        AllocateDescriptorSets(const vk::DescriptorSetAllocateInfo* pAllocateInfo, vk::DescriptorSet* pDescriptorSets) const override;
    void              CreateDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& info, vk::DescriptorSetLayout* layout) const override;
    void              DestroyDescriptorSetLayout(vk::DescriptorSetLayout layout) const override;
    vk::Sampler       CreateSampler(const vk::SamplerCreateInfo& info) const override;
    void              DestroySampler(vk::Sampler sampler) const override;
    vk::DeviceAddress GetDeviceAddress(const vk::BufferDeviceAddressInfo& bufferDeviceAddressInfo) const override;
    vk::Result        CreateImageView(const vk::ImageViewCreateInfo& info, vk::ImageView* view) const override;
    void              DestroyImageView(vk::ImageView view) const override;
    vk::Result        CreatePipelineLayout(const vk::PipelineLayoutCreateInfo& info, vk::PipelineLayout* layout) const override;
    void              DestroyPipelineLayout(vk::PipelineLayout layout) const override;
    vk::Result        CreateComputePipeline(const vk::ComputePipelineCreateInfo& info, vk::Pipeline* pipeline) const override;
    void              DestroyComputePipeline(vk::Pipeline pipeline) const override;
    vk::Result        CreateShaderModule(const vk::ShaderModuleCreateInfo& info, vk::ShaderModule* shaderModule) const override;
    void              DestroyShaderModule(vk::ShaderModule shaderModule) const override;
    vk::Result        CreateGraphicsPipeline(const vk::GraphicsPipelineCreateInfo& info, vk::Pipeline* pipeline) const override;
    void              DestroyPipeline(vk::Pipeline pipeline) const override;
    void              RecordCopyBuffer(vk::CommandBuffer commandBuffer, const vk::CopyBufferInfo2& copyInfo) const override;
    void              RecordPipelineBarrier(vk::CommandBuffer commandBuffer, vk::DependencyInfo& dependencyInfo) const override;
    void              RecordComputeDispatch(vk::CommandBuffer commandBuffer, u32 groupCountX, u32 groupCountY, u32 groupCountZ) const override;
    void              RecordBindDescriptorSets(vk::CommandBuffer cmd, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint pipelineBindPoint,
                                               const u32& firstSet, const std::vector<vk::DescriptorSet>& descriptorSets) const override;
    void              RecordPushConstants(vk::CommandBuffer cmd, vk::PipelineLayout layout, vk::ShaderStageFlagBits shaderFlags, const void* data,
                                          size_t size) const override;
    void              RecordBindPipeline(vk::CommandBuffer cmd, vk::PipelineBindPoint pipelineBindPoint, vk::Pipeline pipeline) const override;
    void              RecordCopyBufferToImage(vk::CommandBuffer cmd, vk::Buffer buffer, vk::Image image, vk::ImageLayout imageLayout,
                                              const std::vector<vk::BufferImageCopy>& regions) const override;
    void              RecordBlitImage(vk::CommandBuffer cmd, vk::BlitImageInfo2 blit) const override;
    vk::Result        FlushMappedMemoryRanges(const std::vector<vk::MappedMemoryRange>& ranges) const override;
    void              RecordDrawMesh(vk::CommandBuffer cmd, u32 taskCountx, u32 taskCounty, u32 taskCountz) const override;

private:
    IInstance& m_instance;

    vk::Device       m_logicalDevice = VK_NULL_HANDLE;
    IPhysicalDevice* m_physicalDevice;

    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    u32       m_graphicsQueueIndex;
    u32       m_presentQueueIndex;

    std::unique_ptr<Allocator> m_allocator;

    vk::CommandPool m_commandPool;

    PFN_vkCmdDrawMeshTasksEXT m_drawMeshTasks;

    void CreateLogicalDevice(IInstance& instance, IPhysicalDevice& physicalDevice);
    void CreateCommandPool(IPhysicalDevice& physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> CreateQueues(IPhysicalDevice& physicalDevice);
};
} // namespace Humongous
