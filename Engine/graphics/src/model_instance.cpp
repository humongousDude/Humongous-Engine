#include "model_instance.hpp"
#include "globals.hpp"
#include "resource_manager.hpp"

namespace Humongous
{

ModelInstance::ModelInstance(std::shared_ptr<Model> model, const n32& instanceID) : m_model(model), m_instanceID(instanceID)
{
    HGINFO("Creating new model instance...");
    const n32 nodeCount = m_model->GetLinearNodes().size();
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
        m_nodeIsMatrixSpecified[i] = nodeBlueprint->isMatrixSpecified; // Store the flag

        m_localNodeMatrices[i] = nodeBlueprint->localMatrix;
    }

    if(!m_model->GetSkins().empty()) { m_jointMatrices.resize(m_model->GetJointMatrices().size()); }

    if(m_model->HasMorphs()) { m_morphWeights.resize(m_model->GetMorphTargets().size()); }

    std::vector<glm::mat4> jointMatricies = model->GetJointMatrices();
    if(!jointMatricies.empty()) { ResourceManager::AddJointMatriciesToModel(jointMatricies, m_instanceID); }
    else
    {
        jointMatricies.push_back(glm::mat4(glm::identity<glm::mat4>()));
        ResourceManager::AddJointMatriciesToModel(jointMatricies, m_instanceID);
    }

    std::vector<f32> morphTargets = model->GetMorphTargets();
    if(!morphTargets.empty()) { ResourceManager::AddMorphTargetsToModel(morphTargets, m_instanceID); }

    UpdateTransforms();
    UpdateSkins();
    UpdateAnimatedAABB();

    ResourceManager::UpdateNodeMatrices(m_globalNodeMatrices, m_instanceID);

    if(!m_jointMatrices.empty()) { ResourceManager::UpdateJointMatrices(m_jointMatrices, m_instanceID); }
    else
    {
        m_jointMatrices.push_back(glm::mat4(glm::identity<glm::mat4>()));
        ResourceManager::UpdateJointMatrices(m_jointMatrices, m_instanceID);
    }

    if(m_model->HasMorphs())
    {
        if(!m_morphWeights.empty()) { ResourceManager::UpdateMorphTargets(m_morphWeights, m_instanceID); }
    }

    HGINFO("Created a new model instance");
}

ModelInstance::~ModelInstance() {}

n32 ModelInstance::GetNodeMatrixOffset() const { return ResourceManager::GetModelHandleToMatrixStart(m_instanceID); }
n32 ModelInstance::GetJointMatrixOffset() const { return ResourceManager::Get().m_modelHandleToJointStart[m_instanceID].first; }
n32 ModelInstance::GetMorphTargetOffset() const { return ResourceManager::Get().m_modelHandleToMorphStart[m_instanceID].first; }

void ModelInstance::UpdateAnimatedAABB()
{
    m_animatedAABB.Invalidate();
    m_animatedAABB.valid = true;

    for(const auto* mesh: m_model->GetMeshes())
    {
        for(const Primitive* primitive: mesh->primitives)
        {
            if(!primitive || !primitive->boundingBox.valid || !primitive->owner) { continue; }

            BoundingBox inflatedLocalAABB = primitive->boundingBox;
            if(primitive->maxMorphDisplacement > glm::epsilon<f32>())
            {
                inflatedLocalAABB.min -= glm::vec4(primitive->maxMorphDisplacement);
                inflatedLocalAABB.max += glm::vec4(primitive->maxMorphDisplacement);
            }

            BoundingBox worldSpacePrimitiveAABB{};

            Node* ownerNode = primitive->owner;
            if(ownerNode->skin)
            {
                worldSpacePrimitiveAABB = BoundingBox();
                worldSpacePrimitiveAABB.valid = true;
                Skin* skin = m_model->GetSkins()[ownerNode->skinIndex];
                for(size_t i = 0; i < skin->joints.size(); ++i)
                {
                    glm::mat4 skinningMatrix = ownerNode->localToModelMatrix * skin->jointMatrices[i];

                    BoundingBox transformedByJoint = BoundingBox::TransformAABB(inflatedLocalAABB, skinningMatrix);
                    worldSpacePrimitiveAABB.Extend(transformedByJoint);
                }
            }
            else { worldSpacePrimitiveAABB = BoundingBox::TransformAABB(inflatedLocalAABB, ownerNode->localToModelMatrix); }

            m_animatedAABB.Extend(worldSpacePrimitiveAABB);
        }
    }
}

void ModelInstance::UpdateAnimation()
{
    if(!m_playAnimation) { return; }

    std::fill(m_morphWeights.begin(), m_morphWeights.end(), 0.0f);

    // Also reset the node transforms to their default pose from the blueprint
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

    // Apply animation to our state vectors
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
                if(!node) { continue; }
                const auto& targetPrimitive = node->mesh->primitives;
                if(targetPrimitive.empty()) { continue; }

                for(auto& prim: targetPrimitive) { sampler.ApplyMorph(index, m_animationTime, *prim, m_morphWeights); }
                break;
        }
    }
}

void ModelInstance::UpdateTransforms()
{
    for(size_t i = 0; i < m_localNodeMatrices.size(); ++i)
    {
        m_localNodeMatrices[i] = glm::translate(glm::mat4(1.0f), m_nodeTranslations[i]) * glm::mat4_cast(m_nodeRotations[i]) *
                                 glm::scale(glm::mat4(1.0f), m_nodeScales[i]);
    }

    for(const auto* nodeBlueprint: m_model->GetLinearNodes())
    {
        glm::mat4 parentGlobalTransform = glm::mat4(1.0f);
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
            const Node*      jointNodeBlueprint = skin->joints[i];
            const glm::mat4& globalJointTransform = m_globalNodeMatrices[jointNodeBlueprint->index];
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

    ResourceManager::UpdateNodeMatrices(m_globalNodeMatrices, m_instanceID);

    if(!m_jointMatrices.empty()) { ResourceManager::UpdateJointMatrices(m_jointMatrices, m_instanceID); }
    else
    {
        m_jointMatrices.push_back(glm::mat4(glm::identity<glm::mat4>()));
        ResourceManager::UpdateJointMatrices(m_jointMatrices, m_instanceID);
    }

    if(m_model->HasMorphs())
    {
        if(!m_morphWeights.empty()) { ResourceManager::UpdateMorphTargets(m_morphWeights, m_instanceID); }
    }
}

} // namespace Humongous
