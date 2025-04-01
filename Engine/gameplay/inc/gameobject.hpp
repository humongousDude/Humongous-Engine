#pragma once

#include "material.hpp"
#include <defines.hpp>

#include <model.hpp>

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>

namespace Humongous
{
struct TransformComponent
{
    glm::vec3 translation{}; // position offset;
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::vec3 rotation{};

    glm::mat4 Mat4() const;
    glm::mat3 NormalMatrix();

    bool operator==(const TransformComponent& other)
    {
        return translation == other.translation && scale == other.scale && rotation == other.rotation;
    }
};

class GameObject
{
public:
    struct UpdateData
    {
        const glm::mat4& pvm;
        const n16&       screenWidth = 800;
        const n16&       screenHeight = 600;
    };

    struct BoundingData
    {
        BoundingBox totalBB{};
        BoundingBox modelBB{};
    };

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

    const id_t GetId() { return m_id; };

    glm::vec3          color{};
    TransformComponent transform{};

    void         SetModel(std::shared_ptr<Model> model);
    void         Update();
    BoundingData GetBoundingData() const { return m_boundingBoxData; }
    BoundingBox  GetBoundingBox() const { return m_worldBounds; }

    std::shared_ptr<Model> model{};

    std::string name;

private:
    id_t m_id;

    GameObject(id_t objId) : m_id{objId} {};

    BoundingData       m_boundingBoxData{};
    BoundingBox        m_worldBounds{};
    TransformComponent m_prevFrameTransform{};

    void UpdateBoundingData();
};

} // namespace Humongous
