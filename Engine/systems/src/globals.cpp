#include "globals.hpp"
#include "defines.hpp"
#include "vector"

namespace Humongous::Globals
{
float Time::Internal_AverageDeltaTime()
{
    const u8                  averageCount{10};
    static std::vector<float> past10Frames(averageCount);
    static u8                 index{0};

    float currentDT = DeltaTime();

    if(index >= averageCount) { index = 0; }
    past10Frames[index] = currentDT;
    index++;

    float sum{0};
    for(const auto& a: past10Frames) { sum += a; }

    return sum / averageCount;
}
} // namespace Humongous::Globals
