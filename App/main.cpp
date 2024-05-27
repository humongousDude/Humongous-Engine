// README:
// Currently this main.cpp file serves as a temporary entrypoint into the engine.
// this is not permanent
// and will be moved to a more appropriate location in the future

#include <vulkan_app.hpp>

int main(int argc, char* argv[])
{
    Humongous::VulkanApp* app = new Humongous::VulkanApp(argc, argv);
    app->Run();
}
