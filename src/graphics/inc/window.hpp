#pragma once

#include <non_copyable.hpp>

#include <GLFW/glfw3.h>

namespace Humongous
{
class Window : NonCopyable
{
public:
    Window();
    ~Window();

    GLFWwindow* GetWindow() const { return window; }

private:
    GLFWwindow* window = nullptr;

    void CreateWindow();
};
}; // namespace Humongous
