#include "logger.hpp"
#include <iostream>
#include <window.hpp>

namespace Humongous
{
Window::Window() { CreateWindow(); }

Window::~Window()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Window::CreateWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
}

void Window::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
    if(glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) { HGFATAL("Failed to create window surface"); }
    HGINFO("Created window surface");
}

} // namespace Humongous
