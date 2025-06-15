#include "model.hpp"
#include "asserts.hpp"
#include "asset_manager.hpp"
#include "defines.hpp"
#include "logger.hpp"
#include "resource_manager.hpp"
#include <set>

#define TINYGLTF_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT

#include "tiny_gltf.h"

namespace Humongous
{
// Primitive::Primitive(n32 firstIndex, n32 indexCount, n32 vertexCount, Material& material)
//     : firstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), material(material)
// {
// };
//
// Mesh
Mesh::Mesh(LogicalDevice* device, glm::mat4 matrix) { this->logicalDevice = device; };

Mesh::~Mesh()
{
    for(Primitive* p: primitives) { delete p; }
}

Model::Model(LogicalDevice* device, const std::string& modelPath, float scale)
{
    HGINFO("Creating model...");
    LoadFromFile(modelPath, device, device->GetGraphicsQueue(), scale);
    HGINFO("Created model");
}

Model::~Model() { Destroy(m_logicalDevice->GetVkDevice()); }

void Model::LoadFromFile(std::string filePath, LogicalDevice* logicalDevice, vk::Queue transferQueue, float scale)
{
    if(m_initialized) { return; }

    HGINFO("Creating model...");

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

    if(fileLoaded)
    {
        LoadTextureSamplers(gltfModel);
        LoadTextures(gltfModel, logicalDevice, transferQueue);
        LoadMaterials(gltfModel);

        const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

        for(size_t i = 0; i < scene.nodes.size(); i++) { GetNodeProps(gltfModel.nodes[scene.nodes[i]], gltfModel, vertexCount, indexCount); }
        loaderInfo.vertexBuffer.resize(vertexCount);
        loaderInfo.indexBuffer.resize(indexCount);

        m_indices.resize(indexCount);

        // TODO: scene handling with no default scene
        for(size_t i = 0; i < scene.nodes.size(); i++)
        {
            const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
            LoadNode(nullptr, node, scene.nodes[i], gltfModel, loaderInfo, scale, glm::identity<glm::mat4>());
        }
        /* if(gltfModel.animations.size() > 0) { loadAnimations(gltfModel); }
        loadSkins(gltfModel); */

        for(auto node: m_linearNodes)
        {
            // // Assign skins
            // if(node->m_skinIndex > -1) { node->m_skin = skins[node->m_skinIndex]; }
            // Initial pose
            if(node->mesh) { node->Update(); }
        }
    }
    else
    {
        HGERROR(error.c_str());
        return;
    }

    // extensions = gltfModel.extensionsUsed;

    m_indices = loaderInfo.indexBuffer;

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

    m_vertices = loaderInfo.vertexBuffer;

    m_localAABB = CalculateModelAABB(m_nodes, loaderInfo.vertexBuffer, loaderInfo.indexBuffer);

    ResourceManager::AddVerticesToModel(m_vertices, allMeshesForThisModel);
    ResourceManager::AddIndicesToModel(m_indices, allPrimitivesForThisModel);

    LoadMaterialData();

    m_initialized = true;
    HGINFO("Initialized model!");
}

void Model::Destroy(vk::Device device)
{
    m_emptyTexture.Destroy();

    for(auto node: m_nodes) { delete node; }
    m_nodes.resize(0);
    m_linearNodes.resize(0);
};

void Model::UpdateUBO(Node* node, glm::mat4 matrix)
{

    // Might need this when I implement animations.
    // if(node->mesh)
    // {
    //     node->mesh->uniformBlock.matrix = matrix;
    //     node->mesh->uniformBuffer.uniformBuffer.WriteToBuffer((void*)&node->mesh->uniformBlock, sizeof(node->mesh->uniformBlock));
    // }
}

void Model::LoadNode(Node* parent, const tinygltf::Node& node, n32 nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo,
                     float globalscale, glm::mat4 parentTransform)
{
    Node* newNode = new Node{};
    newNode->index = nodeIndex;
    newNode->parent = parent;
    newNode->name = node.name;
    // newNode->skinIndex = node.skin;

    glm::mat4 localMatrix = glm::mat4(1.0f);

    // Determine the local node transform: EITHER from an explicit matrix OR from TRS
    if(node.matrix.size() == 16) { localMatrix = glm::make_mat4x4(node.matrix.data()); }
    else
    {
        glm::vec3 translation = glm::vec3(0.0f);
        if(node.translation.size() == 3)
        {
            translation = glm::make_vec3(node.translation.data());
            newNode->translation = translation;
        }

        glm::quat rotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        if(node.rotation.size() == 4)
        {
            rotationQuat = glm::make_quat(node.rotation.data());
            newNode->rotation = rotationQuat;
        }

        glm::vec3 scale = glm::vec3(1.0f);
        if(node.scale.size() == 3)
        {
            scale = glm::make_vec3(node.scale.data());
            newNode->scale = scale;
        }

        localMatrix = glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotationQuat) * glm::scale(glm::mat4(1.0f), scale);
    }

    newNode->matrix = localMatrix;

    glm::mat4 currentWorldTransform = parentTransform * newNode->matrix;

    if(!node.children.empty())
    {
        for(size_t i = 0; i < node.children.size(); i++)
        {
            LoadNode(newNode, model.nodes[node.children[i]], node.children[i], model, loaderInfo, globalscale, currentWorldTransform);
        }
    }

    // Node contains mesh data
    if(node.mesh > -1)
    {

        const tinygltf::Mesh mesh = model.meshes[node.mesh];
        Mesh*                newMesh = new Mesh(m_logicalDevice, newNode->matrix);
        for(size_t j = 0; j < mesh.primitives.size(); j++)
        {
            const tinygltf::Primitive& primitive = mesh.primitives[j];
            n32                        vertexStart = static_cast<n32>(loaderInfo.vertexPos);
            n32                        indexStart = static_cast<n32>(loaderInfo.indexPos);
            n32                        indexCount = 0;
            n32                        vertexCount = 0;
            bool                       hasSkin = false;
            bool                       hasIndices = primitive.indices > -1;
            // Vertices
            {

                const float* bufferPos = nullptr;
                const float* bufferNormals = nullptr;
                const float* bufferTexCoordSet0 = nullptr;
                const float* bufferTexCoordSet1 = nullptr;
                const float* bufferColorSet0 = nullptr;
                const void*  bufferJoints = nullptr;
                const float* bufferWeights = nullptr;
                const float* bufferTangents = nullptr;

                int    posByteStride;
                int    normByteStride;
                int    uv0ByteStride;
                int    uv1ByteStride;
                int    color0ByteStride;
                int    jointByteStride;
                int    weightByteStride;
                size_t tangentByteStride = 0;

                int jointComponentType;

                // Position attribute is required
                HGASSERT(primitive.attributes.find("POSITION") != primitive.attributes.end());

                const tinygltf::Accessor&   posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];

                posByteStride = posAccessor.ByteStride(posView) ? posAccessor.ByteStride(posView)
                                                                : (sizeof(float) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3));

                const unsigned char* rawPosBase = model.buffers[posView.buffer].data.data() + posView.byteOffset + posAccessor.byteOffset;

                // Normals (if present)
                normByteStride = 0;
                const unsigned char* rawNormBase = nullptr;
                if(primitive.attributes.find("NORMAL") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                    const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                    normByteStride = normAccessor.ByteStride(normView) ? normAccessor.ByteStride(normView)
                                                                       : (sizeof(float) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3));
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
                                                                  : (sizeof(float) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2));
                    rawUv0Base = model.buffers[uvView.buffer].data.data() + uvView.byteOffset + uvAccessor.byteOffset;
                }

                if(primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_1")->second];
                    const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
                    bufferTexCoordSet1 =
                        reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                    uv1ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float))
                                                                  : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
                }

                // Vertex colors
                if(primitive.attributes.find("COLOR_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   accessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
                    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    bufferColorSet0 = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    color0ByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                                 : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
                }

                // Skinning
                // Joints
                if(primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
                    const tinygltf::BufferView& jointView = model.bufferViews[jointAccessor.bufferView];
                    bufferJoints = &(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]);
                    jointComponentType = jointAccessor.componentType;
                    jointByteStride = jointAccessor.ByteStride(jointView)
                                          ? (jointAccessor.ByteStride(jointView) / tinygltf::GetComponentSizeInBytes(jointComponentType))
                                          : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
                }

                if(primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   weightAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
                    const tinygltf::BufferView& weightView = model.bufferViews[weightAccessor.bufferView];
                    bufferWeights =
                        reinterpret_cast<const float*>(&(model.buffers[weightView.buffer].data[weightAccessor.byteOffset + weightView.byteOffset]));
                    weightByteStride = weightAccessor.ByteStride(weightView) ? (weightAccessor.ByteStride(weightView) / sizeof(float))
                                                                             : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
                }
                if(primitive.attributes.find("TANGENT") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
                    const tinygltf::BufferView& tangentView = model.bufferViews[tangentAccessor.bufferView];
                    bufferTangents = reinterpret_cast<const float*>(
                        &model.buffers[tangentView.buffer].data[tangentView.byteOffset + tangentAccessor.byteOffset]);
                    tangentByteStride = tangentAccessor.ByteStride(tangentView);
                }

                hasSkin = (bufferJoints && bufferWeights);

                HGASSERT(loaderInfo.vertexPos + posAccessor.count <= loaderInfo.vertexBuffer.size() &&
                         "Not enough space in loaderInfo.vertexBuffer to hold all positions!");
                for(size_t v = 0; v < posAccessor.count; v++)
                {
                    Vertex& vert = loaderInfo.vertexBuffer[loaderInfo.vertexPos];

                    // POSITION is guaranteed to be present and of type FLOAT.
                    {
                        const float* p = reinterpret_cast<const float*>(rawPosBase + (v * posByteStride));
                        vert.position = glm::vec4(glm::make_vec3(p), 1.0f);
                        if(loaderInfo.vertexPos < 5) { HGINFO("  [Pos%zu] = (%.3f, %.3f, %.3f)", v, p[0], p[1], p[2]); }
                    }

                    // NORMAL (if it exists)
                    if(rawNormBase)
                    {
                        const float* n = reinterpret_cast<const float*>(rawNormBase + (v * normByteStride));
                        vert.normal = glm::vec4(glm::normalize(glm::make_vec3(n)), 1.0f);
                    }
                    else { vert.normal = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); }

                    // UV0
                    if(rawUv0Base)
                    {
                        const float* uv = reinterpret_cast<const float*>(rawUv0Base + (v * uv0ByteStride));
                        vert.uv0 = glm::vec4(glm::make_vec2(uv), 1.0f, 1.0f);
                    }
                    else { vert.uv0 = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); }

                    // … repeat for UV1, COLOR_0, TANGENT in exactly the same “byte‐based” style …

                    // JOINTS_0 / WEIGHTS_0 (if present)
                    if(primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor&   jointAccessor = model.accessors[primitive.attributes.at("JOINTS_0")];
                        const tinygltf::BufferView& jointView = model.bufferViews[jointAccessor.bufferView];
                        const unsigned char*        rawJointBase =
                            model.buffers[jointView.buffer].data.data() + jointView.byteOffset + jointAccessor.byteOffset;
                        size_t jointByteStride = jointAccessor.ByteStride(jointView)
                                                     ? jointAccessor.ByteStride(jointView)
                                                     : (tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4) *
                                                        tinygltf::GetComponentSizeInBytes(jointAccessor.componentType));

                        if(jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        {
                            const uint16_t* j = reinterpret_cast<const uint16_t*>(rawJointBase + (v * jointByteStride));
                            // vert.joint0 = glm::vec4(float(j[0]), float(j[1]), float(j[2]), float(j[3]));
                        }
                        else if(jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        {
                            const uint8_t* j = reinterpret_cast<const uint8_t*>(rawJointBase + (v * jointByteStride));
                            // vert.joint0 = glm::vec4(float(j[0]), float(j[1]), float(j[2]), float(j[3]));
                        }
                        else { HGERROR("Unexpected JOINTS_0 componentType = %d", jointAccessor.componentType); }
                    }
                    else { /* vert.joint0 = glm::vec4(0.0f); */ }

                    if(primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor&   weightAccessor = model.accessors[primitive.attributes.at("WEIGHTS_0")];
                        const tinygltf::BufferView& weightView = model.bufferViews[weightAccessor.bufferView];
                        const unsigned char*        rawWeightBase =
                            model.buffers[weightView.buffer].data.data() + weightView.byteOffset + weightAccessor.byteOffset;
                        size_t       weightByteStride = weightAccessor.ByteStride(weightView)
                                                            ? weightAccessor.ByteStride(weightView)
                                                            : (sizeof(float) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4));
                        const float* w = reinterpret_cast<const float*>(rawWeightBase + (v * weightByteStride));
                        // vert.weight0 = glm::make_vec4(w);
                        // (optional) fix degenerate all‐zero weights:
                        // if(glm::length(glm::vec3(vert.weight0)) == 0.0f) { vert.weight0 = glm::vec4(1, 0, 0, 0); }
                    }
                    else { /*  vert.weight0 = glm::vec4(0.0f); */ }

                    // Tangent (if present)
                    if(primitive.attributes.find("TANGENT") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor&   tanAccessor = model.accessors[primitive.attributes.at("TANGENT")];
                        const tinygltf::BufferView& tanView = model.bufferViews[tanAccessor.bufferView];
                        const unsigned char* rawTanBase = model.buffers[tanView.buffer].data.data() + tanView.byteOffset + tanAccessor.byteOffset;
                        size_t               tanByteStride = tanAccessor.ByteStride(tanView)
                                                                 ? tanAccessor.ByteStride(tanView)
                                                                 : (sizeof(float) * tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4));
                        const float*         t = reinterpret_cast<const float*>(rawTanBase + (v * tanByteStride));
                        glm::vec3            tangent = glm::normalize(glm::make_vec3(t));
                        // the w‐component in glTF TANGENT is the sign for the bitangent
                        float     sign = t[3];
                        glm::vec3 bitan = glm::normalize(glm::cross(glm::vec3(vert.normal), tangent) * sign);
                        vert.tangent = glm::vec4(tangent, 1.0f);
                        vert.bitTangent = glm::vec4(bitan, 1.0f);
                    }
                    else
                    {
                        vert.tangent = glm::vec4(1, 0, 0, 0);
                        vert.bitTangent = glm::vec4(0, 1, 0, 0);
                    }

                    loaderInfo.vertexPos++;
                } // end for v in posAccessor.count
            }
            // Indices
            if(hasIndices)
            {
                const tinygltf::Accessor&   accessor = model.accessors[primitive.indices > -1 ? primitive.indices : 0];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer&     buffer = model.buffers[bufferView.buffer];

                indexCount = static_cast<n32>(accessor.count);
                const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                switch(accessor.componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        {
                            const n32* buf = static_cast<const n32*>(dataPtr);
                            HGASSERT(loaderInfo.indexPos + accessor.count <= loaderInfo.indexBuffer.size() &&
                                     "Not enough space in loaderInfo.indexBuffer to hold all indices!");
                            for(size_t index = 0; index < accessor.count; index++)
                            {

                                loaderInfo.indexBuffer[loaderInfo.indexPos] = buf[index];
                                loaderInfo.indexPos++;
                            }
                            break;
                        }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        {
                            const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++)
                            {
                                loaderInfo.indexBuffer[loaderInfo.indexPos] = buf[index];
                                loaderInfo.indexPos++;
                            }
                            break;
                        }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        {
                            const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++)
                            {
                                loaderInfo.indexBuffer[loaderInfo.indexPos] = buf[index];
                                loaderInfo.indexPos++;
                            }
                            break;
                        }
                    default:
                        HGERROR("Index component type %i not supported!", accessor.componentType);
                        return;
                }
            }
            Primitive* newPrimitive = new Primitive(indexStart, // firstIndex (local index offset)
                                                    indexCount, vertexCount,
                                                    vertexStart, // **local** start of this primitive’s vertices
                                                    primitive.material > -1 ? m_materials[primitive.material] : m_materials.back());
            newPrimitive->owner = newNode;
            newMesh->primitives.push_back(newPrimitive);
        }

        newNode->mesh = newMesh;
    }
    if(parent) { parent->children.push_back(newNode); }
    else { m_nodes.push_back(newNode); }
    m_linearNodes.push_back(newNode);
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

    HGERROR("Unknown wrap mode for getvk::WrapMode:  %i", wrapMode);
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

    HGERROR("Unknown filter mode for getvk::FilterMode: %i", filterMode);
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
            // No sampler specified, use a default one
            textureSampler.magFilter = vk::Filter::eLinear;
            textureSampler.minFilter = vk::Filter::eLinear;
            textureSampler.addressModeU = vk::SamplerAddressMode::eRepeat;
            textureSampler.addressModeV = vk::SamplerAddressMode::eRepeat;
            textureSampler.addressModeW = vk::SamplerAddressMode::eRepeat;
        }
        else { textureSampler = m_textureSamplers[tex.sampler]; }
        m_textures.push_back(ResourceManager::RequestTexture(image, textureSampler));
    }

    m_emptyTexture.CreateFromFile(Systems::AssetManager::GetAsset(Systems::AssetManager::AssetType::TEXTURE, "empty"), device,
                                  Texture::ImageType::TEX2D);
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
                glm::vec4(static_cast<float>(fc[0]), static_cast<float>(fc[1]), static_cast<float>(fc[2]), static_cast<float>(fc[3]));
        }
        if(gltfMat.values.find("baseColorTexture") != gltfMat.values.end())
        {
            int texIndex = gltfMat.values.at("baseColorTexture").TextureIndex();
            mat.baseColorTextureIndex = ResourceManager::RequestTexture(gltfModel.images[gltfModel.textures[texIndex].source],
                                                                        m_textureSamplers[gltfModel.textures[texIndex].sampler]);
        }

        if(gltfMat.values.find("metallicFactor") != gltfMat.values.end())
        {
            mat.metallicFactor = static_cast<float>(gltfMat.values.at("metallicFactor").Factor());
        }
        if(gltfMat.values.find("roughnessFactor") != gltfMat.values.end())
        {
            mat.roughnessFactor = static_cast<float>(gltfMat.values.at("roughnessFactor").Factor());
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
            mat.emissiveFactor = glm::vec4(static_cast<float>(ef[0]), static_cast<float>(ef[1]), static_cast<float>(ef[2]), 1);
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
            if(ext.Has("emissiveStrength")) { mat.emissiveStrength = static_cast<float>(ext.Get("emissiveStrength").Get<double>()); }
        }

        if(gltfMat.additionalValues.find("alphaMode") != gltfMat.additionalValues.end())
        {
            auto& am = gltfMat.additionalValues.at("alphaMode");
            if(am.string_value == "MASK")
            {
                mat.alphaMode = Material::ALPHAMODE_MASK;
                if(gltfMat.additionalValues.find("alphaCutoff") != gltfMat.additionalValues.end())
                {
                    mat.alphaCutoff = static_cast<float>(gltfMat.additionalValues.at("alphaCutoff").Factor());
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
            shaderMaterial.specularFactor = glm::vec4(material.extension.specularFactor, 1.0f);
        }

        material.index = ResourceManager::RequestMaterial(shaderMaterial);
        shaderMaterials.push_back(shaderMaterial);
    }
}

void AccumulateWorldMatrices(Node* node, const glm::mat4& parentWorld, std::vector<glm::mat4>& out)
{
    glm::mat4 world = parentWorld * node->matrix;
    out[node->index] = world;

    for(Node* child: node->children) { AccumulateWorldMatrices(child, world, out); }
}

std::vector<glm::mat4> Model::GetMatrixVector()
{
    std::vector<glm::mat4> flat(m_nodes.size());
    for(Node* root: m_nodes) { AccumulateWorldMatrices(root, glm::mat4(1.0f), flat); }
    return flat;
}

void Model::UpdateMaterialBatches(Node* node)
{
    if(node->mesh)
    {
        for(auto* prim: node->mesh->primitives) { m_materialBatches[prim->material.index].push_back(prim); }
    }
    for(auto& c: node->children) { UpdateMaterialBatches(c); }
}

void CalculateNodeBoundsRecursive(BoundingBox& bounds, const Node* node, const std::vector<Model::Vertex>& vertexBufferLocal,
                                  const std::vector<uint32_t>& indexBufferLocal)
{
    if(!node) { return; }

    // Compute this node's world‐transform
    glm::mat4 nodeWorldMatrix = node->GetMatrix();

    if(node->mesh)
    {
        for(const Primitive* primitive: node->mesh->primitives)
        {
            if(!primitive) { continue; }

            if(primitive->hasIndices)
            {
                // For indexed geometry: primitive->firstIndex/ indexCount refer into indexBufferLocal
                for(int i = 0; i < primitive->indexCount; ++i)
                {
                    // localIndex = indexBufferLocal[firstIndex + i]
                    uint32_t localIndex = indexBufferLocal[primitive->firstIndex + i];
                    // That localIndex indexes directly into vertexBufferLocal
                    if(localIndex < vertexBufferLocal.size())
                    {
                        const glm::vec3& localPos = glm::vec3(vertexBufferLocal[localIndex].position);
                        glm::vec4        worldPos = nodeWorldMatrix * glm::vec4(localPos, 1.0f);

                        bounds.min = glm::min(bounds.min, glm::vec3(worldPos));
                        bounds.max = glm::max(bounds.max, glm::vec3(worldPos));
                        bounds.valid = true;
                    }
                    else { HGWARN("Invalid localIndex %u (vertexBufferLocal has %zu elements)", localIndex, vertexBufferLocal.size()); }
                }
            }
            else
            {
                // Non‐indexed: firstIndex is actually a vertex‐offset into vertexBufferLocal
                for(int i = 0; i < primitive->vertexCount; ++i)
                {
                    uint32_t vertexIndex = primitive->firstIndex + i;
                    if(vertexIndex < vertexBufferLocal.size())
                    {
                        const glm::vec3& localPos = glm::vec3(vertexBufferLocal[vertexIndex].position);
                        glm::vec4        worldPos = nodeWorldMatrix * glm::vec4(localPos, 1.0f);

                        bounds.min = glm::min(bounds.min, glm::vec3(worldPos));
                        bounds.max = glm::max(bounds.max, glm::vec3(worldPos));
                        bounds.valid = true;
                    }
                    else { HGWARN("Invalid vertexIndex %u (vertexBufferLocal has %zu elements)", vertexIndex, vertexBufferLocal.size()); }
                }
            }
        }
    }

    // Recurse into children
    for(const Node* child: node->children) { CalculateNodeBoundsRecursive(bounds, child, vertexBufferLocal, indexBufferLocal); }
}

BoundingBox Model::CalculateModelAABB(const std::vector<Node*>& rootNodes, const std::vector<Model::Vertex>& vertexBuffer,
                                      const std::vector<uint32_t>& indexBuffer)
{

    BoundingBox boundingBox{};
    for(const Node* rootNode: rootNodes) { CalculateNodeBoundsRecursive(boundingBox, rootNode, vertexBuffer, indexBuffer); }

    if(boundingBox.valid)
    {
        boundingBox.corners[0] = glm::vec4(boundingBox.min.x, boundingBox.min.y, boundingBox.min.z, 1.0f);
        boundingBox.corners[1] = glm::vec4(boundingBox.max.x, boundingBox.min.y, boundingBox.min.z, 1.0f);
        boundingBox.corners[2] = glm::vec4(boundingBox.max.x, boundingBox.max.y, boundingBox.min.z, 1.0f);
        boundingBox.corners[3] = glm::vec4(boundingBox.min.x, boundingBox.max.y, boundingBox.min.z, 1.0f);
        boundingBox.corners[4] = glm::vec4(boundingBox.min.x, boundingBox.min.y, boundingBox.max.z, 1.0f);
        boundingBox.corners[5] = glm::vec4(boundingBox.max.x, boundingBox.min.y, boundingBox.max.z, 1.0f);
        boundingBox.corners[6] = glm::vec4(boundingBox.max.x, boundingBox.max.y, boundingBox.max.z, 1.0f);
        boundingBox.corners[7] = glm::vec4(boundingBox.min.x, boundingBox.max.y, boundingBox.max.z, 1.0f);
    }

    return boundingBox;
}

} // namespace Humongous
