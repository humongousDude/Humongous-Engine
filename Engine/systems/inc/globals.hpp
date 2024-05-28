#pragma once

#include "non_copyable.hpp"
#include "singleton.hpp"
namespace Humongous
{
namespace Globals
{

class Time : public Singleton<Time>, NonCopyable
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
