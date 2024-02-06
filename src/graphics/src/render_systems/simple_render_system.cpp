#include <render_systems/simple_render_system.hpp>

namespace Humongous
{
SimpleRenderSystem::SimpleRenderSystem(LogicalDevice& logicalDevice) : m_logicalDevice(logicalDevice) {}

SimpleRenderSystem::~SimpleRenderSystem() {}
} // namespace Humongous
