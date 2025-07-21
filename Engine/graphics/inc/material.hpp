#pragma once

#include "defines.hpp"
#include "entity_component_system/components/entity_component.hpp"
#include <Eigen/Dense>

#include <string>

namespace Humongous
{
struct Node;

struct alignas(16) BoundingBox : public EntityComponent
{
    Eigen::Vector4f min = Eigen::Vector4f::Constant(std::numeric_limits<f32>::max());
    Eigen::Vector4f max = Eigen::Vector4f::Constant(std::numeric_limits<f32>::min());
    s32             valid{false};

    static BoundingBox LocalToGlobal(const BoundingBox& localBoundingBox, const Eigen::Matrix4f& model);
    static BoundingBox TransformAABB(const BoundingBox& aabb, const Eigen::Matrix4f& transform)
    {
        // if(!aabb.valid) { return BoundingBox{}; }

        Eigen::Vector4f newMin = Eigen::Vector4f::Constant(std::numeric_limits<f32>::max());
        Eigen::Vector4f newMax = Eigen::Vector4f::Constant(std::numeric_limits<f32>::lowest());

        Eigen::Vector3f corners[8] = {
            Eigen::Vector3f(aabb.min.x(), aabb.min.y(), aabb.min.z()), Eigen::Vector3f(aabb.max.x(), aabb.min.y(), aabb.min.z()),
            Eigen::Vector3f(aabb.min.x(), aabb.max.y(), aabb.min.z()), Eigen::Vector3f(aabb.min.x(), aabb.min.y(), aabb.max.z()),
            Eigen::Vector3f(aabb.max.x(), aabb.max.y(), aabb.min.z()), Eigen::Vector3f(aabb.max.x(), aabb.min.y(), aabb.max.z()),
            Eigen::Vector3f(aabb.min.x(), aabb.max.y(), aabb.max.z()), Eigen::Vector3f(aabb.max.x(), aabb.max.y(), aabb.max.z())};

        for(int i = 0; i < 8; ++i)
        {
            Eigen::Vector4f transformedCorner = transform * Eigen::Vector4f(corners[i].x(), corners[i].y(), corners[i].z(), 1.0);

            newMin = newMin.cwiseMin(transformedCorner);
            newMax = newMax.cwiseMax(transformedCorner);
        }

        return BoundingBox{newMin, newMax};
    }
    void Extend(const Eigen::Vector4f& point)
    {
        if(!valid)
        {
            min = point;
            max = point;
            valid = true;
        }
        else
        {
            min = min.cwiseMin(point);
            max = max.cwiseMax(point);
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
        min = min.cwiseMin(other.min);
        max = max.cwiseMax(other.max);
    }

    void Invalidate()
    {
        min = Eigen::Vector4f::Constant(std::numeric_limits<f32>::max());
        max = Eigen::Vector4f::Constant(std::numeric_limits<f32>::min());
        valid = false;
    };

    BoundingBox() = default;
    BoundingBox(const Eigen::Vector4f& min, const Eigen::Vector4f& max) : min(min), max(max), valid(true) {}
};

struct Material
{
    enum AlphaMode
    {
        ALPHAMODE_OPAQUE,
        ALPHAMODE_MASK,
        ALPHAMODE_BLEND
    };
    AlphaMode       alphaMode = ALPHAMODE_OPAQUE;
    float           alphaCutoff = 1.0f;
    float           metallicFactor = 1.0f;
    float           roughnessFactor = 1.0f;
    Eigen::Vector4f baseColorFactor = Eigen::Vector4f::Ones();
    Eigen::Vector4f emissiveFactor = Eigen::Vector4f::Zero();
    n32             baseColorTextureIndex = -1;
    n32             metallicRoughnessTextureIndex = -1;
    n32             normalTextureIndex = -1;
    n32             occlusionTextureIndex = -1;
    n32             emissiveTextureIndex = -1;
    bool            doubleSided = false;
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
        n32             specularGlossinessTextureIndex = -1;
        n32             diffuseTextureIndex = -1;
        Eigen::Vector4f diffuseFactor = Eigen::Vector4f::Ones();
        Eigen::Vector3f specularFactor = Eigen::Vector3f::Zero();
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
