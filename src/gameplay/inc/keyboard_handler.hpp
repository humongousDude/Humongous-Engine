#pragma once

#include <gameobject.hpp>

namespace Humongous
{
/*
 taken from brendan galea's vulkan tutorial series.
 im shit at matrix math. so please excuse the fact that this
 is copied
*/
class KeyboardHandler
{
public:
    struct KeyMappings
    {
        int moveLeft = GLFW_KEY_A;
        int moveRight = GLFW_KEY_D;
        int moveForward = GLFW_KEY_W;
        int moveBackward = GLFW_KEY_S;
        int moveUp = GLFW_KEY_E;
        int moveDown = GLFW_KEY_Q;
        int lookLeft = GLFW_KEY_LEFT;
        int lookRight = GLFW_KEY_RIGHT;
        int lookUp = GLFW_KEY_UP;
        int lookDown = GLFW_KEY_DOWN;
    };

    void MoveInPlaneXZ(GLFWwindow* window, float dt, GameObject& gameObject);

    KeyMappings keys{};
    float       moveSpeed{3.0f};
    float       lookSpeed{1.5f};
};
} // namespace Humongous
