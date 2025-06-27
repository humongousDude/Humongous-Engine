#pragma once

#include "defines.hpp"
#include "entity_component_system/components/entity_component.hpp"
#include "logger.hpp"

#include <string>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/fwd.hpp>
#include <glm/glm.hpp>

namespace Humongous
{
struct Node;

// EntityComponent doesn't add anything
struct alignas(16) BoundingBox : public EntityComponent
{
    glm::vec4 min{std::numeric_limits<f32>::max()};
    glm::vec4 max{std::numeric_limits<f32>::min()};
    s32       valid{false};

    static BoundingBox LocalToGlobal(const BoundingBox& localBoundingBox, const glm::mat4& model);
    static BoundingBox TransformAABB(const BoundingBox& aabb, const glm::mat4& transform)
    {
        if(!aabb.valid) { return BoundingBox{}; }

        glm::vec4 newMin = glm::vec4(std::numeric_limits<float>::max());
        glm::vec4 newMax = glm::vec4(std::numeric_limits<float>::lowest());

        // Generate and transform all 8 corners
        glm::vec3 corners[8] = {glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z),
                                glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z),
                                glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z), glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
                                glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z), glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z)};

        for(int i = 0; i < 8; ++i)
        {
            glm::vec4 transformedCorner = glm::vec4(transform * glm::vec4(corners[i], 1.0f));
            newMin = glm::min(newMin, transformedCorner);
            newMax = glm::max(newMax, transformedCorner);
        }

        return BoundingBox{newMin, newMax};
    }

    void Extend(const glm::vec4& point)
    {
        if(!valid)
        {
            min = point;
            max = point;
            valid = true;
        }
        else
        {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }
    }

    // Method to extend with another bounding box
    void Extend(const BoundingBox& other)
    {
        if(!other.valid) { return; }
        if(!valid)
        {
            *this = other;
            return;
        }
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    void Invalidate()
    {
        min = glm::vec4(std::numeric_limits<f32>::max());
        max = glm::vec4(std::numeric_limits<f32>::min());
        valid = false;
    };

    BoundingBox() = default;
    BoundingBox(glm::vec4 min, glm::vec4 max) : min(min), max(max), valid(true) {}
};

struct Material
{
    enum AlphaMode
    {
        ALPHAMODE_OPAQUE,
        ALPHAMODE_MASK,
        ALPHAMODE_BLEND
    };
    AlphaMode alphaMode = ALPHAMODE_OPAQUE;
    float     alphaCutoff = 1.0f;
    float     metallicFactor = 1.0f;
    float     roughnessFactor = 1.0f;
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    glm::vec4 emissiveFactor = glm::vec4(0.0f);
    n32       baseColorTextureIndex = -1;
    n32       metallicRoughnessTextureIndex = -1;
    n32       normalTextureIndex = -1;
    n32       occlusionTextureIndex = -1;
    n32       emissiveTextureIndex = -1;
    bool      doubleSided = false;
    struct TexCoordSets
    {
        uint8_t baseColor = 0;
        uint8_t metallicRoughness = 0;
        uint8_t specularGlossiness = 0;
        uint8_t normal = 0;
        uint8_t occlusion = 0;
        uint8_t emissive = 0;
    } texCoordSets;
    struct Extension
    {
        n32       specularGlossinessTextureIndex = -1;
        n32       diffuseTextureIndex = -1;
        glm::vec4 diffuseFactor = glm::vec4(1.0f);
        glm::vec3 specularFactor = glm::vec3(0.0f);
    } extension;
    struct PbrWorkflows
    {
        bool metallicRoughness = true;
        bool specularGlossiness = false;
    } pbrWorkflows;
    int         index = 0;
    std::string name = "";
    bool        unlit = false;
    float       emissiveStrength = 1.0f;
};

} // namespace Humongous
