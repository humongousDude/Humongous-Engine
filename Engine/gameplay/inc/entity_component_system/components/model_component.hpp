#pragma once

#include "defines.hpp"
#include "entity_component_system/components/entity_component.hpp"
#include <model_instance.hpp>

namespace Humongous
{
struct ModelComponent : public EntityComponent
{
    std::shared_ptr<ModelInstance> instance;
};
} // namespace Humongous
