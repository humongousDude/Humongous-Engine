#pragma once

#include <asserts.hpp>
#include <defines.hpp>
#include <non_copyable.hpp>
#include <vector>

#include <GLFW/glfw3.h>
// #include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>

namespace Humongous
{

class Instance : NonCopyable
{
public:
    Instance();
    ~Instance();

    vk::Instance GetVkInstance() const { return m_instance; };

    bool                     IsValidationLayerEnabled() const { return ENABLE_VALIDATION_LAYERS; };
    std::vector<const char*> GetValidationLayers() const { return m_validationLayers; };

private:
    vk::Instance m_instance;

    // dunno how to use the CPP binding for this
    VkDebugUtilsMessengerEXT       m_debugMessenger{VK_NULL_HANDLE};
    const std::vector<const char*> m_validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifndef _DEBUG
    const bool ENABLE_VALIDATION_LAYERS = false;
#else
    const bool ENABLE_VALIDATION_LAYERS = true;
#endif

    void                     InitInstance();
    bool                     CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();

    void SetupDebugMessenger();
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
};

} // namespace Humongous
