#pragma once

#include <non_copyable.hpp>

#include "SDL3/SDL.h"
#include "defines.hpp"
#include "vulkan/vulkan.hpp"

namespace Humongous
{
class Window : NonCopyable
{
public:
    Window();
    ~Window();

    SDL_Window* GetWindow() const { return window; }

    // bool ShouldWindowClose() const { return glfwWindowShouldClose(window); }

    vk::SurfaceKHR CreateWindowSurface(vk::Instance instance);

    bool IsFocused() const { return SDL_GetWindowFlags(window) == SDL_WINDOW_INPUT_FOCUS; }
    bool IsMinimized() const { return SDL_GetWindowFlags(window) == SDL_WINDOW_MAXIMIZED; }

    vk::Extent2D GetExtent() const { return {static_cast<n32>(width), static_cast<n32>(height)}; }

    bool WasWindowResized() const { return m_wasWindowResizedFlag; }
    void ResetWindowResizedFlag() { m_wasWindowResizedFlag = false; }

    void HideCursor();
    void ShowCursor();

    bool IsCursorHidden() { return m_cursorHidden; }

private:
    int  width = 800, height = 600;
    bool m_cursorHidden = false;

    SDL_Window* window = nullptr;

    bool m_wasWindowResizedFlag;

    void CreateWindow();

    static bool HandleWindowResized(void* userdata, SDL_Event* event);
};
}; // namespace Humongous
