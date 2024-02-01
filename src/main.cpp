#include <fmt/core.h>
#include <vulkan_app.hpp>

int main()
{
    try
    {
        Humongous::VulkanApp app;
    }
    catch(const std::exception&)
    {
        fmt::print(stderr, "Fatal error!\n");
    }
}
