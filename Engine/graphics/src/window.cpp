#include "SDL3/SDL_vulkan.h"
#include "logger.hpp"
#include <window.hpp>

namespace Humongous
{
Window::Window() { CreateWindow(); }

Window::~Window()
{
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    HGINFO("Destroyed window and terminated GLFW");
}

void Window::HideCursor()
{
    SDL_SetWindowRelativeMouseMode(window, true);
    m_cursorHidden = true;
}

void Window::ShowCursor()
{
    SDL_SetWindowRelativeMouseMode(window, false);
    m_cursorHidden = false;
}

void Window::CreateWindow()
{
    if(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) != true)
    {
        HGFATAL("Failed to initalize SDL3! Error: %s", SDL_GetError());
    };
    if(!SDL_Vulkan_LoadLibrary(NULL)) { HGFATAL("Failed to load vulkan! Error: %s", SDL_GetError()); };
    if(!(window = SDL_CreateWindow("Humongous Window", width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE)))
    {
        HGFATAL("Failed to create SDL3 Window! Error: %s", SDL_GetError());
    };

    SDL_AddEventWatch(HandleWindowResized, this);
}

vk::SurfaceKHR Window::CreateWindowSurface(vk::Instance instance)
{
    VkSurfaceKHR a;
    if(!SDL_Vulkan_CreateSurface(window, instance, nullptr, &a)) { HGFATAL("Failed to create window surface"); }
    HGINFO("Created window surface");
    return a;
}

bool Window::HandleWindowResized(void* userdata, SDL_Event* event)
{
    if(event->type != SDL_EVENT_WINDOW_RESIZED) { return false; }
    auto self = reinterpret_cast<Window*>(userdata);
    self->m_wasWindowResizedFlag = true;
    self->width = event->window.data1;
    self->height = event->window.data2;
    HGINFO("Window Resized");
    return true;
}

} // namespace Humongous
