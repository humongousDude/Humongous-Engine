#pragma once

#include "non_copyable.hpp"
#include <vulkan/vulkan_core.h>

namespace Humongous
{
class SwapChain : NonCopyable
{
public:
    SwapChain();
    ~SwapChain();

private:
    VkSwapchainKHR m_swapChain;

    VkSwapchainKHR* m_oldSwap = nullptr;
};
} // namespace Humongous
