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

private:
    GLFWwindow* window = nullptr;

    void CreateWindow();
};
}; // namespace Humongous
