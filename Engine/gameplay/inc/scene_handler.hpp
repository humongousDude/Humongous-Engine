#pragma once

#include "singleton.hpp"
#include "world.hpp"

namespace Humongous
{
class SceneHandler : Singleton<SceneHandler>
{
public:
    static void Init() { Get().Internal_Init(); };

    static void LoadScene() {
        // Save whatever data that needs saving
        // Unload the current scene, world, etc...
        // Load the required scene, take its data and reconstruct World
    };

    static World* GetWorld() { return &Get().m_world; }

private:
    World m_world{};

    void Internal_Init() {};
};
} // namespace Humongous
