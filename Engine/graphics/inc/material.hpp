#pragma once

#include "entity_component_system/components/entity_component.hpp"
#include "texture.hpp"

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
    glm::vec3 min{std::numeric_limits<f32>::max()}; // 12 bytes
    float     padding1;                             // 4 bytes for alignment

    glm::vec3 max{std::numeric_limits<f32>::min()}; // 12 bytes
    float     padding2;                             // 4 bytes for alignment

    std::array<glm::vec4, 8> corners;

    s32   valid{false}; // 4 bytes (matches GLSL int for std140)
    float padding3[3];  // 12 bytes to align the struct to 48 bytes

    static BoundingBox LocalToGlobal(const BoundingBox& localBoundingBox, const glm::mat4& model);

    BoundingBox() = default;
    BoundingBox(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}
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
