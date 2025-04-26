#pragma once

#include "defines.hpp"
#include "non_copyable.hpp"
#include "singleton.hpp"

namespace Humongous
{
namespace Globals
{

// I am unsure where I should put this
enum class DescriptorSetIndices : n32
{
    Camera = 0, // Camera gets 1 descriptor set
    Scene = 1,  // Scene gets 1 descriptor set
    Skybox = 2,
    Model = 3, // Model gets 4 descriptor sets
    Debug = 7, // Debug gets 1 descriptor set
};

class Time : public Singleton<Time>, NonCopyable
{
public:
    static f32  DeltaTime() { return Get().Internal_DeltaTime(); }
    static f32  AverageDeltaTime() { return Get().Internal_AverageDeltaTime(); }
    static f32  TimeSinceStart() { return Get().Internal_TimeSinceStart(); }
    static void Update(f32 deltaTime) { Get().Internal_Update(deltaTime); }

private:
    f32  Internal_DeltaTime() { return deltaTime; }
    f32  Internal_AverageDeltaTime();
    void Internal_Update(f32 newDeltaTime);
    f32  Internal_TimeSinceStart() { return totalTime; };

    f32 deltaTime{0};
    f32 totalTime{0};
};

} // namespace Globals
} // namespace Humongous
