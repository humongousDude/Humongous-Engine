#pragma once

#include "entity_component.hpp"
#include <string>

namespace Humongous
{
struct NameComponent : EntityComponent
{
    std::string name;
};
} // namespace Humongous
