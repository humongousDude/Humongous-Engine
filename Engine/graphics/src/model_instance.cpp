#include "model_instance.hpp"
#include "globals.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"

namespace Humongous
{

ModelInstance::ModelInstance(std::shared_ptr<Model> model, ResourceManager& resourceManager, const u32& instanceID)
    : m_model(model), m_resourceManager(resourceManager), m_instanceID(instanceID)
{
    HGTRACE("Creating new model instance...");
    const u32 nodeCount = m_model->GetLinearNodes().size();
    m_localNodeMatrices.resize(nodeCount);
    m_globalNodeMatrices.resize(nodeCount);
    m_nodeTranslations.resize(nodeCount);
    m_nodeRotations.resize(nodeCount);
    m_nodeScales.resize(nodeCount);
    m_nodeIsMatrixSpecified.resize(nodeCount);

    for(size_t i = 0; i < nodeCount; ++i)
    {
        const Node* nodeBlueprint = m_model->GetLinearNodes()[i];

        m_nodeTranslations[i] = nodeBlueprint->translation;
        m_nodeRotations[i] = nodeBlueprint->rotation;
        m_nodeScales[i] = nodeBlueprint->scale;
        m_nodeIsMatrixSpecified[i] = nodeBlueprint->isMatrixSpecified;

        m_localNodeMatrices[i] = nodeBlueprint->localMatrix;
    }

    if(!m_model->GetJointMatrices().empty()) { m_jointMatrices.resize(model->GetJointMatrices().size()); }
    else
    {
        m_jointMatrices.push_back(Eigen::Matrix4f::Identity());
    }
    if(m_model->HasMorphs())
    {
        m_morphWeights.resize(model->GetMorphTargets().size());
        m_hasMorphTargets = true;
    }

    UpdateTransforms();
    UpdateSkins();
    UpdateAnimatedAABB();

    HGTRACE("Created a new model instance");
}

ModelInstance::~ModelInstance() {}

u32 ModelInstance::GetNodeMatrixOffset() const { return m_resourceManager.GetModelHandleToMatrixStart(m_instanceID); }
u32 ModelInstance::GetJointMatrixOffset() const { return m_resourceManager.GetModelHandleToJointStart(m_instanceID); }
u32 ModelInstance::GetMorphTargetOffset() const { return m_resourceManager.GetModelHandleToMorphStart(m_instanceID); }

// FIXME: This way of calculating the AABB is very loose, and should be improved.
void ModelInstance::UpdateAnimatedAABB()
{
    m_animatedAABB.Invalidate();
    m_animatedAABB.valid = true;

    for(const auto* mesh: m_model->GetMeshes())
    {
        for(const auto& primitive: mesh->primitives)
        {
            if(!primitive || !primitive->boundingBox.valid || !primitive->owner) { continue; }

            BoundingBox inflatedLocalAABB = primitive->boundingBox;
            if(primitive->maxMorphDisplacement > std::numeric_limits<f32>::epsilon())
            {
                inflatedLocalAABB.min -= Eigen::Vector4f::Constant(primitive->maxMorphDisplacement);
                inflatedLocalAABB.max += Eigen::Vector4f::Constant(primitive->maxMorphDisplacement);
            }

            BoundingBox worldSpacePrimitiveAABB{};

            Node*           ownerNode = primitive->owner;
            Eigen::Matrix4f animatedOwnerGlobalTransform = m_globalNodeMatrices[ownerNode->index];
            if(ownerNode->skin)
            {
                worldSpacePrimitiveAABB = BoundingBox();
                worldSpacePrimitiveAABB.valid = true;
                Skin* skin = m_model->GetSkins()[ownerNode->skinIndex];

                worldSpacePrimitiveAABB = BoundingBox::TransformAABB(inflatedLocalAABB, animatedOwnerGlobalTransform);
            }
            else
            {
                worldSpacePrimitiveAABB = BoundingBox::TransformAABB(inflatedLocalAABB, animatedOwnerGlobalTransform);
            }

            m_animatedAABB.Extend(worldSpacePrimitiveAABB);
        }
    }
}

void ModelInstance::UpdateAnimation()
{
    if(!m_playAnimation) { return; }

    std::fill(m_morphWeights.begin(), m_morphWeights.end(), 0.0f);

    for(size_t i = 0; i < m_model->GetNodes().size(); ++i)
    {
        m_nodeTranslations[i] = m_model->GetNodes()[i]->translation;
        m_nodeRotations[i] = m_model->GetNodes()[i]->rotation;
        m_nodeScales[i] = m_model->GetNodes()[i]->scale;
    }

    f32 deltaTime = Globals::Time::AverageDeltaTime();

    m_animationTime += deltaTime;
    const auto& anim = m_model->GetAnimations()[m_currentAnimationIndex];
    if(m_animationTime > anim.end) { m_animationTime = anim.start; }

    for(const auto& channel: anim.channels)
    {
        auto& sampler = anim.samplers[channel.samplerIndex];

        size_t index = 0;

        if(sampler.inputs.size() == 1) { index = 0; }
        else
        {
            for(size_t i = 0; i < sampler.inputs.size() - 1; ++i)
            {
                if(m_animationTime >= sampler.inputs[i] && m_animationTime <= sampler.inputs[i + 1])
                {
                    index = i;
                    break;
                }
            }
            if(m_animationTime >= sampler.inputs.back()) { index = sampler.inputs.size() - 1; }
        }

        switch(channel.path)
        {
            case Model::AnimationChannel::PathType::TRANSLATION:
                sampler.ApplyTranslation(index, m_animationTime, m_nodeTranslations, channel.node->index);
                break;
            case Model::AnimationChannel::PathType::ROTATION:
                sampler.ApplyRotation(index, m_animationTime, m_nodeRotations, channel.node->index);
                break;
            case Model::AnimationChannel::PathType::SCALE:
                sampler.ApplyScale(index, m_animationTime, m_nodeScales, channel.node->index);
                break;
            case Model::AnimationChannel::PathType::WEIGHTS:
                auto node = m_model->NodeFromIndex(channel.node->index);
                if(!node)
                {
                    HGWARN("Node %i not found", channel.node->index);
                    continue;
                }
                const auto& targetPrimitive = node->mesh->primitives;
                if(targetPrimitive.empty())
                {
                    HGWARN("No primitives found for node %i", channel.node->index);
                    continue;
                }

                for(auto& prim: targetPrimitive) { sampler.ApplyMorph(index, m_animationTime, *prim, m_morphWeights); }
                break;
        }
    }
}

void ModelInstance::UpdateTransforms()
{
    for(size_t i = 0; i < m_localNodeMatrices.size(); ++i)
    {
        Eigen::Affine3f localTransform = Eigen::Affine3f::Identity();
        localTransform.translate(m_nodeTranslations[i]);
        localTransform.rotate(m_nodeRotations[i]);
        localTransform.scale(m_nodeScales[i]);

        m_localNodeMatrices[i] = localTransform.matrix();
    }

    for(const auto* nodeBlueprint: m_model->GetLinearNodes())
    {
        Eigen::Matrix4f parentGlobalTransform = Eigen::Matrix4f::Identity();

        if(nodeBlueprint->parent) { parentGlobalTransform = m_globalNodeMatrices[nodeBlueprint->parent->index]; }

        m_globalNodeMatrices[nodeBlueprint->index] = parentGlobalTransform * m_localNodeMatrices[nodeBlueprint->index];
    }
}

void ModelInstance::UpdateSkins()
{
    for(const auto* skin: m_model->GetSkins())
    {
        if(skin->joints.empty()) { return; }

        if(m_jointMatrices.size() != skin->joints.size()) { m_jointMatrices.resize(skin->joints.size()); }

        for(size_t i = 0; i < skin->joints.size(); ++i)
        {
            const Node*            jointNodeBlueprint = skin->joints[i];
            const Eigen::Matrix4f& globalJointTransform = m_globalNodeMatrices[jointNodeBlueprint->index];
            m_jointMatrices[i] = globalJointTransform * skin->inverseBindMatrices[i];
        }
    }
}

void ModelInstance::Update()
{
    if(!m_playAnimation) { return; }

    UpdateAnimation();
    UpdateTransforms();
    UpdateSkins();
    UpdateAnimatedAABB();
}

} // namespace Humongous
