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

void Window::HideCursor()
{
    glfwSetCursorPos(window, width / 2.f, height / 2.f);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    m_cursorHidden = true;
}

void Window::ShowCursor()
{
    glfwSetCursorPos(window, width / 2.f, height / 2.f);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    m_cursorHidden = false;
}

void Window::CreateWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, "Humongous Engine", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetWindowSizeCallback(window, HandleWindowResized);
}

vk::SurfaceKHR Window::CreateWindowSurface(vk::Instance instance)
{
    VkSurfaceKHR a;
    if(glfwCreateWindowSurface(instance, window, nullptr, &a) != VK_SUCCESS) { HGFATAL("Failed to create window surface"); }
    HGINFO("Created window surface");
    return a;
}

void Window::HandleWindowResized(GLFWwindow* window, int width, int height)
{
    auto self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    self->m_wasWindowResizedFlag = true;
    self->width = width;
    self->height = height;
}

} // namespace Humongous
