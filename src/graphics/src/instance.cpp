#include <cstring>
#include <fmt/core.h>
#include <instance.hpp>
#include <logger.hpp>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

namespace Humongous
{
// TODO: move this + the rest of the debug related stuff
// to a utils file or maybe to core

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    switch(messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            HGERROR("VALIDATION TYPE: %s\n VALIDATION MESSAGE: %s\n", string_VkDebugUtilsMessageTypeFlagsEXT(messageType).c_str(),
                    pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            HGWARN("VALIDATION TYPE: %s\n VALIDATION MESSAGE: %s\n", string_VkDebugUtilsMessageTypeFlagsEXT(messageType).c_str(),
                   pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            HGINFO("VALIDATION TYPE: %s\n VALIDATION MESSAGE: %s\n", string_VkDebugUtilsMessageTypeFlagsEXT(messageType).c_str(),
                   pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        default:
            break;
    }

    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance m_instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if(func != nullptr) { return func(m_instance, pCreateInfo, pAllocator, pDebugMessenger); }
    else { return VK_ERROR_EXTENSION_NOT_PRESENT; }
}

void DestroyDebugUtilsMessengerEXT(VkInstance m_instance, VkDebugUtilsMessengerEXT m_debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
    if(func != nullptr) { func(m_instance, m_debugMessenger, pAllocator); }
}

Instance::Instance()
{
    InitInstance();
    SetupDebugMessenger();
}

Instance::~Instance()
{
    if(ENABLE_VALIDATION_LAYERS) { DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr); }

    vkDestroyInstance(m_instance, nullptr);
    HGINFO("Destroyed Vulkan Instance");
}

void Instance::InitInstance()
{
    if(ENABLE_VALIDATION_LAYERS && !CheckValidationLayerSupport()) { HGERROR("Validation layers requested, but not available!"); }

    HGINFO("Initializing Vulkan Instance!");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanOnMyOwn";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Humongous";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if(ENABLE_VALIDATION_LAYERS)
    {
        createInfo.enabledLayerCount = static_cast<u32>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();

        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    auto extensions = GetRequiredExtensions();

    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    u32 extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());

    fmt::print("Available extensions:\n");

    for(const auto& extension: extensionProperties) { fmt::print("\t{}\n", extension.extensionName); }

    if(vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
    {
        HGFATAL("Failed to create vulkan instance! \nFile: %s, \nLine: %d", __FILE__, __LINE__);
    }
}

bool Instance::CheckValidationLayerSupport()
{
    u32 layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for(const char* layerName: m_validationLayers)
    {
        bool layerFound = false;

        for(const auto& layerProperties: availableLayers)
        {
            if(strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if(!layerFound) { return false; }
    }

    return true;
}

std::vector<const char*> Instance::GetRequiredExtensions()
{
    u32          glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if(ENABLE_VALIDATION_LAYERS) { extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); }
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

    return extensions;
}

void Instance::SetupDebugMessenger()
{
    if(!ENABLE_VALIDATION_LAYERS) { return; }
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    PopulateDebugMessengerCreateInfo(createInfo);
    if(CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
    {
        HGFATAL("Failed to set up debug messenger!");
    }
}

void Instance::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

} // namespace Humongous
