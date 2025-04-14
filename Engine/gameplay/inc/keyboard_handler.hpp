#pragma once

#include <gameobject.hpp>

namespace Humongous
{
/*
 taken from brendan galea's vulkan tutorial series.
 im bad at matrix math. so please excuse the fact that this
 is copied
*/
class KeyboardHandler
{
public:
    enum class Movements
    {
        FORWARD,
        FORWARD_LEFT,
        FORWARD_RIGHT,
        BACKWARD,
        BACKWARD_LEFT,
        BACKWARD_RIGHT,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        NONE,
    };

    struct InputData
    {
        float     frameTime;
        Movements movementType;
        float&    mouseDeltaX;
        float&    mouseDeltaY;
        Camera&   camera;
    };

    void ProcessInput(const InputData& inputData);

    float lookSpeed = 0.5f;
    float moveSpeed = 25.0f;
};
} // namespace Humongous
