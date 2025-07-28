#include "globals.hpp"
#include "defines.hpp"
#include "vector"
#include <algorithm>

namespace Humongous::Globals
{
f32 Time::Internal_AverageDeltaTime()
{
    const u8                averageCount{10};
    static std::vector<f32> past10Frames(averageCount);
    static u8               index{0};

    f32 currentDT = DeltaTime();

    if(index >= averageCount) { index = 0; }
    past10Frames[index] = currentDT;
    index++;

    f32 sum{0};
    for(const auto& a: past10Frames) { sum += a; }

    auto averageDT = sum / averageCount;
    averageDT = std::clamp(averageDT, 0.0f, 50.0f);

    return averageDT;
}

void Time::Internal_Update(f32 newDeltaTime)
{
    if(newDeltaTime <= 0) { return; }
    deltaTime = newDeltaTime;
    totalTime += newDeltaTime;
}
} // namespace Humongous::Globals
