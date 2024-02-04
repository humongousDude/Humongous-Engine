// README:
// Currently this main.cpp file serves as a temporary entrypoint into the engine.
// this is not be permanent
// and will be moved to a more appropriate location in the future

#include <fmt/core.h>
#include <vulkan_app.hpp>

int main()
{
    try
    {
        Humongous::VulkanApp app{};
        app.Run();
    }
    catch(const std::exception& e)
    {
        fmt::print(stderr, "Caught exception: %s\n", e.what());
    }
}
