#pragma once

#include "defines.hpp"
#include "logical_device.hpp"
#include "mock_allocator.hpp"
#include "mock_physical_device.hpp"
#include "gmock/gmock.h"

//
namespace Humongous
{

class MockLogicalDevice : public ILogicalDevice
{
public:
    mutable testing::NiceMock<MockPhysicalDevice> m_physicalDevice;
    mutable testing::NiceMock<MockAllocator>      m_allocator;
    mutable vk::Queue                             m_graphicsQueue;
    mutable vk::Queue                             m_presentQueue;
    mutable u32                                   m_graphicsQueueIndex;
    mutable u32                                   m_presentQueueIndex;

    MockLogicalDevice()
    {
        ON_CALL(*this, GetPhysicalDevice()).WillByDefault(testing::ReturnRef(m_physicalDevice));

        ON_CALL(*this, CreateComputePipeline(::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgPointee<1>(vk::Pipeline(reinterpret_cast<VkPipeline>(0xCAFEC0DE))),
                                            ::testing::Return(vk::Result::eSuccess)));

        ON_CALL(*this, CreateGraphicsPipeline(::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgPointee<1>(vk::Pipeline(reinterpret_cast<VkPipeline>(0xC0DECAFE))),
                                            ::testing::Return(vk::Result::eSuccess)));

        ON_CALL(*this, CreateShaderModule(::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgPointee<1>(vk::ShaderModule(reinterpret_cast<VkShaderModule>(0xDEADBEEF))),
                                            ::testing::Return(vk::Result::eSuccess)));

        ON_CALL(*this, CreatePipelineLayout(::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgPointee<1>(vk::PipelineLayout(reinterpret_cast<VkPipelineLayout>(0xDEADBEEF))),
                                            ::testing::Return(vk::Result::eSuccess)));

        ON_CALL(*this, DestroyPipeline(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, DestroyShaderModule(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, DestroyComputePipeline(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, DestroyPipelineLayout(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, GetGraphicsQueue()).WillByDefault(::testing::Return(m_graphicsQueue));
        ON_CALL(*this, GetPresentQueue()).WillByDefault(::testing::Return(m_presentQueue));

        ON_CALL(*this, GetGraphicsQueueIndex()).WillByDefault(::testing::Return(m_graphicsQueueIndex));
        ON_CALL(*this, GetPresentQueueIndex()).WillByDefault(::testing::Return(m_presentQueueIndex));

        ON_CALL(*this, GetAllocator()).WillByDefault(::testing::ReturnRef(m_allocator));

        ON_CALL(*this, BeginSingleTimeCommands())
            .WillByDefault(::testing::Return(vk::CommandBuffer(reinterpret_cast<VkCommandBuffer>(0xAABBCCDD))));

        ON_CALL(*this, EndSingleTimeCommands(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, CreateDescriptorPool(::testing::_))
            .WillByDefault(::testing::Return(vk::DescriptorPool(reinterpret_cast<VkDescriptorPool>(0xAABBCCDD))));

        ON_CALL(*this, AllocateDescriptorSets(::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgPointee<1>(vk::DescriptorSet(reinterpret_cast<VkDescriptorSet>(0xAABBCCDD))),
                                            ::testing::Return(vk::Result::eSuccess)));

        ON_CALL(*this, ResetDescriptorPool(::testing::_)).WillByDefault(::testing::Return());
        ON_CALL(*this, UpdateDescriptorSets(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, CreateDescriptorSetLayout(::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(
                ::testing::SetArgPointee<1>(vk::DescriptorSetLayout(reinterpret_cast<VkDescriptorSetLayout>(0xAABBCCDD))), ::testing::Return()));

        ON_CALL(*this, DestroyDescriptorSetLayout(::testing::_)).WillByDefault(::testing::Return());

        ON_CALL(*this, CreateSampler(::testing::_)).WillByDefault(::testing::Return(vk::Sampler(reinterpret_cast<VkSampler>(0xAABBCCDD))));

        ON_CALL(*this, DestroySampler(::testing::_)).WillByDefault(::testing::Return());
    }

    MOCK_METHOD(vk::Device, GetVkDevice, (), (const, override));
    MOCK_METHOD(IPhysicalDevice&, GetPhysicalDevice, (), (const, override));
    MOCK_METHOD(vk::Queue, GetGraphicsQueue, (), (const, override));
    MOCK_METHOD(vk::Queue, GetPresentQueue, (), (const, override));
    MOCK_METHOD(u32, GetGraphicsQueueIndex, (), (const, override));
    MOCK_METHOD(u32, GetPresentQueueIndex, (), (const, override));
    MOCK_METHOD(IAllocator&, GetAllocator, (), (const, override));
    MOCK_METHOD(vk::CommandBuffer, BeginSingleTimeCommands, (), (const, override));
    MOCK_METHOD(void, EndSingleTimeCommands, (vk::CommandBuffer cmd), (const, override));
    MOCK_METHOD(vk::DescriptorPool, CreateDescriptorPool, (const vk::DescriptorPoolCreateInfo& info), (const, override));
    MOCK_METHOD(void, DestroyDescriptorPool, (vk::DescriptorPool pool), (const, override));
    MOCK_METHOD(void, FreeDescriptorSets, (vk::DescriptorPool pool, std::vector<vk::DescriptorSet>& descriptors), (const, override));
    MOCK_METHOD(void, ResetDescriptorPool, (vk::DescriptorPool pool), (const, override));
    MOCK_METHOD(void, UpdateDescriptorSets, (const std::vector<vk::WriteDescriptorSet>& writes), (const, override));
    MOCK_METHOD(vk::Result, AllocateDescriptorSets, (const vk::DescriptorSetAllocateInfo* pAllocateInfo, vk::DescriptorSet* pDescriptorSets),
                (const, override));
    MOCK_METHOD(void, CreateDescriptorSetLayout, (const vk::DescriptorSetLayoutCreateInfo& info, vk::DescriptorSetLayout* layout),
                (const, override));
    MOCK_METHOD(void, DestroyDescriptorSetLayout, (vk::DescriptorSetLayout layout), (const, override));
    MOCK_METHOD(vk::Sampler, CreateSampler, (const vk::SamplerCreateInfo& info), (const, override));
    MOCK_METHOD(void, DestroySampler, (vk::Sampler sampler), (const, override));
    MOCK_METHOD(vk::DeviceAddress, GetDeviceAddress, (const vk::BufferDeviceAddressInfo& bufferDeviceAddressInfo), (const, override));
    MOCK_METHOD(vk::Result, CreatePipelineLayout, (const vk::PipelineLayoutCreateInfo& info, vk::PipelineLayout* layout), (const, override));
    MOCK_METHOD(void, DestroyPipelineLayout, (vk::PipelineLayout layout), (const, override));
    MOCK_METHOD(vk::Result, CreateImageView, (const vk::ImageViewCreateInfo& info, vk::ImageView* view), (const, override));
    MOCK_METHOD(void, DestroyImageView, (vk::ImageView view), (const, override));
    MOCK_METHOD(vk::Result, CreateComputePipeline, (const vk::ComputePipelineCreateInfo& info, vk::Pipeline* pipeline), (const, override));
    MOCK_METHOD(void, DestroyComputePipeline, (vk::Pipeline pipeline), (const, override));
    MOCK_METHOD(vk::Result, CreateShaderModule, (const vk::ShaderModuleCreateInfo& info, vk::ShaderModule* shaderModule), (const, override));
    MOCK_METHOD(void, DestroyShaderModule, (vk::ShaderModule shaderModule), (const, override));
    MOCK_METHOD(vk::Result, CreateGraphicsPipeline, (const vk::GraphicsPipelineCreateInfo& info, vk::Pipeline* pipeline), (const, override));
    MOCK_METHOD(void, DestroyPipeline, (vk::Pipeline pipeline), (const, override));
    MOCK_METHOD(void, RecordCopyBuffer, (vk::CommandBuffer commandBuffer, const vk::CopyBufferInfo2& copyInfo), (const, override));
    MOCK_METHOD(void, RecordPipelineBarrier, (vk::CommandBuffer commandBuffer, vk::DependencyInfo& dependencyInfo), (const, override));
    MOCK_METHOD(void, RecordComputeDispatch, (vk::CommandBuffer commandBuffer, u32 groupCountX, u32 groupCountY, u32 groupCountZ),
                (const, override));
    MOCK_METHOD(void, RecordBindDescriptorSets,
                (vk::CommandBuffer cmd, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint pipelineBindPoint, const u32& firstSet,
                 const std::vector<vk::DescriptorSet>& descriptorSets),
                (const, override));
    MOCK_METHOD(void, RecordPushConstants,
                (vk::CommandBuffer cmd, vk::PipelineLayout layout, vk::ShaderStageFlagBits shaderFlags, const void* data, size_t size),
                (const, override));
    MOCK_METHOD(void, RecordBindPipeline, (vk::CommandBuffer cmd, vk::PipelineBindPoint pipelineBindPoint, vk::Pipeline pipeline),
                (const, override));
    MOCK_METHOD(void, RecordCopyBufferToImage,
                (vk::CommandBuffer cmd, vk::Buffer buffer, vk::Image image, vk::ImageLayout imageLayout,
                 const std::vector<vk::BufferImageCopy>& regions),
                (const, override));
    MOCK_METHOD(void, RecordBlitImage, (vk::CommandBuffer cmd, vk::BlitImageInfo2 blit), (const, override));
};

} // namespace Humongous
