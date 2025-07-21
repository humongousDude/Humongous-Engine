#pragma once

#include "camera.hpp"

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
        const f32       frameTime;
        const Movements movementType;
        const f32&      mouseDeltaX;
        const f32&      mouseDeltaY;
        Camera&         camera;
    };

    void ProcessInput(const InputData& inputData);

    f32 lookSpeed = 0.5f;
    f32 moveSpeed = 25.0f;
};
} // namespace Humongous
