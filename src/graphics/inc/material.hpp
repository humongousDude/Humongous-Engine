#pragma once

#include "abstractions/buffer.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "images.hpp"
#include "texture.hpp"
#include <string>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/fwd.hpp>
#include <glm/glm.hpp>

#include "defines.hpp"
#include <render_pipeline.hpp>

namespace Humongous
{
struct Node;

struct BoundingBox
{
    glm::vec3 min;
    glm::vec3 max;
    bool      valid = false;
    BoundingBox(){};
    BoundingBox(glm::vec3 min, glm::vec3 max) : min(min), max(max){};
    BoundingBox GetAABB(glm::mat4 m);
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
    Texture*  baseColorTexture;
    Texture*  metallicRoughnessTexture;
    Texture*  normalTexture;
    Texture*  occlusionTexture;
    Texture*  emissiveTexture;
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
        Texture*  specularGlossinessTexture;
        Texture*  diffuseTexture;
        glm::vec4 diffuseFactor = glm::vec4(1.0f);
        glm::vec3 specularFactor = glm::vec3(0.0f);
    } extension;
    struct PbrWorkflows
    {
        bool metallicRoughness = true;
        bool specularGlossiness = false;
    } pbrWorkflows;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    int             index = 0;
    std::string     name = "";
    bool            unlit = false;
    float           emissiveStrength = 1.0f;
};

} // namespace Humongous
