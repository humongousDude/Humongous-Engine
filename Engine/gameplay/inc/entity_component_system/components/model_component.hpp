#pragma once

#include "defines.hpp"
#include "entity_component_system/components/entity_component.hpp"

namespace Humongous
{
struct ModelComponent : public EntityComponent
{
    n32 modelHandle;
};
} // namespace Humongous
