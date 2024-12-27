#include "globals.hpp"
#include "defines.hpp"
#include "vector"

namespace Humongous::Globals
{
f32 Time::Internal_AverageDeltaTime()
{
    const n8                averageCount{10};
    static std::vector<f32> past10Frames(averageCount);
    static n8               index{0};

    f32 currentDT = DeltaTime();

    if(index >= averageCount) { index = 0; }
    past10Frames[index] = currentDT;
    index++;

    f32 sum{0};
    for(const auto& a: past10Frames) { sum += a; }

    return sum / averageCount;
}

void Time::Internal_Update(f32 newDeltaTime)
{
    if(newDeltaTime <= 0) { return; }
    deltaTime = newDeltaTime;
    totalTime += newDeltaTime;
}
} // namespace Humongous::Globals
