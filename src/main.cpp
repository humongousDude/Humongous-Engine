// README:
// Currently this main.cpp file serves as a temporary entrypoint into the engine.
// this is not be permanent
// and will be moved to a more appropriate location in the future

#include <vulkan_app.hpp>

int main()
{
    Humongous::VulkanApp app{};
    app.Run();
}
