#pragma once
#include "defines.hpp"
#include "instance.hpp"
#include "non_copyable.hpp"
#include "physical_device.hpp"
#include "vk_mem_alloc.h"

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

    virtual VmaAllocator GetVmaAllocator() const = 0;

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

    struct VMAData
    {
        u32 allocationCount = 0;
        u32 freeCount = 0;
    };
};

class VulkanLogicalDevice : public ILogicalDevice, NonCopyable
{
public:
    VulkanLogicalDevice(Instance& instance, IPhysicalDevice& physicalDevice);
    ~VulkanLogicalDevice() override;

    vk::Device       GetVkDevice() const override { return m_logicalDevice; }
    IPhysicalDevice& GetPhysicalDevice() const override { return *m_physicalDevice; }

    vk::Queue GetGraphicsQueue() const override { return m_graphicsQueue; }
    vk::Queue GetPresentQueue() const override { return m_presentQueue; }

    u32 GetGraphicsQueueIndex() const override { return m_graphicsQueueIndex; }
    u32 GetPresentQueueIndex() const override { return m_presentQueueIndex; }

    VmaAllocator GetVmaAllocator() const override { return m_allocator; }

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

    struct VMAData
    {
        u32 allocationCount = 0;
        u32 freeCount = 0;
    };

private:
    Instance& m_instance;

    vk::Device       m_logicalDevice = VK_NULL_HANDLE;
    IPhysicalDevice* m_physicalDevice;

    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;
    u32       m_graphicsQueueIndex;
    u32       m_presentQueueIndex;

    VmaAllocator m_allocator;

    VMAData m_vmaData;

    vk::CommandPool m_commandPool;

    void CreateLogicalDevice(Instance& instance, IPhysicalDevice& physicalDevice);
    void CreateVmaAllocator(Instance& instance, IPhysicalDevice& physicalDevice);
    void CreateCommandPool(IPhysicalDevice& physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> CreateQueues(IPhysicalDevice& physicalDevice);
};
} // namespace Humongous
