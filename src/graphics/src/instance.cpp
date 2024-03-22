#include <instance.hpp>
#include <logger.hpp>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

namespace Humongous
{
// TODO: move this + the rest of the debug related stuff
// to a utils file or maybe to core

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                                    vk::DebugUtilsMessageTypeFlagsEXT             messageType,
                                                    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    switch(messageSeverity)
    {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
            HGERROR("VALIDATION TYPE: %s\n VALIDATION MESSAGE: %s\n", vk::to_string(messageType).c_str(), pCallbackData->pMessage);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            HGWARN("VALIDATION TYPE: %s\n VALIDATION MESSAGE: %s\n", vk::to_string(messageType).c_str(), pCallbackData->pMessage);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
            HGINFO("VALIDATION TYPE: %s\n VALIDATION MESSAGE: %s\n", vk::to_string(messageType).c_str(), pCallbackData->pMessage);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
        default:
            break;
    }

    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if(func != nullptr) { return func(instance, pCreateInfo, pAllocator, pDebugMessenger); }
    else { return VK_ERROR_EXTENSION_NOT_PRESENT; }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if(func != nullptr) { func(instance, debugMessenger, pAllocator); }
}

Instance::Instance()
{
    InitInstance();
    HGDEBUG("GOT HERE %d, %s", __LINE__, __FILE__);
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

    vk::ApplicationInfo appInfo{};
    appInfo.sType = vk::StructureType::eApplicationInfo;
    appInfo.pApplicationName = "HumongousEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Humongous";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo createInfo{};
    createInfo.sType = vk::StructureType::eInstanceCreateInfo;
    createInfo.pApplicationInfo = &appInfo;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if(!ENABLE_VALIDATION_LAYERS)
    {
        createInfo.enabledLayerCount = static_cast<u32>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();

        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if(debugCreateInfo.pfnUserCallback != nullptr) { HGDEBUG("me no sad"); }

    auto extensions = GetRequiredExtensions();

    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    u32 extensionCount = 0;

    vk::Result res = vk::enumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if(res != vk::Result::eSuccess) {}

    std::vector<vk::ExtensionProperties> extensionProperties(extensionCount);

    res = vk::enumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());

    HGINFO("Available extensions:\n");

    for(const auto& extension: extensionProperties) { HGINFO("\t%s", extension.extensionName); }

    if(vk::createInstance(&createInfo, nullptr, &m_instance) != vk::Result::eSuccess)
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
    const char** glfwExtensions = nullptr;
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
    HGDEBUG("got here :( %d, %s", __LINE__, __func__);

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    PopulateDebugMessengerCreateInfo(createInfo);

    // if(CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
    // {
    //     HGFATAL("Failed to set up debug messenger!");
    // }
}

void Instance::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.flags = 0;
    createInfo.pNext = nullptr;
    createInfo.pUserData = nullptr;
    // createInfo.pfnUserCallback = DebugCallback;
}

} // namespace Humongous
