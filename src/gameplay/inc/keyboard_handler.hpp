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
    enum class Movements
    {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        NONE,
    };

    struct InputData
    {
        float       frameTime;
        GameObject& gameObject;
        Movements   movementType;
        double&     mouseDeltaX;
        double&     mouseDeltaY;
    };

    void ProcessInput(const InputData& inputData);

    float moveSpeed{3.0f};
    float lookSpeed{.1f};
};
} // namespace Humongous
