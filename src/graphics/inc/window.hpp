#pragma once

#include <non_copyable.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Humongous
{
class Window : NonCopyable
{
public:
    Window();
    ~Window();

    GLFWwindow* GetWindow() const { return window; }

    bool ShouldWindowClose() const { return glfwWindowShouldClose(window); }

    void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

    bool IsFocused() const { return glfwGetWindowAttrib(window, GLFW_FOCUSED); }
    bool IsMinimized() const { return glfwGetWindowAttrib(window, GLFW_ICONIFIED); }

    VkExtent2D GetExtent() const { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }

    bool WasWindowResized() const { return m_wasWindowResizedFlag; }
    void ResetWindowResizedFlag() { m_wasWindowResizedFlag = false; }

private:
    int width = 800, height = 600;

    GLFWwindow* window = nullptr;

    bool m_wasWindowResizedFlag;

    void CreateWindow();

    static void HandleWindowResized(GLFWwindow* window, int width, int height);
};
}; // namespace Humongous
