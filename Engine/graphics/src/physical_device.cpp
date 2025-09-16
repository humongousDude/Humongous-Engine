#include "physical_device.hpp"
#include "logger.hpp"
#include "string"
#include "vector"
#include <vulkan/vulkan_to_string.hpp>

namespace Humongous
{
PhysicalDevice::PhysicalDevice(IInstance& instance, Window& window) : m_instance{instance}
{
    m_surface = window.CreateWindowSurface(m_instance.GetVkInstance());
    PickPhysicalDevice();
}

PhysicalDevice::~PhysicalDevice()
{
    vkDestroySurfaceKHR(m_instance.GetVkInstance(), m_surface, nullptr);
    HGINFO("Destroyed surface and let go of physical device handle");
}

void PhysicalDevice::PickPhysicalDevice()
{
    HGINFO("looking for a physical device...");
    uint32_t   deviceCount = 0;
    vk::Result result;

    result = m_instance.GetVkInstance().enumeratePhysicalDevices(&deviceCount, nullptr);
    if(result != vk::Result::eSuccess)
    {
        HGFATAL("Failed to get number of physical devices! Error: %s", vk::to_string(result).c_str());
        return;
    }
    if(deviceCount == 0)
    {
        HGFATAL("Failed to find GPUs with Vulkan support!");
        return;
    }
    HGINFO("found %d devices", deviceCount);

    std::vector<vk::PhysicalDevice> devices(deviceCount);
    result = m_instance.GetVkInstance().enumeratePhysicalDevices(&deviceCount, devices.data());
    if(result != vk::Result::eSuccess)
    {
        HGFATAL("Failed to enumerate physical devices! Error: %s", vk::to_string(result).c_str());
        return;
    }

    m_physicalDevice = VK_NULL_HANDLE;
    m_currentCapabilities = {};

    // Keep track of the best device found so far
    vk::PhysicalDevice bestPhysicalDevice = VK_NULL_HANDLE;
    DeviceSupportLevel bestSupportLevel = DeviceSupportLevel::BaseGraphics;
    DeviceCapabilities bestCapabilities = {};

    for(const auto& device: devices)
    {
        vk::PhysicalDeviceProperties deviceProperties = device.getProperties();
        HGINFO("Evaluating device: %s", deviceProperties.deviceName.data());

        DeviceCapabilities capabilities = GetDeviceCapabilities(device);
        DeviceSupportLevel currentLevel = EvaluateDeviceSupportLevel(capabilities);

        HGINFO("  Support Level: %s", [currentLevel]() { // Lambda for string conversion
            switch(currentLevel)
            {
                case DeviceSupportLevel::BaseGraphics:
                    return "Base Graphics";
                case DeviceSupportLevel::MeshShaders:
                    return "Mesh Shaders";
                default:
                    return "Unknown";
            }
        }());

        if(currentLevel >= bestSupportLevel)
        {
            bestSupportLevel = currentLevel;
            bestPhysicalDevice = device;
            bestCapabilities = capabilities;
        }

        // TODO: Scoring for multiple Mesh Shader capable devices
        if(bestSupportLevel == DeviceSupportLevel::MeshShaders) { break; }
    }

    m_physicalDevice = bestPhysicalDevice;
    m_currentSupportLevel = bestSupportLevel;
    m_currentCapabilities = bestCapabilities; // Save capabilities of the chosen device

    if(m_physicalDevice == VK_NULL_HANDLE)
    {
        HGFATAL("Failed to find a suitable GPU! No device meets even the minimum 'BaseGraphics' requirements.");
        return;
    }
    else
    {
        vk::PhysicalDeviceProperties chosenDeviceProperties = m_physicalDevice.getProperties();
        HGINFO("Selected Physical Device: %s (Support Level: %s)", chosenDeviceProperties.deviceName.data(), [bestSupportLevel]() {
            switch(bestSupportLevel)
            {
                case DeviceSupportLevel::BaseGraphics:
                    return "Base Graphics";
                case DeviceSupportLevel::MeshShaders:
                    return "Mesh Shaders";
                default:
                    return "Unknown";
            }
        }());
    }
}

PhysicalDevice::SwapChainSupportDetails PhysicalDevice::QuerySwapChainSupport(vk::PhysicalDevice physicalDevice) const
{
    vk::PhysicalDeviceSurfaceInfo2KHR surfaceInfo{};
    surfaceInfo.surface = m_surface;
    surfaceInfo.pNext = nullptr;

    SwapChainSupportDetails details{};
    if(physicalDevice.getSurfaceCapabilities2KHR(&surfaceInfo, &details.capabilities) != vk::Result::eSuccess)
    {
        HGFATAL("Failed to get surface capabilities!");
        return {};
    };

    u32  formatCount;
    auto result = physicalDevice.getSurfaceFormats2KHR(&surfaceInfo, &formatCount, nullptr);
    if(result != vk::Result::eSuccess)
    {
        HGFATAL("Failed to get surface format count! Error: %s", vk::to_string(result).c_str());
        return {};
    }

    if(formatCount != 0)
    {
        details.formats.resize(formatCount);

        result = physicalDevice.getSurfaceFormats2KHR(&surfaceInfo, &formatCount, details.formats.data());
        if(result != vk::Result::eSuccess)
        {
            HGFATAL("Failed to get surface formats! Error: %s", vk::to_string(result).c_str());
            return {};
        }
    }

    u32 presentModeCount;
    result = physicalDevice.getSurfacePresentModesKHR(m_surface, &presentModeCount, nullptr);
    if(result != vk::Result::eSuccess)
    {
        HGFATAL("Failed to acquire present mode count! Error: %s", vk::to_string(result).c_str());
        return {};
    }

    if(presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        result = physicalDevice.getSurfacePresentModesKHR(m_surface, &presentModeCount, details.presentModes.data());

        if(result != vk::Result::eSuccess)
        {
            HGFATAL("Failed to acquire present modes! Error: %s", vk::to_string(result).c_str());
            return {};
        }
    }

    return details;
}

b32 PhysicalDevice::CheckExtensionAvailability(vk::PhysicalDevice physicalDevice, const char* extensionName)
{
    uint32_t extensionCount;
    auto     res = physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr);
    if(res != vk::Result::eSuccess)
    {
        HGFATAL("Failed to acquire device extension count! Error: %s", vk::to_string(res).c_str());
        return false;
    }
    std::vector<vk::ExtensionProperties> availableExtensions(extensionCount);
    res = physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
    if(res != vk::Result::eSuccess)
    {
        HGFATAL("Failed to acquire device extension properties! Error: %s", vk::to_string(res).c_str());
        return false;
    }

    for(const auto& extension: availableExtensions)
    {
        if(strcmp(extension.extensionName, extensionName) == 0) { return true; }
    }
    return false;
}

template <typename T>
b32 PhysicalDevice::CheckPhysicalDeviceFeature(vk::PhysicalDevice physicalDevice, T& featuresStruct, std::function<b8(const T&)> featureCheck)
{
    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &featuresStruct;
    physicalDevice.getFeatures2(&features2);
    return featureCheck(featuresStruct);
}

PhysicalDevice::DeviceSupportLevel PhysicalDevice::EvaluateDeviceSupportLevel(const DeviceCapabilities& capabilities)
{
    if(capabilities.supportsMeshShaders) { return DeviceSupportLevel::MeshShaders; }

    return DeviceSupportLevel::BaseGraphics;
}

PhysicalDevice::DeviceCapabilities PhysicalDevice::GetDeviceCapabilities(vk::PhysicalDevice physicalDevice)
{
    DeviceCapabilities capabilities{};

    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);

    b8 allBaseExtensionsPresent = true;
    for(const char* ext: REQUIRED_BASE_DEVICE_EXTENSIONS)
    {
        if(!CheckExtensionAvailability(physicalDevice, ext))
        {
            HGINFO("Device %s is missing base extension: %s", physicalDevice.getProperties().deviceName.data(), ext);
            allBaseExtensionsPresent = false;
            break;
        }
    }

    if(!allBaseExtensionsPresent) { return capabilities; }
    m_presentExtensions = REQUIRED_BASE_DEVICE_EXTENSIONS;

    vk::PhysicalDeviceFeatures features{};
    physicalDevice.getFeatures(&features);
    capabilities.supportsSamplerAnisotropy = features.samplerAnisotropy;

    b8 supportsMeshShaderExtension = CheckExtensionAvailability(physicalDevice, VK_EXT_MESH_SHADER_EXTENSION_NAME);

    if(supportsMeshShaderExtension)
    {
        vk::PhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};

        b8 supportsTaskShaders = CheckPhysicalDeviceFeature<vk::PhysicalDeviceMeshShaderFeaturesEXT>(
            physicalDevice, meshShaderFeatures, [](const vk::PhysicalDeviceMeshShaderFeaturesEXT& f) { return f.taskShader; });

        b8 supportsMeshShaders = CheckPhysicalDeviceFeature<vk::PhysicalDeviceMeshShaderFeaturesEXT>(
            physicalDevice, meshShaderFeatures, [](const vk::PhysicalDeviceMeshShaderFeaturesEXT& f) { return f.meshShader; });

        capabilities.supportsMeshShaders = supportsTaskShaders && supportsMeshShaders;
        if(capabilities.supportsMeshShaders) { m_presentExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME); }
    }

    capabilities.supportsBindlessDescriptors = CheckExtensionAvailability(physicalDevice, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    if(capabilities.supportsBindlessDescriptors)
    {
        vk::PhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures{};
        capabilities.supportsBindlessDescriptors = CheckPhysicalDeviceFeature<vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>(
            physicalDevice, indexingFeatures,
            [](const vk::PhysicalDeviceDescriptorIndexingFeaturesEXT& f) { return f.descriptorBindingPartiallyBound && f.runtimeDescriptorArray; });
    }

    return capabilities;
}

PhysicalDevice::QueueFamilyData PhysicalDevice::FindQueueFamilies(vk::PhysicalDevice physicalDevice) const
{
    QueueFamilyData indices;
    u32             queueFamilyCount = 0;
    physicalDevice.getQueueFamilyProperties2(&queueFamilyCount, nullptr);
    std::vector<vk::QueueFamilyProperties2> queueFamilyProperties(queueFamilyCount);

    physicalDevice.getQueueFamilyProperties2(&queueFamilyCount, queueFamilyProperties.data());
    vk::Bool32 presentSupport = false;

    s32 i = 0;
    for(const auto& queueFamily: queueFamilyProperties)
    {
        if(queueFamily.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            if(!indices.graphicsFamily.has_value()) { indices.graphicsFamily = i; }
        }

        auto result = physicalDevice.getSurfaceSupportKHR(i, m_surface, &presentSupport);
        if(result != vk::Result::eSuccess) { HGFATAL("Failed to get surface support! Error: %s", vk::to_string(result).c_str()); }

        if(presentSupport)
        {
            if(!indices.presentFamily.has_value()) { indices.presentFamily = i; }
        }

        if(queueFamily.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eCompute)
        {
            if(!indices.computeFamily.has_value() || i != indices.graphicsFamily.value() && i != indices.transferFamily.value())
            {
                indices.computeFamily = i;
            }
            else if(!indices.computeFamily.has_value() || i != indices.graphicsFamily.value()) { indices.computeFamily = i; }
            else { indices.computeFamily = indices.graphicsFamily.value(); }
        }

        if(queueFamily.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eTransfer)
        {
            if(!indices.transferFamily.has_value() || (i != indices.graphicsFamily.value() && i != indices.computeFamily.value()))
            {
                indices.transferFamily = i;
            }
            else if(!indices.transferFamily.has_value() || i != indices.graphicsFamily.value()) { indices.transferFamily = i; }
            else { indices.transferFamily = indices.graphicsFamily.value(); }
        }

        i++;
    }

    if(!indices.computeFamily.has_value()) { indices.computeFamily = indices.graphicsFamily.value(); }
    if(!indices.transferFamily.has_value()) { indices.transferFamily = indices.graphicsFamily.value(); }

    HGINFO("Indices graphics: %d, present: %d, compute: %d, transfer: %d", indices.graphicsFamily.value(), indices.presentFamily.value(),
           indices.computeFamily.value(), indices.transferFamily.value());

    return indices;
}

} // namespace Humongous
