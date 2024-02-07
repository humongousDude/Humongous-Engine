#pragma once

#include <defines.hpp>

#include <model.hpp>

// libs
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>

namespace Humongous
{
struct TransformComponent
{
    glm::vec3 translation{}; // position offset;
    glm::vec3 scale{1.0f, 1.0f, 1.f};
    glm::vec3 rotation{};

    glm::mat4 Mat4();
    glm::mat3 NormalMatrix();
};

struct RigidBodyComponent
{
    glm::vec2 velocity;
    float     mass{1.0f};
};

class GameObject
{
public:
    using id_t = unsigned int;
    using Map = std::unordered_map<id_t, GameObject>;

    static GameObject CreateGameObject()
    {
        static id_t currentId = 0;
        return GameObject{currentId++};
    }

    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&&) = default;
    GameObject& operator=(GameObject&&) = default;

    const id_t GetId() { return id; };

    std::shared_ptr<Model> model{};
    glm::vec3              color{};
    TransformComponent     transform{};
    RigidBodyComponent     rigidBody{};

private:
    id_t id;

    GameObject(id_t objId) : id{objId} {};
};

} // namespace Humongous
