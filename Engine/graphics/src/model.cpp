#include "model.hpp"
#include "defines.hpp"
#include "extra.hpp"
#include "logger.hpp"
#include "meshoptimizer.h"
#include "resource_manager.hpp"
#include <set>

#define TINYGLTF_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT

#include "tiny_gltf.h"

namespace Humongous
{
// Mesh
Mesh::Mesh(LogicalDevice* device, Eigen::Matrix4f matrix) { this->logicalDevice = device; };

Mesh::~Mesh()
{
    for(Primitive* p: primitives) { delete p; }
}

Model::Model(LogicalDevice* device, const std::string& modelPath, f32 scale, const n32& handle) : m_handle(handle)
{
    HGINFO("Creating model...");
    LoadFromFile(modelPath, device, device->GetGraphicsQueue(), scale);
    HGINFO("Created model");
}

Model::~Model() { Destroy(m_logicalDevice->GetVkDevice()); }

void Model::LoadFromFile(std::string filePath, LogicalDevice* logicalDevice, vk::Queue transferQueue, f32 scale)
{
    if(m_initialized) { return; }

    tinygltf::Model    gltfModel;
    tinygltf::TinyGLTF gltfContext;

    std::string error;
    std::string warning;

    this->m_logicalDevice = logicalDevice;

    bool   binary = false;
    size_t extpos = filePath.rfind('.', filePath.length());
    if(extpos != std::string::npos) { binary = (filePath.substr(extpos + 1, filePath.length() - extpos) == "glb"); }

    bool       fileLoaded = binary ? gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, filePath.c_str())
                                   : gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filePath.c_str());
    LoaderInfo loaderInfo{};
    size_t     vertexCount = 0;
    size_t     indexCount = 0;

    size_t lastSlashPos = filePath.rfind('/');
    size_t lastBackslashPos = filePath.rfind('\\');

    size_t filenameStartPos = std::string::npos;
    if(lastSlashPos != std::string::npos && lastBackslashPos != std::string::npos) { filenameStartPos = std::max(lastSlashPos, lastBackslashPos); }
    else if(lastSlashPos != std::string::npos) { filenameStartPos = lastSlashPos; }
    else if(lastBackslashPos != std::string::npos) { filenameStartPos = lastBackslashPos; }

    std::string fileName;
    if(filenameStartPos != std::string::npos) { fileName = filePath.substr(filenameStartPos + 1); }
    else { fileName = filePath; }

    std::string filenameWithoutExtension;
    if(extpos != std::string::npos && extpos > filenameStartPos)
    {
        if(filenameStartPos != std::string::npos)
        {
            filenameWithoutExtension = filePath.substr(filenameStartPos + 1, extpos - (filenameStartPos + 1));
        }
        else { filenameWithoutExtension = filePath.substr(0, extpos); }
    }
    else { filenameWithoutExtension = fileName; }

    m_name = filenameWithoutExtension;

    if(fileLoaded)
    {
        LoadTextureSamplers(gltfModel);
        LoadTextures(gltfModel, logicalDevice, transferQueue);
        LoadMaterials(gltfModel);

        const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

        for(size_t i = 0; i < scene.nodes.size(); i++) { GetNodeProps(gltfModel.nodes[scene.nodes[i]], gltfModel, vertexCount, indexCount); }
        m_linearNodes.resize(gltfModel.nodes.size());

        // TODO: scene handling with no default scene
        for(size_t i = 0; i < scene.nodes.size(); i++)
        {
            const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
            LoadNode(nullptr, node, scene.nodes[i], gltfModel, loaderInfo, scale, Eigen::Matrix4f::Identity());
        }
        if(gltfModel.animations.size() > 0) { LoadAnimations(gltfModel); }
        LoadSkins(gltfModel);

        for(auto node: m_linearNodes)
        {
            if(node->skinIndex > -1) { node->skin = m_skins[node->skinIndex]; }
            else { node->skin = nullptr; }
        }
    }
    else
    {
        HGERROR(error.c_str());
        return;
    }

    m_indices.insert(m_indices.begin(), loaderInfo.indexBuffer.begin(), loaderInfo.indexBuffer.end());
    m_vertices.insert(m_vertices.begin(), loaderInfo.vertexBuffer.begin(), loaderInfo.vertexBuffer.end());

    for(auto& node: m_nodes) { UpdateMaterialBatches(node); }

    std::set<Mesh*> uniqueMeshes;
    for(Node* node: m_linearNodes)
    {
        if(node->mesh) { uniqueMeshes.insert(node->mesh); }
    }

    std::vector<Mesh*>      allMeshesForThisModel(uniqueMeshes.begin(), uniqueMeshes.end());
    std::vector<Primitive*> allPrimitivesForThisModel;
    for(Node* node: m_linearNodes)
    {
        if(node->mesh)
        {
            allPrimitivesForThisModel.insert(allPrimitivesForThisModel.end(), node->mesh->primitives.begin(), node->mesh->primitives.end());
        }
    }

    HGINFO("Model %s loaded", m_name.c_str());
    HGINFO("Nodes: %i", m_nodes.size());
    HGINFO("Meshes: %i", m_meshes.size());
    HGINFO("Animations: %i", m_animations.size());
    HGINFO("Vertices: %i", m_vertices.size());
    HGINFO("Indices: %i", m_indices.size());

    // CreateMeshlets();

    CalculateRestAABB();

    ResourceManager::AddVerticesToModel(m_vertices, allMeshesForThisModel);
    ResourceManager::AddIndicesToModel(m_indices, allPrimitivesForThisModel);

    LoadMaterialData();

    std::vector<Eigen::Matrix4f> jointMatricies;
    for(const auto& skin: m_skins)
    {
        if(skin->jointMatrices.empty()) { continue; }

        jointMatricies.insert(jointMatricies.end(), skin->jointMatrices.begin(), skin->jointMatrices.end());
    }

    if(!jointMatricies.empty()) { ResourceManager::AddJointMatriciesToModel(jointMatricies, m_handle); }
    else
    {
        jointMatricies.push_back(Eigen::Matrix4f::Identity());
        ResourceManager::AddJointMatriciesToModel(jointMatricies, m_handle);
    }

    std::vector<f32> morphTargets;
    for(const auto& mesh: m_meshes) { morphTargets.insert(morphTargets.begin(), mesh->weights.begin(), mesh->weights.end()); }
    if(!morphTargets.empty()) { ResourceManager::AddMorphTargetsToModel(morphTargets, m_handle); }
    m_initialized = true;
}

void Model::Destroy(vk::Device device)
{
    for(auto node: m_nodes) { delete node; }
    m_nodes.resize(0);
    m_linearNodes.resize(0);
};

// TODO: clean up this abomination

struct Position
{
    f32 x, y, z;
};

void Model::LoadNode(Node* parent, const tinygltf::Node& node, n32 nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo, f32 globalscale,
                     Eigen::Matrix4f parentTransform)
{
    Node* newNode = new Node{};
    newNode->index = nodeIndex;
    newNode->parent = parent;
    newNode->name = node.name;
    newNode->skinIndex = node.skin;

    if(node.matrix.size() == 16)
    {
        newNode->localMatrix = Eigen::Map<const Eigen::Matrix4d>(node.matrix.data()).cast<float>();
        newNode->isMatrixSpecified = true;
        Utils::DecomposeMatrix(newNode->localMatrix, newNode->translation, newNode->rotation, newNode->scale);
    }
    else
    {
        if(node.translation.size() == 3) { newNode->translation = Eigen::Map<const Eigen::Vector3d>(node.translation.data()).cast<float>(); }
        if(node.rotation.size() == 4) { newNode->rotation = Eigen::Map<const Eigen::Quaterniond>(node.rotation.data()).cast<float>(); }
        if(node.scale.size() == 3) { newNode->scale = Eigen::Map<const Eigen::Vector3d>(node.scale.data()).cast<float>(); }
        newNode->isMatrixSpecified = false;

        newNode->CalculateLocalMatrix();
    }

    Eigen::Matrix4f currentWorldTransform = parentTransform * newNode->localMatrix;
    newNode->localToModelMatrix = currentWorldTransform;

    if(!node.children.empty())
    {
        for(size_t i = 0; i < node.children.size(); i++)
        {
            LoadNode(newNode, model.nodes[node.children[i]], node.children[i], model, loaderInfo, globalscale, currentWorldTransform);
        }
    }

    if(node.mesh > -1)
    {
        const tinygltf::Mesh mesh = model.meshes[node.mesh];
        Mesh*                newMesh = new Mesh(m_logicalDevice, newNode->localMatrix);

        if(!mesh.weights.empty()) { m_hasMorphTargets = true; }

        newMesh->weights.resize(mesh.weights.size());
        for(size_t i = 0; i < mesh.weights.size(); ++i) { newMesh->weights[i] = static_cast<f32>(mesh.weights[i]); }

        for(size_t j = 0; j < mesh.primitives.size(); j++)
        {
            const tinygltf::Primitive& primitive = mesh.primitives[j];

            n32 vertexStart = static_cast<n32>(loaderInfo.vertexPos);
            n32 indexStart = static_cast<n32>(loaderInfo.indexPos);
            n32 indexCount = 0;
            n32 vertexCount = 0;

            bool hasSkin = false;
            bool hasIndices = primitive.indices > -1;

            std::vector<std::vector<Eigen::Vector3f>> morphTargetPositionsOriginal; // Store original
            std::vector<std::vector<Eigen::Vector3f>> morphTargetNormalsOriginal;
            std::vector<std::vector<Eigen::Vector4f>> morphTargetTangentsOriginal;

            Eigen::Vector4f currentPrimitiveMinPos = Eigen::Vector4f::Constant(std::numeric_limits<f32>::max());
            Eigen::Vector4f currentPrimitiveMaxPos = Eigen::Vector4f::Constant(std::numeric_limits<f32>::lowest());
            f32             currentMaxMorphDisplacement = 0.0f;

            std::vector<Vertex> currentPrimitiveVertices;
            std::vector<n32>    currentPrimitiveIndices;

            for(const auto& target_map: primitive.targets)
            {
                std::vector<Eigen::Vector3f> targetPositions;
                std::vector<Eigen::Vector3f> targetNormals;
                std::vector<Eigen::Vector4f> targetTangents;

                // Target Positions (_POSITION)
                auto posIt = target_map.find("POSITION");
                if(posIt != target_map.end())
                {
                    const tinygltf::Accessor&   accessor = model.accessors[posIt->second];
                    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer&     buffer = model.buffers[bufferView.buffer];
                    const Eigen::Vector3f*      dataPtr =
                        reinterpret_cast<const Eigen::Vector3f*>(buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);
                    targetPositions.assign(dataPtr, dataPtr + accessor.count);

                    for(size_t k = 0; k < accessor.count; ++k)
                    {
                        currentMaxMorphDisplacement = std::max(currentMaxMorphDisplacement, targetPositions[k].norm());
                    }
                }
                morphTargetPositionsOriginal.push_back(targetPositions);

                // Target Normals (_NORMAL) - Optional
                auto normalIt = target_map.find("NORMAL");
                if(normalIt != target_map.end())
                {
                    const tinygltf::Accessor&   accessor = model.accessors[normalIt->second];
                    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer&     buffer = model.buffers[bufferView.buffer];
                    const Eigen::Vector3f*      dataPtr =
                        reinterpret_cast<const Eigen::Vector3f*>(buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);
                    targetNormals.assign(dataPtr, dataPtr + accessor.count);
                }
                morphTargetNormalsOriginal.push_back(targetNormals);

                // Target Tangents (_TANGENT) - Optional
                auto tangentIt = target_map.find("TANGENT");
                if(tangentIt != target_map.end())
                {
                    const tinygltf::Accessor&   accessor = model.accessors[tangentIt->second];
                    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer&     buffer = model.buffers[bufferView.buffer];
                    const Eigen::Vector4f*      dataPtr =
                        reinterpret_cast<const Eigen::Vector4f*>(buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);
                    targetTangents.assign(dataPtr, dataPtr + accessor.count);
                }
                morphTargetTangentsOriginal.push_back(targetTangents);
            }

            const f32*  bufferPos = nullptr;
            const f32*  bufferNormals = nullptr;
            const f32*  bufferTexCoordSet0 = nullptr;
            const f32*  bufferTexCoordSet1 = nullptr;
            const f32*  bufferColorSet0 = nullptr;
            const void* bufferJoints = nullptr;
            const f32*  bufferWeights = nullptr;
            const f32*  bufferTangents = nullptr;

            int    posByteStride;
            int    normByteStride;
            int    uv0ByteStride;
            int    uv1ByteStride;
            int    color0ByteStride;
            int    jointByteStride;
            int    weightByteStride;
            size_t tangentByteStride = 0;

            int jointComponentType;

            HGASSERT(primitive.attributes.find("POSITION") != primitive.attributes.end());

            const tinygltf::Accessor&   posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];

            posByteStride = posAccessor.ByteStride(posView) ? posAccessor.ByteStride(posView)
                                                            : (sizeof(f32) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3));

            const unsigned char* rawPosBase = model.buffers[posView.buffer].data.data() + posView.byteOffset + posAccessor.byteOffset;

            // Normals (if present)
            normByteStride = 0;
            const unsigned char* rawNormBase = nullptr;
            if(primitive.attributes.find("NORMAL") != primitive.attributes.end())
            {
                const tinygltf::Accessor&   normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                normByteStride = normAccessor.ByteStride(normView) ? normAccessor.ByteStride(normView)
                                                                   : (sizeof(f32) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3));
                rawNormBase = model.buffers[normView.buffer].data.data() + normView.byteOffset + normAccessor.byteOffset;
            }

            // UV0 (if present)
            uv0ByteStride = 0;
            const unsigned char* rawUv0Base = nullptr;
            if(primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
            {
                const tinygltf::Accessor&   uvAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
                uv0ByteStride = uvAccessor.ByteStride(uvView) ? uvAccessor.ByteStride(uvView)
                                                              : (sizeof(f32) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2));
                rawUv0Base = model.buffers[uvView.buffer].data.data() + uvView.byteOffset + uvAccessor.byteOffset;
            }

            if(primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end())
            {
                const tinygltf::Accessor&   uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_1")->second];
                const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
                // bufferTexCoordSet1 = reinterpret_cast<const f32*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset +
                // uvView.byteOffset]));
                uv1ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(f32))
                                                              : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
            }

            // Vertex colors
            if(primitive.attributes.find("COLOR_0") != primitive.attributes.end())
            {
                const tinygltf::Accessor&   accessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
                const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                // bufferColorSet0 = reinterpret_cast<const f32*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                color0ByteStride =
                    accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(f32)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
            }

            // Skinning
            // Joints
            if(primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
            {
                const tinygltf::Accessor&   jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
                const tinygltf::BufferView& jointView = model.bufferViews[jointAccessor.bufferView];
                // bufferJoints = &(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]);
                jointComponentType = jointAccessor.componentType;
                jointByteStride = jointAccessor.ByteStride(jointView)
                                      ? (jointAccessor.ByteStride(jointView) / tinygltf::GetComponentSizeInBytes(jointComponentType))
                                      : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
            }

            if(primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
            {
                const tinygltf::Accessor&   weightAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
                const tinygltf::BufferView& weightView = model.bufferViews[weightAccessor.bufferView];
                // bufferWeights =
                //     reinterpret_cast<const f32*>(&(model.buffers[weightView.buffer].data[weightAccessor.byteOffset + weightView.byteOffset]));
                weightByteStride = weightAccessor.ByteStride(weightView) ? (weightAccessor.ByteStride(weightView) / sizeof(f32))
                                                                         : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
            }
            if(primitive.attributes.find("TANGENT") != primitive.attributes.end())
            {
                const tinygltf::Accessor&   tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
                const tinygltf::BufferView& tangentView = model.bufferViews[tangentAccessor.bufferView];
                // bufferTangents =
                //     reinterpret_cast<const f32*>(&model.buffers[tangentView.buffer].data[tangentView.byteOffset + tangentAccessor.byteOffset]);
                tangentByteStride = tangentAccessor.ByteStride(tangentView);
            }

            hasSkin = (bufferJoints && bufferWeights);
            vertexCount = static_cast<n32>(posAccessor.count);

            // currentPrimitiveVertices will grow to posAccessor.count
            currentPrimitiveVertices.reserve(posAccessor.count);

            for(size_t v = 0; v < posAccessor.count; v++)
            {
                Vertex vert{};

                // POSITION is guaranteed to be present and of type FLOAT.
                {
                    const f32*      p = reinterpret_cast<const f32*>(rawPosBase + (v * posByteStride));
                    Eigen::Vector3f posVec = Eigen::Map<const Eigen::Vector3f>(p);
                    vert.position = Eigen::Vector4f(posVec.x(), posVec.y(), posVec.z(), 1.0f);
                }

                // NORMAL (if it exists)
                if(rawNormBase)
                {
                    const f32*      n = reinterpret_cast<const f32*>(rawNormBase + (v * normByteStride));
                    Eigen::Vector3f normVec = Eigen::Map<const Eigen::Vector3f>(n);
                    normVec.normalize();
                    vert.normal = Eigen::Vector4f(normVec.x(), normVec.y(), normVec.z(), 1.0f);
                }
                else { vert.normal = Eigen::Vector4f::Zero(); }

                // UV0
                if(rawUv0Base)
                {
                    const f32*      uv = reinterpret_cast<const f32*>(rawUv0Base + (v * uv0ByteStride));
                    Eigen::Vector2f uvVec = Eigen::Map<const Eigen::Vector2f>(uv);
                    vert.uv0 = Eigen::Vector4f(uvVec.x(), uvVec.y(), 1.0f, 1.0f);
                }
                else { vert.uv0 = Eigen::Vector4f::Zero(); }

                // JOINTS_0 / WEIGHTS_0 (if present)
                if(primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   jointAccessor = model.accessors[primitive.attributes.at("JOINTS_0")];
                    const tinygltf::BufferView& jointView = model.bufferViews[jointAccessor.bufferView];
                    const unsigned char*        rawJointBase =
                        model.buffers[jointView.buffer].data.data() + jointView.byteOffset + jointAccessor.byteOffset;
                    size_t currentJointByteStride = jointAccessor.ByteStride(jointView)
                                                        ? jointAccessor.ByteStride(jointView)
                                                        : (tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4) *
                                                           tinygltf::GetComponentSizeInBytes(jointAccessor.componentType));

                    if(jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const n16* j = reinterpret_cast<const n16*>(rawJointBase + (v * currentJointByteStride));
                        vert.joint0 = Eigen::Vector4i(j[0], j[1], j[2], j[3]);
                    }
                    else if(jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const n8* j = reinterpret_cast<const n8*>(rawJointBase + (v * currentJointByteStride));
                        vert.joint0 = Eigen::Vector4i(j[0], j[1], j[2], j[3]);
                    }
                    else { HGERROR("Unexpected JOINTS_0 componentType = %d", jointAccessor.componentType); }
                }
                else { vert.joint0 = Eigen::Vector4i::Zero(); }

                if(primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   weightAccessor = model.accessors[primitive.attributes.at("WEIGHTS_0")];
                    const tinygltf::BufferView& weightView = model.bufferViews[weightAccessor.bufferView];
                    const unsigned char*        rawWeightBase =
                        model.buffers[weightView.buffer].data.data() + weightView.byteOffset + weightAccessor.byteOffset;
                    size_t     currentWeightByteStride = weightAccessor.ByteStride(weightView)
                                                             ? weightAccessor.ByteStride(weightView)
                                                             : (sizeof(f32) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4));
                    const f32* w = reinterpret_cast<const f32*>(rawWeightBase + (v * currentWeightByteStride));
                    vert.weight0 = Eigen::Map<const Eigen::Vector4f>(w);
                    if(vert.weight0.norm() == 0.0f) { vert.weight0 = Eigen::Vector4f::Zero(); }
                }
                else { vert.weight0 = Eigen::Vector4f::Zero(); }

                // Tangent (if present)
                if(primitive.attributes.find("TANGENT") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   tanAccessor = model.accessors[primitive.attributes.at("TANGENT")];
                    const tinygltf::BufferView& tanView = model.bufferViews[tanAccessor.bufferView];
                    const unsigned char* rawTanBase = model.buffers[tanView.buffer].data.data() + tanView.byteOffset + tanAccessor.byteOffset;
                    size_t               currentTanByteStride = tanAccessor.ByteStride(tanView)
                                                                    ? tanAccessor.ByteStride(tanView)
                                                                    : (sizeof(f32) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4));
                    const f32*           t = reinterpret_cast<const f32*>(rawTanBase + (v * currentTanByteStride));
                    Eigen::Vector3f      tangent = Eigen::Map<const Eigen::Vector3f>(t).normalized();
                    // the w‐component in glTF TANGENT is the sign for the bitangent
                    f32             sign = t[3];
                    Eigen::Vector3f bitan = (vert.normal.head<3>().cross(tangent) * sign).normalized();
                    vert.tangent = Eigen::Vector4f(tangent.x(), tangent.y(), tangent.z(), 1.0f);
                    vert.bitTangent = Eigen::Vector4f(bitan.x(), bitan.y(), bitan.z(), 1.0f);
                }
                else
                {
                    vert.tangent = Eigen::Vector4f::Zero();
                    vert.bitTangent = Eigen::Vector4f::Zero();
                }

                // Morph Target Data (already extracted into morphTargetPositionsOriginal, etc.)
                const constexpr size_t MAX_SUPPORTED_TARGETS = 4;
                for(size_t targetIdx = 0; targetIdx < morphTargetPositionsOriginal.size() && targetIdx < MAX_SUPPORTED_TARGETS; ++targetIdx)
                {
                    // Ensure the target position exists for this vertex (accessor.count check implicitly handled by loop)
                    if(v < morphTargetPositionsOriginal[targetIdx].size())
                    {
                        const Eigen::Vector3f& targetOffset = morphTargetPositionsOriginal[targetIdx][v];
                        switch(targetIdx)
                        {
                            case 0:
                                vert.targetPos0 = Eigen::Vector4f(targetOffset.x(), targetOffset.y(), targetOffset.z(), 0.0f);
                                break;
                            case 1:
                                vert.targetPos1 = Eigen::Vector4f(targetOffset.x(), targetOffset.y(), targetOffset.z(), 0.0f);
                                break;
                            // ... handle cases for targetPos2, targetPos3 if needed
                            default:
                                break;
                        }
                    }
                }
                if(morphTargetPositionsOriginal.size() <= 0) { vert.targetPos0 = Eigen::Vector4f::Zero(); }
                if(morphTargetPositionsOriginal.size() <= 1) { vert.targetPos1 = Eigen::Vector4f::Zero(); }
                // Handle targetPos2, targetPos3 similarly if you add them to Vertex struct

                currentPrimitiveMinPos = currentPrimitiveMinPos.cwiseMin(vert.position);
                currentPrimitiveMaxPos = currentPrimitiveMaxPos.cwiseMax(vert.position);

                currentPrimitiveVertices.push_back(vert);
            } // end for v in posAccessor.count

            if(hasIndices)
            {
                HGASSERT(primitive.indices > -1);
                const tinygltf::Accessor&   accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer&     buffer = model.buffers[bufferView.buffer];

                indexCount = static_cast<n32>(accessor.count);
                const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                currentPrimitiveIndices.reserve(accessor.count);
                switch(accessor.componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        {
                            const n32* buf = static_cast<const n32*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++) { currentPrimitiveIndices.push_back(buf[index]); }
                            break;
                        }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        {
                            const n16* buf = static_cast<const n16*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++) { currentPrimitiveIndices.push_back(buf[index]); }
                            break;
                        }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        {
                            const n8* buf = static_cast<const n8*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++) { currentPrimitiveIndices.push_back(buf[index]); }
                            break;
                        }
                    default:
                        HGERROR("Index component type %i not supported!", accessor.componentType);
                        return;
                }
            }

            Primitive* newPrimitive = new Primitive();
            newPrimitive->localFirstIndex = indexStart;
            newPrimitive->indexCount = static_cast<n32>(currentPrimitiveIndices.size());
            newPrimitive->localVertexOffset = vertexStart;
            newPrimitive->vertexCount = static_cast<n32>(currentPrimitiveVertices.size());

            newPrimitive->material = primitive.material > -1 ? &m_materials[primitive.material] : &m_materials.back();
            newPrimitive->owner = newNode;
            newPrimitive->maxMorphDisplacement = currentMaxMorphDisplacement;
            newPrimitive->boundingBox = BoundingBox(currentPrimitiveMinPos, currentPrimitiveMaxPos);
            newPrimitive->boundingBox.valid = true;
            newPrimitive->id = newMesh->primitives.size();
            newPrimitive->globalWeightOffset = m_morphTargets.size();

            HGINFO("Primitive %i has %i vertices and %i indices pre optimization", newPrimitive->id, currentPrimitiveVertices.size(),
                   currentPrimitiveIndices.size());

            if(!currentPrimitiveIndices.empty() && !currentPrimitiveVertices.empty())
            {
                std::vector<n32> remap(currentPrimitiveVertices.size());
                size_t           uniqueVertexCount =
                    meshopt_generateVertexRemap(remap.data(), currentPrimitiveIndices.data(), currentPrimitiveIndices.size(),
                                                currentPrimitiveVertices.data(), currentPrimitiveVertices.size(), sizeof(Vertex));

                std::vector<n32>    remappedIndices(currentPrimitiveIndices.size());
                std::vector<Vertex> remappedVertices(uniqueVertexCount);

                meshopt_remapIndexBuffer(remappedIndices.data(), currentPrimitiveIndices.data(), currentPrimitiveIndices.size(), remap.data());
                meshopt_remapVertexBuffer(remappedVertices.data(), currentPrimitiveVertices.data(), currentPrimitiveVertices.size(), sizeof(Vertex),
                                          remap.data());

                newPrimitive->morphTargetPositions.resize(morphTargetPositionsOriginal.size());
                newPrimitive->morphTargetNormals.resize(morphTargetNormalsOriginal.size());
                newPrimitive->morphTargetTangents.resize(morphTargetTangentsOriginal.size());

                for(size_t targetIdx = 0; targetIdx < morphTargetPositionsOriginal.size(); ++targetIdx)
                {
                    newPrimitive->morphTargetPositions[targetIdx].resize(uniqueVertexCount);
                    meshopt_remapVertexBuffer(newPrimitive->morphTargetPositions[targetIdx].data(), morphTargetPositionsOriginal[targetIdx].data(),
                                              morphTargetPositionsOriginal[targetIdx].size(), sizeof(Eigen::Vector3f), remap.data());
                }
                for(size_t targetIdx = 0; targetIdx < morphTargetNormalsOriginal.size(); ++targetIdx)
                {
                    newPrimitive->morphTargetNormals[targetIdx].resize(uniqueVertexCount);
                    meshopt_remapVertexBuffer(newPrimitive->morphTargetNormals[targetIdx].data(), morphTargetNormalsOriginal[targetIdx].data(),
                                              morphTargetNormalsOriginal[targetIdx].size(), sizeof(Eigen::Vector3f), remap.data());
                }
                for(size_t targetIdx = 0; targetIdx < morphTargetTangentsOriginal.size(); ++targetIdx)
                {
                    newPrimitive->morphTargetTangents[targetIdx].resize(uniqueVertexCount);
                    meshopt_remapVertexBuffer(newPrimitive->morphTargetTangents[targetIdx].data(), morphTargetTangentsOriginal[targetIdx].data(),
                                              morphTargetTangentsOriginal[targetIdx].size(), sizeof(Eigen::Vector4f), remap.data());
                }

                meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), remappedIndices.size(), uniqueVertexCount);

                meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(), remappedIndices.size(), &remappedVertices[0].position.x(),
                                         uniqueVertexCount, sizeof(Vertex), 1.05f);

                meshopt_optimizeVertexFetch(remappedVertices.data(), remappedIndices.data(), remappedIndices.size(), remappedVertices.data(),
                                            uniqueVertexCount, sizeof(Vertex));

                newPrimitive->localVertexOffset = static_cast<n32>(loaderInfo.vertexBuffer.size());
                newPrimitive->vertexCount = static_cast<n32>(remappedVertices.size());
                newPrimitive->localFirstIndex = static_cast<n32>(loaderInfo.indexBuffer.size());
                newPrimitive->indexCount = static_cast<n32>(remappedIndices.size());

                loaderInfo.vertexBuffer.insert(loaderInfo.vertexBuffer.end(), remappedVertices.begin(), remappedVertices.end());
                loaderInfo.indexBuffer.insert(loaderInfo.indexBuffer.end(), remappedIndices.begin(), remappedIndices.end());
            }
            else
            {
                newPrimitive->localVertexOffset = static_cast<n32>(loaderInfo.vertexBuffer.size());
                newPrimitive->vertexCount = static_cast<n32>(currentPrimitiveVertices.size());
                newPrimitive->localFirstIndex = static_cast<n32>(loaderInfo.indexBuffer.size());
                newPrimitive->indexCount = static_cast<n32>(currentPrimitiveIndices.size());

                loaderInfo.vertexBuffer.insert(loaderInfo.vertexBuffer.end(), currentPrimitiveVertices.begin(), currentPrimitiveVertices.end());
                loaderInfo.indexBuffer.insert(loaderInfo.indexBuffer.end(), currentPrimitiveIndices.begin(), currentPrimitiveIndices.end());

                newPrimitive->morphTargetPositions = std::move(morphTargetPositionsOriginal);
                newPrimitive->morphTargetNormals = std::move(morphTargetNormalsOriginal);
                newPrimitive->morphTargetTangents = std::move(morphTargetTangentsOriginal);
            }

            HGINFO("Primitive %i has %i vertices and %i indices post optimization", newPrimitive->id, newPrimitive->vertexCount,
                   newPrimitive->indexCount);

            newPrimitive->hasIndices = hasIndices;
            newMesh->primitives.push_back(newPrimitive);
            m_primitives.push_back(newPrimitive);
            m_morphTargets.insert(m_morphTargets.end(), newMesh->weights.begin(), newMesh->weights.end());
        }

        newNode->mesh = newMesh;
        m_meshes.push_back(newMesh);
    }
    if(parent) { parent->children.push_back(newNode); }
    else { m_nodes.push_back(newNode); }
    m_linearNodes[newNode->index] = newNode;
}

void Model::CreateMeshlets() { HGINFO("Meshlets created. Total meshlets: %zu", m_meshlets.size()); }

// Node Helpers

Node* Model::NodeFromIndex(n32 index)
{
    Node* nodeFound = nullptr;
    for(auto& node: m_nodes)
    {
        nodeFound = FindNode(node, index);
        if(nodeFound) { break; }
    }
    return nodeFound;
}

Node* Model::FindNode(Node* parent, n32 index)
{
    Node* nodeFound = nullptr;
    if(parent->index == index) { return parent; }
    for(auto& child: parent->children)
    {
        nodeFound = FindNode(child, index);
        if(nodeFound) { break; }
    }
    return nodeFound;
}

void Model::GetNodeProps(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount)
{
    if(node.children.size() > 0)
    {
        for(size_t i = 0; i < node.children.size(); i++) { GetNodeProps(model.nodes[node.children[i]], model, vertexCount, indexCount); }
    }
    if(node.mesh > -1)
    {
        const tinygltf::Mesh mesh = model.meshes[node.mesh];
        for(size_t i = 0; i < mesh.primitives.size(); i++)
        {
            auto primitive = mesh.primitives[i];
            vertexCount += model.accessors[primitive.attributes.find("POSITION")->second].count;
            if(primitive.indices > -1) { indexCount += model.accessors[primitive.indices].count; }
        }
    }
}

// Materials and textures

vk::SamplerAddressMode Model::GetVkWrapMode(s32 wrapMode)
{
    switch(wrapMode)
    {
        case -1:
        case 10497:
            return vk::SamplerAddressMode::eRepeat;
        case 33071:
            return vk::SamplerAddressMode::eClampToEdge;
        case 33648:
            return vk::SamplerAddressMode::eMirroredRepeat;
    }

    HGWARN("Unknown wrap mode for getvk::WrapMode:  %i", wrapMode);
    return vk::SamplerAddressMode::eRepeat;
}

vk::Filter Model::GetVkFilterMode(s32 filterMode)
{
    switch(filterMode)
    {
        case -1:
        case 9728:
            return vk::Filter::eNearest;
        case 9729:
            return vk::Filter::eLinear;
        case 9984:
            return vk::Filter::eNearest;
        case 9985:
            return vk::Filter::eNearest;
        case 9986:
            return vk::Filter::eLinear;
        case 9987:
            return vk::Filter::eLinear;
    }

    HGWARN("Unknown filter mode for getvk::FilterMode: %i", filterMode);
    return vk::Filter::eNearest;
}

void Model::LoadTextures(tinygltf::Model& gltfModel, LogicalDevice* device, vk::Queue transferQueue)
{
    for(tinygltf::Texture& tex: gltfModel.textures)
    {
        tinygltf::Image         image = gltfModel.images[tex.source];
        Texture::TexSamplerInfo textureSampler;
        if(tex.sampler == -1)
        {
            textureSampler.magFilter = vk::Filter::eLinear;
            textureSampler.minFilter = vk::Filter::eLinear;
            textureSampler.addressModeU = vk::SamplerAddressMode::eRepeat;
            textureSampler.addressModeV = vk::SamplerAddressMode::eRepeat;
            textureSampler.addressModeW = vk::SamplerAddressMode::eRepeat;
        }
        else { textureSampler = m_textureSamplers[tex.sampler]; }
        m_textures.push_back(ResourceManager::RequestTexture(image, textureSampler));
    }
}

void Model::LoadTextureSamplers(tinygltf::Model& gltfModel)
{
    for(tinygltf::Sampler smpl: gltfModel.samplers)
    {
        Texture::TexSamplerInfo sampler{};
        sampler.minFilter = GetVkFilterMode(smpl.minFilter);
        sampler.magFilter = GetVkFilterMode(smpl.magFilter);
        sampler.addressModeU = GetVkWrapMode(smpl.wrapS);
        sampler.addressModeV = GetVkWrapMode(smpl.wrapT);
        sampler.addressModeW = sampler.addressModeV;
        m_textureSamplers.push_back(sampler);
    }
}

void Model::LoadMaterials(tinygltf::Model& gltfModel)
{
    for(const auto& gltfMat: gltfModel.materials)
    {
        Material mat{};
        mat.doubleSided = gltfMat.doubleSided;

        if(gltfMat.values.find("baseColorFactor") != gltfMat.values.end())
        {
            auto fc = gltfMat.values.at("baseColorFactor").ColorFactor();
            mat.baseColorFactor =
                Eigen::Vector4f(static_cast<f32>(fc[0]), static_cast<f32>(fc[1]), static_cast<f32>(fc[2]), static_cast<f32>(fc[3]));
        }
        if(gltfMat.values.find("baseColorTexture") != gltfMat.values.end())
        {
            int texIndex = gltfMat.values.at("baseColorTexture").TextureIndex();
            mat.baseColorTextureIndex = ResourceManager::RequestTexture(gltfModel.images[gltfModel.textures[texIndex].source],
                                                                        m_textureSamplers[gltfModel.textures[texIndex].sampler]);
        }

        if(gltfMat.values.find("metallicFactor") != gltfMat.values.end())
        {
            mat.metallicFactor = static_cast<f32>(gltfMat.values.at("metallicFactor").Factor());
        }
        if(gltfMat.values.find("roughnessFactor") != gltfMat.values.end())
        {
            mat.roughnessFactor = static_cast<f32>(gltfMat.values.at("roughnessFactor").Factor());
        }
        if(gltfMat.values.find("metallicRoughnessTexture") != gltfMat.values.end())
        {
            int mrIndex = gltfMat.values.at("metallicRoughnessTexture").TextureIndex();
            mat.metallicRoughnessTextureIndex = ResourceManager::RequestTexture(gltfModel.images[gltfModel.textures[mrIndex].source],
                                                                                m_textureSamplers[gltfModel.textures[mrIndex].sampler]);
        }

        if(gltfMat.additionalValues.find("normalTexture") != gltfMat.additionalValues.end())
        {
            auto& nv = gltfMat.additionalValues.at("normalTexture");
            int   normalIndex = nv.TextureIndex();
            mat.normalTextureIndex = ResourceManager::RequestTexture(gltfModel.images[gltfModel.textures[normalIndex].source],
                                                                     m_textureSamplers[gltfModel.textures[normalIndex].sampler]);
        }

        if(gltfMat.additionalValues.find("occlusionTexture") != gltfMat.additionalValues.end())
        {
            auto& ov = gltfMat.additionalValues.at("occlusionTexture");
            int   occIndex = ov.TextureIndex();
            mat.occlusionTextureIndex = ResourceManager::RequestTexture(gltfModel.images[gltfModel.textures[occIndex].source],
                                                                        m_textureSamplers[gltfModel.textures[occIndex].sampler]);
        }

        if(gltfMat.additionalValues.find("emissiveFactor") != gltfMat.additionalValues.end())
        {
            auto ef = gltfMat.additionalValues.at("emissiveFactor").ColorFactor();
            mat.emissiveFactor = Eigen::Vector4f(static_cast<f32>(ef[0]), static_cast<f32>(ef[1]), static_cast<f32>(ef[2]), 1);
        }
        if(gltfMat.additionalValues.find("emissiveTexture") != gltfMat.additionalValues.end())
        {
            auto& ev = gltfMat.additionalValues.at("emissiveTexture");
            int   emIndex = ev.TextureIndex();

            mat.emissiveTextureIndex = ResourceManager::RequestTexture(gltfModel.images[gltfModel.textures[emIndex].source],
                                                                       m_textureSamplers[gltfModel.textures[emIndex].sampler]);
        }
        if(gltfMat.extensions.find("KHR_materials_emissive_strength") != gltfMat.extensions.end())
        {
            auto& ext = gltfMat.extensions.at("KHR_materials_emissive_strength");
            if(ext.Has("emissiveStrength")) { mat.emissiveStrength = static_cast<f32>(ext.Get("emissiveStrength").Get<double>()); }
        }

        if(gltfMat.additionalValues.find("alphaMode") != gltfMat.additionalValues.end())
        {
            auto& am = gltfMat.additionalValues.at("alphaMode");
            if(am.string_value == "MASK")
            {
                mat.alphaMode = Material::ALPHAMODE_MASK;
                if(gltfMat.additionalValues.find("alphaCutoff") != gltfMat.additionalValues.end())
                {
                    mat.alphaCutoff = static_cast<f32>(gltfMat.additionalValues.at("alphaCutoff").Factor());
                }
            }
            else if(am.string_value == "BLEND") { mat.alphaMode = Material::ALPHAMODE_BLEND; }
        }

        mat.index = static_cast<int>(m_materials.size());
        mat.name = gltfMat.name;
        m_materials.push_back(mat);
        m_materialBatches.emplace(mat.index, std::vector<Primitive*>());
    }

    Material defaultMat{};
    defaultMat.index = static_cast<int>(m_materials.size());
    m_materials.push_back(defaultMat);
    m_materialBatches.emplace(defaultMat.index, std::vector<Primitive*>());
}

void Model::LoadMaterialData()
{
    std::vector<ShaderMaterial> shaderMaterials{};
    for(auto& material: m_materials)
    {
        ShaderMaterial shaderMaterial{};

        shaderMaterial.emissiveFactor = material.emissiveFactor;
        // To save space, availabilty and texture coordinate set are combined
        // -1 = texture not used for this material, >= 0 texture used and index of texture coordinate set
        shaderMaterial.baseColorTextureSet = material.baseColorTextureIndex != -1 ? material.texCoordSets.baseColor : -1;
        shaderMaterial.baseColorTextureIndex = material.baseColorTextureIndex;

        shaderMaterial.normalTextureSet = material.normalTextureIndex != -1 ? material.texCoordSets.normal : -1;
        shaderMaterial.normalTextureIndex = material.normalTextureIndex;

        shaderMaterial.occlusionTextureSet = material.occlusionTextureIndex != -1 ? material.texCoordSets.occlusion : -1;
        shaderMaterial.occlusionTextureIndex = material.occlusionTextureIndex;

        shaderMaterial.emissiveTextureSet = material.emissiveTextureIndex != -1 ? material.texCoordSets.emissive : -1;
        shaderMaterial.emissiveTextureIndex = material.emissiveTextureIndex;

        shaderMaterial.alphaMask = static_cast<f32>(material.alphaMode == Material::ALPHAMODE_MASK);
        shaderMaterial.alphaMaskCutoff = material.alphaCutoff;
        shaderMaterial.emissiveStrength = material.emissiveStrength;

        // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

        if(material.pbrWorkflows.metallicRoughness)
        {
            shaderMaterial.workflow = static_cast<f32>(PBR_WORKFLOW_METALLIC_ROUGHNESS);
            shaderMaterial.baseColorFactor = material.baseColorFactor;
            shaderMaterial.metallicFactor = material.metallicFactor;
            shaderMaterial.roughnessFactor = material.roughnessFactor;
            shaderMaterial.physicalDescriptorTextureSet =
                material.metallicRoughnessTextureIndex != -1 ? material.texCoordSets.metallicRoughness : -1;
            shaderMaterial.baseColorTextureSet = material.baseColorTextureIndex != -1 ? material.texCoordSets.baseColor : -1;
        }

        if(material.pbrWorkflows.specularGlossiness)
        {
            shaderMaterial.workflow = static_cast<f32>(PBR_WORKFLOW_SPECULAR_GLOSSINESS);
            shaderMaterial.physicalDescriptorTextureSet =
                material.extension.specularGlossinessTextureIndex != -1 ? material.texCoordSets.specularGlossiness : -1;
            shaderMaterial.baseColorTextureSet = material.extension.diffuseTextureIndex != -1 ? material.texCoordSets.baseColor : -1;
            shaderMaterial.diffuseFactor = material.extension.diffuseFactor;
            shaderMaterial.specularFactor = Eigen::Vector4f(material.extension.specularFactor.x(), material.extension.specularFactor.y(),
                                                            material.extension.specularFactor.z(), 1.0f);
        }

        material.index = ResourceManager::RequestMaterial(shaderMaterial);
        shaderMaterials.push_back(shaderMaterial);
    }
}

void Model::UpdateMaterialBatches(Node* node)
{
    if(node->mesh)
    {
        for(auto* prim: node->mesh->primitives) { m_materialBatches[prim->material->index].push_back(prim); }
    }
    for(auto& c: node->children) { UpdateMaterialBatches(c); }
}

// Animations and skinning

void Model::CalculateRestAABB()
{
    m_restAABB.Invalidate();
    m_restAABB.valid = true;

    for(const auto* mesh: m_meshes)
    {
        for(const Primitive* primitive: mesh->primitives)
        {
            if(!primitive || !primitive->boundingBox.valid || !primitive->owner) { continue; }

            const Eigen::Matrix4f& primitiveToModelSpaceMatrix = primitive->owner->localToModelMatrix;

            BoundingBox transformedPrimitiveAABB = BoundingBox::TransformAABB(primitive->boundingBox, primitiveToModelSpaceMatrix);

            m_restAABB.Extend(transformedPrimitiveAABB);
        }
    }
}

void Model::LoadSkins(tinygltf::Model& gltfModel)
{
    for(tinygltf::Skin& source: gltfModel.skins)
    {
        Skin* newSkin = new Skin{};
        newSkin->name = source.name;

        if(source.skeleton > -1) { newSkin->skeletonRoot = NodeFromIndex(source.skeleton); }
        for(int jointIndex: source.joints)
        {
            Node* node = NodeFromIndex(jointIndex);
            if(node) { newSkin->joints.push_back(NodeFromIndex(jointIndex)); }
        }

        if(source.inverseBindMatrices > -1)
        {
            const tinygltf::Accessor&   accessor = gltfModel.accessors[source.inverseBindMatrices];
            const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
            const tinygltf::Buffer&     buffer = gltfModel.buffers[bufferView.buffer];

            newSkin->inverseBindMatrices.resize(accessor.count);

            const float* srcDataPtr = reinterpret_cast<const float*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);

            for(size_t i = 0; i < accessor.count; ++i) { newSkin->inverseBindMatrices[i] = Eigen::Map<const Eigen::Matrix4f>(srcDataPtr + i * 16); }
        }

        if(newSkin->joints.size() > 128) { HGWARN("Skin %s has %i joints", newSkin->name.c_str(), newSkin->joints.size()); }

        newSkin->UpdateJointMatrices();
        m_jointMatricies.insert(m_jointMatricies.end(), newSkin->jointMatrices.begin(), newSkin->jointMatrices.end());
        m_skins.push_back(newSkin);
    }
}

void Model::LoadAnimations(tinygltf::Model& gltfModel)
{
    for(tinygltf::Animation& anim: gltfModel.animations)
    {
        Animation animation{};
        animation.name = anim.name;
        if(anim.name.empty()) { animation.name = std::to_string(m_animations.size()); }

        m_animNameToIndex.emplace(animation.name, m_animations.size());
        m_animIndexToName.emplace(m_animations.size(), animation.name);

        // Samplers
        for(auto& samp: anim.samplers)
        {
            AnimationSampler sampler{};

            if(samp.interpolation == "LINEAR") { sampler.interpolation = AnimationSampler::InterpolationType::LINEAR; }
            if(samp.interpolation == "STEP") { sampler.interpolation = AnimationSampler::InterpolationType::STEP; }
            if(samp.interpolation == "CUBICSPLINE") { sampler.interpolation = AnimationSampler::InterpolationType::CUBICSPLINE; }

            // Read sampler input time values
            {
                const tinygltf::Accessor&   accessor = gltfModel.accessors[samp.input];
                const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer&     buffer = gltfModel.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                const f32*  buf = static_cast<const f32*>(dataPtr);
                for(size_t index = 0; index < accessor.count; index++) { sampler.inputs.push_back(buf[index]); }

                for(auto input: sampler.inputs)
                {
                    if(input < animation.start) { animation.start = input; };
                    if(input > animation.end) { animation.end = input; }
                }
            }

            // Read sampler output T/R/S values
            {
                const tinygltf::Accessor&   accessor = gltfModel.accessors[samp.output];
                const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer&     buffer = gltfModel.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

                switch(accessor.type)
                {
                    case TINYGLTF_TYPE_VEC3:
                        {
                            const Eigen::Vector3f* buf = static_cast<const Eigen::Vector3f*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++)
                            {
                                sampler.outputsVec4.push_back(Eigen::Vector4f(buf[index].x(), buf[index].y(), buf[index].z(), 0.0f));
                                sampler.outputs.push_back(buf[index][0]);
                                sampler.outputs.push_back(buf[index][1]);
                                sampler.outputs.push_back(buf[index][2]);
                            }
                            break;
                        }
                    case TINYGLTF_TYPE_VEC4:
                        {
                            const Eigen::Vector4f* buf = static_cast<const Eigen::Vector4f*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++)
                            {
                                sampler.outputsVec4.push_back(buf[index]);
                                sampler.outputs.push_back(buf[index][0]);
                                sampler.outputs.push_back(buf[index][1]);
                                sampler.outputs.push_back(buf[index][2]);
                                sampler.outputs.push_back(buf[index][3]);
                            }
                            break;
                        }
                    case TINYGLTF_TYPE_SCALAR:
                        {
                            size_t actualNumMorphTargets = 2;

                            const f32* buf = static_cast<const f32*>(dataPtr);
                            for(size_t i = 0; i < accessor.count; i += actualNumMorphTargets)
                            {
                                Eigen::Vector4f weightsVec = Eigen::Vector4f::Zero();
                                for(size_t k = 0; k < actualNumMorphTargets && k < 4; ++k) { weightsVec[k] = buf[i + k]; }
                                sampler.outputsVec4.push_back(weightsVec);
                            }
                            break;
                        }
                    default:
                        {
                            HGWARN("Animation output accessor type %d not handled!", accessor.type);
                            break;
                        }
                }
            }

            animation.samplers.push_back(sampler);
        }

        // Channels
        for(auto& source: anim.channels)
        {
            AnimationChannel channel{};

            if(source.target_path == "rotation") { channel.path = AnimationChannel::PathType::ROTATION; }
            else if(source.target_path == "translation") { channel.path = AnimationChannel::PathType::TRANSLATION; }
            else if(source.target_path == "scale") { channel.path = AnimationChannel::PathType::SCALE; }
            else if(source.target_path == "weights") { channel.path = AnimationChannel::PathType::WEIGHTS; }
            else { HGWARN("Unknown target path!"); }
            channel.samplerIndex = source.sampler;
            channel.node = NodeFromIndex(source.target_node);
            if(!channel.node) { continue; }

            animation.channels.push_back(channel);
        }

        m_animations.push_back(animation);
    }
    HGINFO("Loaded %i animations for model %s", m_animations.size(), m_name.c_str());
}

// TODO: move this out of here

// AnimationSampler, thank you so much Sascha Willems for the examples

// Cube spline interpolation function used for translate/scale/rotate with cubic spline animation samples
// Details on how this works can be found in the specs
// https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#appendix-c-spline-interpolation
Eigen::Vector4f Model::Model::AnimationSampler::CubicSplineInterpolation(size_t index, f32 time, n32 stride) const
{
    f32          delta = inputs[index + 1] - inputs[index];
    f32          t = (time - inputs[index]) / delta;
    const size_t current = index * stride * 3;
    const size_t next = (index + 1) * stride * 3;
    const size_t A = 0;
    const size_t V = stride * 1;
    const size_t B = stride * 2;

    f32             t2 = powf(t, 2);
    f32             t3 = powf(t, 3);
    Eigen::Vector4f pt = Eigen::Vector4f::Zero();
    for(n32 i = 0; i < stride; i++)
    {
        f32 p0 = outputs[current + i + V];         // starting point at t = 0
        f32 m0 = delta * outputs[current + i + A]; // scaled starting tangent at t = 0
        f32 p1 = outputs[next + i + V];            // ending point at t = 1
        f32 m1 = delta * outputs[next + i + B];    // scaled ending tangent at t = 1
        pt[i] = ((2.f * t3 - 3.f * t2 + 1.f) * p0) + ((t3 - 2.f * t2 + t) * m0) + ((-2.f * t3 + 3.f * t2) * p1) + ((t3 - t2) * m1);
    }
    return pt;
}

void Model::AnimationSampler::ApplyTranslation(size_t index, f32 time, std::vector<Eigen::Vector3f>& translations, n32 targetNodeIndex) const
{
    if(inputs.size() == 1)
    {
        translations[targetNodeIndex] = outputsVec4[0].head<3>(); // Take X, Y, Z from Vector4f
        return;
    }

    switch(interpolation)
    {
        case Model::AnimationSampler::InterpolationType::LINEAR:
            {
                f32                 u = 0.0f;
                f32                 timeSpan = inputs[index + 1] - inputs[index];
                constexpr const f32 EPSILON = std::numeric_limits<f32>::epsilon();

                if(timeSpan < EPSILON)
                { // Check for near-zero time span
                    u = 0.0f;
                }
                else { u = std::max(0.0f, time - inputs[index]) / timeSpan; }

                // Eigen equivalent of glm::mix(a, b, u) for vectors: a * (1.0f - u) + b * u
                translations[targetNodeIndex] = (outputsVec4[index] * (1.0f - u) + outputsVec4[index + 1] * u).head<3>();
                break;
            }
        case Model::AnimationSampler::InterpolationType::STEP:
            {
                translations[targetNodeIndex] = outputsVec4[index].head<3>();
                break;
            }
        case Model::AnimationSampler::InterpolationType::CUBICSPLINE:
            {
                translations[targetNodeIndex] = CubicSplineInterpolation(index, time, 3).head<3>();
                break;
            }
    }
}

void Model::AnimationSampler::ApplyScale(size_t index, f32 time, std::vector<Eigen::Vector3f>& scales, n32 targetNodeIndex) const
{
    if(inputs.size() == 1)
    {
        scales[targetNodeIndex] = outputsVec4[0].head<3>();
        return;
    }

    switch(interpolation)
    {
        case Model::AnimationSampler::InterpolationType::LINEAR:
            {
                f32       u = 0.0f;
                f32       timeSpan = inputs[index + 1] - inputs[index];
                const f32 EPSILON = std::numeric_limits<f32>::epsilon();

                if(timeSpan < EPSILON) { u = 0.0f; }
                else { u = std::max(0.0f, time - inputs[index]) / timeSpan; }

                scales[targetNodeIndex] = (outputsVec4[index] * (1.0f - u) + outputsVec4[index + 1] * u).head<3>();
                break;
            }
        case Model::AnimationSampler::InterpolationType::STEP:
            {
                scales[targetNodeIndex] = outputsVec4[index].head<3>();
                break;
            }
        case Model::AnimationSampler::InterpolationType::CUBICSPLINE:
            {
                scales[targetNodeIndex] = CubicSplineInterpolation(index, time, 3).head<3>();
                break;
            }
    }
}

void Model::AnimationSampler::ApplyRotation(size_t index, f32 time, std::vector<Eigen::Quaternionf>& rotations, n32 targetNodeIndex) const
{
    if(inputs.size() == 1)
    {
        // Direct construction from components, assuming outputsVec4 stores (x, y, z, w)
        // Eigen::Quaternionf has a constructor from (w, x, y, z) or directly from Matrix3f
        // If outputsVec4 stores (x,y,z,w), then:
        rotations[targetNodeIndex] = Eigen::Quaternionf(outputsVec4[0].w(), outputsVec4[0].x(), outputsVec4[0].y(), outputsVec4[0].z());
        rotations[targetNodeIndex].normalize(); // Always normalize quaternions after construction/interpolation
        return;
    }

    switch(interpolation)
    {
        case Model::AnimationSampler::InterpolationType::LINEAR:
            {
                f32 u = std::max(0.0f, time - inputs[index]) / (inputs[index + 1] - inputs[index]);

                // Construct Eigen quaternions from outputsVec4 elements
                // Assuming outputsVec4 stores (x,y,z,w) order
                Eigen::Quaternionf q1(outputsVec4[index].w(), outputsVec4[index].x(), outputsVec4[index].y(), outputsVec4[index].z());
                Eigen::Quaternionf q2(outputsVec4[index + 1].w(), outputsVec4[index + 1].x(), outputsVec4[index + 1].y(),
                                      outputsVec4[index + 1].z());

                // glm::slerp(q1, q2, u) -> Eigen::Quaternionf::slerp(q1, q2, u)
                // glm::normalize(...) -> Eigen::Quaternionf::normalized()
                rotations[targetNodeIndex] = q1.slerp(u, q2).normalized(); // slerp returns normalized if inputs are normalized
                                                                           // but calling .normalized() explicitly is good practice.
                break;
            }
        case Model::AnimationSampler::InterpolationType::STEP:
            {
                Eigen::Quaternionf q(outputsVec4[index].w(), outputsVec4[index].x(), outputsVec4[index].y(), outputsVec4[index].z());
                rotations[targetNodeIndex] = q.normalized();
                break;
            }
        case Model::AnimationSampler::InterpolationType::CUBICSPLINE:
            {
                // Assuming CubicSplineInterpolation returns a Vector4f for rotation (x,y,z,w)
                Eigen::Vector4f    rot_vec = CubicSplineInterpolation(index, time, 4);
                Eigen::Quaternionf q(rot_vec.w(), rot_vec.x(), rot_vec.y(), rot_vec.z());
                rotations[targetNodeIndex] = q.normalized();
                break;
            }
    }
}

void Model::AnimationSampler::ApplyMorph(size_t index, f32 time, const Primitive& targetPrimitive, std::vector<float>& instanceWeights) const
{
    if(targetPrimitive.morphTargetPositions.empty())
    { // Use .empty() for vector size check
        return;
    }

    size_t numMorphTargets = targetPrimitive.morphTargetPositions.size();

    if(inputs.size() == 1)
    {
        // Using outputsVec4[0].size() is more robust for checking components if Vector4f is not fixed-size
        // but for Eigen::Vector4f, .size() is always 4.
        // Using `std::min` with `4` for a fixed `Vector4f` is safe.
        size_t actual_outputs_count = std::min((size_t)outputsVec4[0].size(), numMorphTargets);
        for(size_t k = 0; k < actual_outputs_count; ++k) { instanceWeights[targetPrimitive.globalWeightOffset + k] = outputsVec4[0][k]; }
        return;
    }

    switch(interpolation)
    {
        case Model::AnimationSampler::InterpolationType::LINEAR:
            {
                f32       timeSpan = inputs[index + 1] - inputs[index];
                f32       u = 0.0f;
                const f32 EPSILON = std::numeric_limits<f32>::epsilon(); // Re-use epsilon

                if(timeSpan > EPSILON)
                { // Check for non-zero time span
                    u = std::max(0.0f, time - inputs[index]) / timeSpan;
                }

                size_t actual_outputs_count = std::min({(size_t)outputsVec4[index].size(), (size_t)outputsVec4[index + 1].size(), numMorphTargets});
                for(size_t k = 0; k < actual_outputs_count; ++k)
                {
                    // Eigen equivalent of glm::mix(a, b, u) for scalars: a * (1.0f - u) + b * u
                    instanceWeights[targetPrimitive.globalWeightOffset + k] = outputsVec4[index][k] * (1.0f - u) + outputsVec4[index + 1][k] * u;
                }
                break;
            }
        case Model::AnimationSampler::InterpolationType::STEP:
            {
                size_t actual_outputs_count = std::min((size_t)outputsVec4[index].size(), numMorphTargets);
                for(size_t k = 0; k < actual_outputs_count; ++k)
                {
                    instanceWeights[targetPrimitive.globalWeightOffset + k] = outputsVec4[index][k];
                }
                break;
            }
        case Model::AnimationSampler::InterpolationType::CUBICSPLINE:
            {
                // Assuming CubicSplineInterpolation returns a VectorXf or Vector4f
                // The `(int)numMorphTargets` argument to `CubicSplineInterpolation` suggests
                // it might return a vector of variable size (Eigen::VectorXf).
                // If it always returns Vector4f, this needs `head<numMorphTargets>` or similar.
                Eigen::VectorXf interpolatedWeights; // Use VectorXf if numMorphTargets can be > 4
                // Assuming CubicSplineInterpolation knows how many components to return.
                interpolatedWeights = CubicSplineInterpolation(index, time, (int)numMorphTargets);

                size_t actual_outputs_count = std::min((size_t)interpolatedWeights.size(), numMorphTargets);
                for(size_t k = 0; k < actual_outputs_count; ++k)
                {
                    instanceWeights[targetPrimitive.globalWeightOffset + k] = interpolatedWeights[k];
                }
                break;
            }
    }
}
} // namespace Humongous
