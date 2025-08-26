#include "physical_device.hpp"
#include "logger.hpp"
#include "string"
#include "vector"
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_to_string.hpp>

// FIXME: No discard warnings

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
        HGFATAL("Failed to get number of physical devices! Error: %s", string_VkResult(static_cast<VkResult>(result)));
    }
    if(deviceCount == 0) { HGFATAL("Failed to find GPUs with Vulkan support!"); }
    HGINFO("found %d devices", deviceCount);

    std::vector<vk::PhysicalDevice> devices(deviceCount);
    result = m_instance.GetVkInstance().enumeratePhysicalDevices(&deviceCount, devices.data());
    if(result != vk::Result::eSuccess)
    {
        HGFATAL("Failed to enumerate physical devices! Error: %s", string_VkResult(static_cast<VkResult>(result)));
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

    m_currentCapabilities.supportsMeshShaders = false;
    m_currentSupportLevel = DeviceSupportLevel::BaseGraphics;
    ;

    if(m_physicalDevice == VK_NULL_HANDLE)
    {
        HGFATAL("Failed to find a suitable GPU! No device meets even the minimum 'BaseGraphics' requirements.");
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
    };

    u32  formatCount;
    auto result = physicalDevice.getSurfaceFormats2KHR(&surfaceInfo, &formatCount, nullptr);
    if(result != vk::Result::eSuccess) { HGFATAL("Failed to get surface format count! Error: %s", vk::to_string(result).c_str()); }

    if(formatCount != 0)
    {
        details.formats.resize(formatCount);
        // FIXME: this is probably not a good way to set the sType, but I can't figure out another way
        for(int i = 0; i < formatCount; i++) { details.formats[i].sType = vk::StructureType::eSurfaceFormat2KHR; }

        result = physicalDevice.getSurfaceFormats2KHR(&surfaceInfo, &formatCount, details.formats.data());
        if(result != vk::Result::eSuccess) { HGFATAL("Failed to get surface formats! Error: %s", vk::to_string(result).c_str()); }
    }

    u32 presentModeCount;
    result = physicalDevice.getSurfacePresentModesKHR(m_surface, &presentModeCount, nullptr);
    if(result != vk::Result::eSuccess) { HGFATAL("Failed to acquire present mode count! Error: %s", vk::to_string(result).c_str()); }

    if(presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        result = physicalDevice.getSurfacePresentModesKHR(m_surface, &presentModeCount, details.presentModes.data());

        if(result != vk::Result::eSuccess) { HGFATAL("Failed to acquire present modes! Error: %s", vk::to_string(result).c_str()); }
    }

    return details;
}

b32 PhysicalDevice::CheckExtensionAvailability(vk::PhysicalDevice physicalDevice, const char* extensionName)
{
    uint32_t extensionCount;
    physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<vk::ExtensionProperties> availableExtensions(extensionCount);
    physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    for(const auto& extension: availableExtensions)
    {
        if(strcmp(extension.extensionName, extensionName) == 0) { return true; }
    }
    return false;
}

bool PhysicalDevice::IsDeviceSuitable(vk::PhysicalDevice physicalDevice)
{
    vk::PhysicalDeviceProperties2 deviceProperties{};
    physicalDevice.getProperties2(&deviceProperties);
    vk::PhysicalDeviceFeatures2 deviceFeatures{};
    physicalDevice.getFeatures2(&deviceFeatures);

    bool haveAllRequiredIndices = FindQueueFamilies(physicalDevice).IsComplete();
    bool deviceHasExtensions = CheckDeviceExtensionSupport(physicalDevice);

    bool swapChainAdequate = false;
    if(deviceHasExtensions)
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    if(haveAllRequiredIndices && deviceHasExtensions && swapChainAdequate)
    {
        HGINFO("device is suitable");
        HGINFO("Device: %s", deviceProperties.properties.deviceName.data());
    }

    return haveAllRequiredIndices && deviceHasExtensions && swapChainAdequate;
}

template <typename T>
b32 PhysicalDevice::CheckPhysicalDeviceFeature(vk::PhysicalDevice physicalDevice, T& featuresStruct, std::function<bool(const T&)> featureCheck)
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

    bool allBaseExtensionsPresent = true;
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

bool PhysicalDevice::CheckDeviceExtensionSupport(vk::PhysicalDevice physicalDevice)
{
    u32  extensionCount;
    auto result = physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr);

    if(result != vk::Result::eSuccess) { HGFATAL("Couldn't acquire device extension count! Error: %s", vk::to_string(result).c_str()); }

    std::vector<vk::ExtensionProperties> availableExtensions(extensionCount);
    result = physicalDevice.enumerateDeviceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
    if(result != vk::Result::eSuccess) { HGFATAL("Couldn't acquire device extension properties! Error: %s", vk::to_string(result).c_str()); }

    // std::set<std::string> requiredExtensions(D.begin(), deviceExtensions.end());

    HGDEBUG("%d extensions avablialbi", extensionCount);
    return 1;
    // for(const auto& extension: availableExtensions) { requiredExtensions.erase(extension.extensionName); }
    //
    // for(const auto& extension: requiredExtensions) { HGINFO("Missing extension: %s", extension.c_str()); }
    //
    // return requiredExtensions.empty();
}

PhysicalDevice::QueueFamilyData PhysicalDevice::FindQueueFamilies(vk::PhysicalDevice physicalDevice) const
{
    QueueFamilyData indices;
    u32             queueFamilyCount = 0;
    physicalDevice.getQueueFamilyProperties2(&queueFamilyCount, nullptr);
    std::vector<vk::QueueFamilyProperties2> queueFamilyProperties(queueFamilyCount);
    for(auto& queueFamilyProperty: queueFamilyProperties) { queueFamilyProperty.sType = vk::StructureType::eQueueFamilyProperties2; }
    physicalDevice.getQueueFamilyProperties2(&queueFamilyCount, queueFamilyProperties.data());
    vk::Bool32 presentSupport = false;

    int i = 0;
    for(const auto& queueFamily: queueFamilyProperties)
    {
        if(queueFamily.queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) { indices.graphicsFamily = i; }
        auto result = physicalDevice.getSurfaceSupportKHR(i, m_surface, &presentSupport);
        if(result != vk::Result::eSuccess) { HGFATAL("Failed to get surface support! Error: %s", vk::to_string(result).c_str()); }
        if(presentSupport) { indices.presentFamily = i; }
        i++;
    }
    HGINFO("Indices graphics: %d, present: %d", indices.graphicsFamily.value(), indices.presentFamily.value());

    return indices;
}

} // namespace Humongous
