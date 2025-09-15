#pragma once

#include "defines.hpp"
#include "non_copyable.hpp"
#include "singleton.hpp"

namespace Humongous
{
namespace Globals
{

// I am unsure where I should put this
enum class ModelDescriptorIndices : u32
{
    Camera = 0, // Camera gets 1 descriptor set
    Model = 1,  // Model gets 2 descriptor sets

    // TODO: This should probably not be here?
    Debug = 3, // Debug gets 1 descriptor set
};

enum class StencilMasks : u32
{
    Nothing = 0,
    Model = 1,
};

// TODO: Replace with config loaded from file
enum class Limits : u32
{
    MaxFramesInFlight = 2,
    MaximumRenderDistance = 200,
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
