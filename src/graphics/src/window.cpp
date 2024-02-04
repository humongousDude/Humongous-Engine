#include "logger.hpp"
#include <window.hpp>

namespace Humongous
{
Window::Window() { CreateWindow(); }

Window::~Window()
{
    glfwDestroyWindow(window);
    glfwTerminate();
    HGINFO("Destroyed window and terminated GLFW");
}

void Window::CreateWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
}

void Window::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
    if(glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) { HGFATAL("Failed to create window surface"); }
    HGINFO("Created window surface");
}

} // namespace Humongous
