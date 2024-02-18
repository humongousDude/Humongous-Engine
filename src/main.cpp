// README:
// Currently this main.cpp file serves as a temporary entrypoint into the engine.
// this is not be permanent
// and will be moved to a more appropriate location in the future
//

#include "logger.hpp"
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
        HGFATAL("Exception occurred: %s", e.what());
    }
}
