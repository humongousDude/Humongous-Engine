#pragma once
#include "model.hpp"
#include <limits>

namespace Humongous
{
class ModelInstance
{
public:
    ModelInstance(std::shared_ptr<Model> model, class ResourceManager& resourceManager, const u32& instanceID);
    ~ModelInstance();

    std::shared_ptr<Model> GetModel() const { return m_model; }

    void SetAnimation(const std::string_view& animName)
    {
        auto it = m_animNameToIndex.find(animName.data());
        if(it == m_animNameToIndex.end())
        {
            HGWARN("Invalid animName. Skipping update");
            return;
        }
        m_currentAnimationIndex = it->second;
        m_animationTime = 0;
    };

    void SetAnimation(const u32& index)
    {
        m_currentAnimationIndex = index;
        m_animationTime = 0;
    }

    void PlayAnimation()
    {
        m_playAnimation = true;
        m_animationTime = 0;
    };

    void StopAnimation()
    {
        m_playAnimation = false;
        m_animationTime = 0;
    };

    void PauseAnimation() { m_playAnimation = false; };
    void UnPauseAnimation() { m_playAnimation = true; };
    b32  PlayingAnimation() const { return m_playAnimation; };

    u32                                  GetAnimationCount() const { return m_model->GetAnimationCount(); }
    std::string                          GetCurrentAnimationName() const { return m_animIndexToName.at(m_currentAnimationIndex); }
    f32                                  GetAnimationTime() const { return m_animationTime; }
    const std::vector<Model::Animation>& GetAnimations() const { return m_model->GetAnimations(); }
    b32                                  HasMorphs() const { return m_hasMorphTargets; }
    b32                                  HasJoints() const { return !m_jointMatrices.empty(); }

    const std::vector<Eigen::Matrix4f>& GetNodeMatrices() const { return m_globalNodeMatrices; }
    const std::vector<Eigen::Matrix4f>& GetJointMatrices() const { return m_jointMatrices; }
    const std::vector<f32>&             GetMorphWeights() const { return m_morphWeights; }

    f32 GetMorph(const u32& index) const { return m_morphWeights[index]; }
    u32 GetMorphCount() const { return m_morphWeights.size(); }

    u32 GetNodeMatrixOffset() const;
    u32 GetJointMatrixOffset() const;
    u32 GetMorphTargetOffset() const;

    u32 GetInstanceID() const { return m_instanceID; }

    BoundingBox GetAnimatedBoundingBox() const { return m_animatedAABB; }

    void Update();

private:
    std::shared_ptr<Model> m_model;
    class ResourceManager& m_resourceManager;

    std::unordered_map<std::string, u32> m_animNameToIndex;
    std::unordered_map<u32, std::string> m_animIndexToName;
    std::vector<Model::Animation>        m_animations;
    u32                                  m_currentAnimationIndex{0};
    f32                                  m_animationTime{0};
    b32                                  m_updateAnimation{false};
    b32                                  m_playAnimation{false};
    b32                                  m_hasMorphTargets{false};
    u32                                  m_instanceID{std::numeric_limits<u32>::max()};
    b32                                  m_hasAnimations;
    BoundingBox                          m_animatedAABB{};

    b32 m_isPlaying{false};

    std::vector<Eigen::Vector3f>    m_nodeTranslations;
    std::vector<Eigen::Quaternionf> m_nodeRotations;
    std::vector<Eigen::Vector3f>    m_nodeScales;

    std::vector<float> m_morphWeights;

    std::vector<Eigen::Matrix4f> m_localNodeMatrices;
    std::vector<Eigen::Matrix4f> m_globalNodeMatrices;
    std::vector<b32>             m_nodeIsMatrixSpecified;

    std::vector<Eigen::Matrix4f> m_jointMatrices;

    void UpdateAnimation();
    void UpdateTransforms();
    void UpdateSkins();
    void UpdateAnimatedAABB();

    std::vector<Eigen::Matrix4f> GetMatrixVector();
};

} // namespace Humongous
