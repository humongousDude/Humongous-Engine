#pragma once
#include "model.hpp"
#include <limits>

namespace Humongous
{
class ModelInstance
{
public:
    ModelInstance(std::shared_ptr<Model> model, const n32& instanceID);
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

    void SetAnimation(const n32& index)
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

    n32                                  GetAnimationCount() const { return m_model->GetAnimationCount(); }
    std::string                          GetCurrentAnimationName() const { return m_animIndexToName.at(m_currentAnimationIndex); }
    f32                                  GetAnimationTime() const { return m_animationTime; }
    const std::vector<Model::Animation>& GetAnimations() const { return m_model->GetAnimations(); }
    b32                                  HasMorphs() const { return m_hasMorphTargets; }

    f32 GetMorph(const n32& index) const { return m_morphWeights[index]; }
    n32 GetMorphCount() const { return m_morphWeights.size(); }

    n32 GetNodeMatrixOffset() const;
    n32 GetJointMatrixOffset() const;
    n32 GetMorphTargetOffset() const;

    n32 GetInstanceID() const { return m_instanceID; }

    BoundingBox GetAnimatedBoundingBox() const { return m_animatedAABB; }

    void Update();

private:
    std::shared_ptr<Model> m_model;

    std::unordered_map<std::string, n32> m_animNameToIndex;
    std::unordered_map<n32, std::string> m_animIndexToName;
    std::vector<Model::Animation>        m_animations;
    n32                                  m_currentAnimationIndex{0};
    f32                                  m_animationTime{0};
    b32                                  m_updateAnimation{false};
    b32                                  m_playAnimation{false};
    b32                                  m_hasMorphTargets{false};
    n32                                  m_instanceID{std::numeric_limits<n32>::max()};
    b32                                  m_hasAnimations;
    BoundingBox                          m_animatedAABB{};

    b32 m_isPlaying{false};

    std::vector<glm::vec3> m_nodeTranslations;
    std::vector<glm::quat> m_nodeRotations;
    std::vector<glm::vec3> m_nodeScales;

    std::vector<float> m_morphWeights;

    std::vector<glm::mat4> m_localNodeMatrices;
    std::vector<glm::mat4> m_globalNodeMatrices;
    std::vector<b32>       m_nodeIsMatrixSpecified;

    std::vector<glm::mat4> m_jointMatrices;

    void UpdateAnimation();
    void UpdateTransforms();
    void UpdateSkins();
    void UpdateAnimatedAABB();

    std::vector<glm::mat4> GetMatrixVector();
};

} // namespace Humongous
