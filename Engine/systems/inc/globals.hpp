#pragma once

#include "singleton.hpp"
namespace Humongous
{
namespace Globals
{

class Time : Singleton<Time>
{
public:
    static float DeltaTime() { return Get().Internal_DeltaTime(); }
    static void  Update(float deltaTime) { Get().UpdateDeltaTime(deltaTime); }

private:
    float Internal_DeltaTime() { return deltaTime; }
    void  UpdateDeltaTime(float newDeltaTime) { deltaTime = newDeltaTime; }

    float deltaTime;
};

} // namespace Globals
} // namespace Humongous
