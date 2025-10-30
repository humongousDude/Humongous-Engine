#pragma once

#include "defines.hpp"
#include "non_copyable.hpp"
#include "vulkan/vulkan.hpp"
#include <SDL3/SDL.h>

namespace Humongous
{
class Window : NonCopyable
{
public:
    Window();
    ~Window();

    SDL_Window* GetWindow() const { return window; }

    // b8 ShouldWindowClose() const { return glfwWindowShouldClose(window); }

    vk::SurfaceKHR CreateWindowSurface(vk::Instance instance);

    b8 IsFocused() const { return SDL_GetWindowFlags(window) == SDL_WINDOW_INPUT_FOCUS; }
    b8 IsMinimized() const { return SDL_GetWindowFlags(window) == SDL_WINDOW_MAXIMIZED; }

    vk::Extent2D GetExtent() const { return {static_cast<u32>(width), static_cast<u32>(height)}; }

    b8   WasWindowResized() const { return m_wasWindowResizedFlag; }
    void ResetWindowResizedFlag() { m_wasWindowResizedFlag = false; }

    void HideCursor();
    void ShowCursor();

    b8 IsCursorHidden() { return m_cursorHidden; }

private:
    int width = 1280, height = 720;
    b8  m_cursorHidden = false;

    SDL_Window* window = nullptr;

    b8 m_wasWindowResizedFlag;

    void CreateWindow();

    static bool HandleWindowResized(void* userdata, SDL_Event* event);
};
}; // namespace Humongous
