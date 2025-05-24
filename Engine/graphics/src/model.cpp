#include "model.hpp"
#include "abstractions/descriptor_writer.hpp"
#include "asserts.hpp"
#include "asset_manager.hpp"
#include "defines.hpp"
#include "globals.hpp"
#include "iostream"
#include "logger.hpp"
#include "resource_manager.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT

#include "tiny_gltf.h"

namespace Humongous
{
Primitive::Primitive(n32 firstIndex, n32 indexCount, n32 vertexCount, Material& material)
    : firstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), material(material)
{
    hasIndices = indexCount > 0;
};

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

void Model::Destroy(vk::Device device)
{
    for(auto& t: m_textures) { t.Destroy(); }
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
                     float globalscale, glm::mat4 parentTransform) // Add parentTransform as parameter
{
    Node* newNode = new Node{};
    newNode->index = nodeIndex;
    newNode->parent = parent;
    newNode->name = node.name;
    // newNode->skinIndex = node.skin;
    newNode->matrix = glm::mat4(1.0f);

    // Generate local node matrix
    glm::vec3 translation = glm::vec3(0.0f);
    if(node.translation.size() == 3)
    {
        translation = glm::make_vec3(node.translation.data());
        newNode->translation = translation;
    }
    glm::mat4 rotation = glm::mat4(1.0f);
    if(node.rotation.size() == 4)
    {
        glm::quat q = glm::make_quat(node.rotation.data());
        newNode->rotation = glm::mat4(q);
    }
    glm::vec3 scale = glm::vec3(1.0f);
    if(node.scale.size() == 3)
    {
        scale = glm::make_vec3(node.scale.data());
        newNode->scale = scale;
    }
    if(node.matrix.size() == 16) { newNode->matrix = glm::make_mat4x4(node.matrix.data()); };

    glm::mat4 currentTransform = parentTransform * newNode->matrix; // Calculate current transform

    // Node with children
    if(node.children.size() > 0)
    {
        for(size_t i = 0; i < node.children.size(); i++)
        {
            LoadNode(newNode, model.nodes[node.children[i]], node.children[i], model, loaderInfo, globalscale,
                     currentTransform); // Pass currentTransform
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
            glm::vec3                  posMin{}; // Not needed anymore, using m_localAABB directly
            glm::vec3                  posMax{}; // Not needed anymore, using m_localAABB directly
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

                int posByteStride;
                int normByteStride;
                int uv0ByteStride;
                int uv1ByteStride;
                int color0ByteStride;
                int jointByteStride;
                int weightByteStride;

                int jointComponentType;

                // Position attribute is required
                HGASSERT(primitive.attributes.find("POSITION") != primitive.attributes.end());

                const tinygltf::Accessor&   posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
                const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
                bufferPos = reinterpret_cast<const float*>(model.buffers[posView.buffer].data.data() + posView.byteOffset + posAccessor.byteOffset);
                vertexCount = static_cast<n32>(posAccessor.count);
                posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float))
                                                                : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);

                if(primitive.attributes.find("NORMAL") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                    bufferNormals =
                        reinterpret_cast<const float*>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
                    normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float))
                                                                       : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
                }

                // UVs
                if(primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor&   uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
                    bufferTexCoordSet0 =
                        reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                    uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float))
                                                                  : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
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

                hasSkin = (bufferJoints && bufferWeights);

                for(size_t v = 0; v < posAccessor.count; v++)
                {
                    Vertex& vert = loaderInfo.vertexBuffer[loaderInfo.vertexPos];
                    vert.position = glm::vec4(glm::make_vec3(&bufferPos[v * posByteStride]), 1.0f);
                    vert.normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * normByteStride]) : glm::vec3(0.0f)));
                    vert.uv0 = bufferTexCoordSet0 ? glm::make_vec2(&bufferTexCoordSet0[v * uv0ByteStride]) : glm::vec3(0.0f);
                    vert.uv1 = bufferTexCoordSet1 ? glm::make_vec2(&bufferTexCoordSet1[v * uv1ByteStride]) : glm::vec3(0.0f);
                    vert.color = bufferColorSet0 ? glm::make_vec4(&bufferColorSet0[v * color0ByteStride]) : glm::vec4(1.0f);

                    glm::mat4 coordinateTransform =
                        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate -90 degrees around X-axis
                    glm::vec4 transformedPos = coordinateTransform * glm::vec4(vert.position, 1.0f);
                    vert.position = glm::vec3(transformedPos);

                    if(hasSkin)
                    {
                        switch(jointComponentType)
                        {
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                                {
                                    const uint16_t* buf = static_cast<const uint16_t*>(bufferJoints);
                                    // vert.joint0 = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
                                    break;
                                }
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                                {
                                    const uint8_t* buf = static_cast<const uint8_t*>(bufferJoints);
                                    // vert.joint0 = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
                                    break;
                                }
                            default:
                                // Not supported by spec
                                std::cerr << "Joint component type " << jointComponentType << " not supported!" << std::endl;
                                break;
                        }
                    }
                    else
                    {
                        // vert.joint0 = glm::vec4(0.0f);
                    }
                    // vert.weight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * weightByteStride]) : glm::vec4(0.0f);
                    // Fix for all zero weights
                    // if (glm::length(vert.weight0) == 0.0f) {
                    //     vert.weight0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                    // }
                    loaderInfo.vertexPos++;
                }
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
                            for(size_t index = 0; index < accessor.count; index++)
                            {
                                loaderInfo.indexBuffer[loaderInfo.indexPos] = buf[index] + vertexStart;
                                loaderInfo.indexPos++;
                            }
                            break;
                        }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        {
                            const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++)
                            {
                                loaderInfo.indexBuffer[loaderInfo.indexPos] = buf[index] + vertexStart;
                                loaderInfo.indexPos++;
                            }
                            break;
                        }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        {
                            const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
                            for(size_t index = 0; index < accessor.count; index++)
                            {
                                loaderInfo.indexBuffer[loaderInfo.indexPos] = buf[index] + vertexStart;
                                loaderInfo.indexPos++;
                            }
                            break;
                        }
                    default:
                        std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                        return;
                }
            }
            Primitive* newPrimitive =
                new Primitive(indexStart, indexCount, vertexCount, primitive.material > -1 ? m_materials[primitive.material] : m_materials.back());
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

    std::cerr << "Unknown wrap mode for getvk::WrapMode: " << wrapMode << std::endl;
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

    std::cerr << "Unknown filter mode for getvk::FilterMode: " << filterMode << std::endl;
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
        Texture texture;
        texture.CreateFromGLTFImage(image, textureSampler, device, transferQueue);
        m_textures.push_back(std::move(texture));
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
    int i = 0;

    for(tinygltf::Material& mat: gltfModel.materials)
    {
        Material material{};
        material.doubleSided = mat.doubleSided;

        if(mat.values.find("baseColorTexture") != mat.values.end())
        {
            material.baseColorTexture = &m_textures[mat.values["baseColorTexture"].TextureIndex()];
            material.texCoordSets.baseColor = mat.values["baseColorTexture"].TextureTexCoord();
        }
        if(mat.values.find("metallicRoughnessTexture") != mat.values.end())
        {
            material.metallicRoughnessTexture = &m_textures[mat.values["metallicRoughnessTexture"].TextureIndex()];
            material.texCoordSets.metallicRoughness = mat.values["metallicRoughnessTexture"].TextureTexCoord();
        }
        if(mat.values.find("roughnessFactor") != mat.values.end())
        {
            material.roughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
        }
        if(mat.values.find("metallicFactor") != mat.values.end())
        {
            material.metallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
        }
        if(mat.values.find("baseColorFactor") != mat.values.end())
        {
            material.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
        }
        if(mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
        {
            material.normalTexture = &m_textures[mat.additionalValues["normalTexture"].TextureIndex()];
            material.texCoordSets.normal = mat.additionalValues["normalTexture"].TextureTexCoord();
        }
        if(mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
        {
            material.emissiveTexture = &m_textures[mat.additionalValues["emissiveTexture"].TextureIndex()];
            material.texCoordSets.emissive = mat.additionalValues["emissiveTexture"].TextureTexCoord();
        }
        if(mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
        {
            material.occlusionTexture = &m_textures[mat.additionalValues["occlusionTexture"].TextureIndex()];
            material.texCoordSets.occlusion = mat.additionalValues["occlusionTexture"].TextureTexCoord();
        }
        if(mat.additionalValues.find("alphaMode") != mat.additionalValues.end())
        {
            tinygltf::Parameter param = mat.additionalValues["alphaMode"];
            if(param.string_value == "BLEND") { material.alphaMode = Material::ALPHAMODE_BLEND; }
            if(param.string_value == "MASK")
            {
                material.alphaCutoff = 0.5f;
                material.alphaMode = Material::ALPHAMODE_MASK;
            }
        }
        if(mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end())
        {
            material.alphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
        }
        if(mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end())
        {
            material.emissiveFactor = glm::vec4(glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
        }

        // Extensions
        // TODO: Find out if there is a nicer way of reading these properties with recent tinygltf headers
        if(mat.extensions.find("KHR_materials_pbrSpecularGlossiness") != mat.extensions.end())
        {
            auto ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
            if(ext->second.Has("specularGlossinessTexture"))
            {
                auto index = ext->second.Get("specularGlossinessTexture").Get("index");
                material.extension.specularGlossinessTexture = &m_textures[index.Get<int>()];
                auto texCoordSet = ext->second.Get("specularGlossinessTexture").Get("texCoord");
                material.texCoordSets.specularGlossiness = texCoordSet.Get<int>();
                material.pbrWorkflows.specularGlossiness = true;
            }
            if(ext->second.Has("diffuseTexture"))
            {
                auto index = ext->second.Get("diffuseTexture").Get("index");
                material.extension.diffuseTexture = &m_textures[index.Get<int>()];
            }
            if(ext->second.Has("diffuseFactor"))
            {
                auto factor = ext->second.Get("diffuseFactor");
                for(n32 i = 0; i < factor.ArrayLen(); i++)
                {
                    auto val = factor.Get(i);
                    material.extension.diffuseFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                }
            }
            if(ext->second.Has("specularFactor"))
            {
                auto factor = ext->second.Get("specularFactor");
                for(n32 i = 0; i < factor.ArrayLen(); i++)
                {
                    auto val = factor.Get(i);
                    material.extension.specularFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                }
            }
        }

        if(mat.extensions.find("KHR_materials_unlit") != mat.extensions.end()) { material.unlit = true; }

        if(mat.extensions.find("KHR_materials_emissive_strength") != mat.extensions.end())
        {
            auto ext = mat.extensions.find("KHR_materials_emissive_strength");
            if(ext->second.Has("emissiveStrength"))
            {
                auto value = ext->second.Get("emissiveStrength");
                material.emissiveStrength = (float)value.Get<double>();
            }
        }

        n32 index = static_cast<n32>(m_materials.size());

        material.index = index;
        material.name = mat.name;
        m_materials.push_back(material);

        std::vector<Primitive*> empty{};
        m_materialBatches.emplace(index, empty);
    }

    // Push a default material at the end of the list for meshes with no material assigned
    m_materials.push_back(Material());

    std::vector<Primitive*> empt{};
    m_materialBatches.emplace(m_materialBatches.size(), empt);
}

void Model::CreateMaterialDataBuffer()
{
    std::vector<ShaderMaterial> shaderMaterials{};
    for(auto& material: m_materials)
    {
        ShaderMaterial shaderMaterial{};

        shaderMaterial.emissiveFactor = material.emissiveFactor;
        // To save space, availabilty and texture coordinate set are combined
        // -1 = texture not used for this material, >= 0 texture used and index of texture coordinate set
        shaderMaterial.colorTextureSet = material.baseColorTexture != nullptr ? material.texCoordSets.baseColor : -1;
        shaderMaterial.normalTextureSet = material.normalTexture != nullptr ? material.texCoordSets.normal : -1;
        shaderMaterial.occlusionTextureSet = material.occlusionTexture != nullptr ? material.texCoordSets.occlusion : -1;
        shaderMaterial.emissiveTextureSet = material.emissiveTexture != nullptr ? material.texCoordSets.emissive : -1;
        shaderMaterial.alphaMask = static_cast<float>(material.alphaMode == Material::ALPHAMODE_MASK);
        shaderMaterial.alphaMaskCutoff = material.alphaCutoff;
        shaderMaterial.emissiveStrength = material.emissiveStrength;

        // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

        if(material.pbrWorkflows.metallicRoughness)
        {
            shaderMaterial.workflow = static_cast<float>(PBR_WORKFLOW_METALLIC_ROUGHNESS);
            shaderMaterial.baseColorFactor = material.baseColorFactor;
            shaderMaterial.metallicFactor = material.metallicFactor;
            shaderMaterial.roughnessFactor = material.roughnessFactor;
            shaderMaterial.PhysicalDescriptorTextureSet =
                material.metallicRoughnessTexture != nullptr ? material.texCoordSets.metallicRoughness : -1;
            shaderMaterial.colorTextureSet = material.baseColorTexture != nullptr ? material.texCoordSets.baseColor : -1;
        }

        if(material.pbrWorkflows.specularGlossiness)
        {
            shaderMaterial.workflow = static_cast<float>(PBR_WORKFLOW_SPECULAR_GLOSSINESS);
            shaderMaterial.PhysicalDescriptorTextureSet =
                material.extension.specularGlossinessTexture != nullptr ? material.texCoordSets.specularGlossiness : -1;
            shaderMaterial.colorTextureSet = material.extension.diffuseTexture != nullptr ? material.texCoordSets.baseColor : -1;
            shaderMaterial.diffuseFactor = material.extension.diffuseFactor;
            shaderMaterial.specularFactor = glm::vec4(material.extension.specularFactor, 1.0f);
        }

        shaderMaterials.push_back(shaderMaterial);
    }

    vk::DeviceSize bufferSize = shaderMaterials.size() * sizeof(ShaderMaterial);
    Buffer         stagingBuffer{m_logicalDevice,
                         bufferSize,
                         1,
                         vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};
    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer((void*)shaderMaterials.data(), bufferSize);

    m_materialDataBuffer.Init(m_logicalDevice, bufferSize, 1, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
                              vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY);

    Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, m_materialDataBuffer, bufferSize);
}

void Model::LoadFromFile(std::string filePath, LogicalDevice* logicalDevice, vk::Queue transferQueue, float scale)
{
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

    size_t vertexBufferSize = vertexCount * sizeof(Vertex);
    size_t indexBufferSize = indexCount * sizeof(n32);

    HGASSERT(vertexBufferSize > 0);

    Buffer vertexStaging{logicalDevice,
                         vertexBufferSize,
                         1,
                         vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};
    vertexStaging.Map();

    vertexStaging.WriteToBuffer((void*)loaderInfo.vertexBuffer.data());
    Buffer indexStaging{};
    if(indexBufferSize > 0)
    {
        indexStaging.Init(logicalDevice, indexBufferSize, 1, vk::BufferUsageFlagBits::eTransferSrc,
                          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, VMA_MEMORY_USAGE_CPU_TO_GPU);
        indexStaging.Map();
        indexStaging.WriteToBuffer((void*)loaderInfo.indexBuffer.data());
    }

    m_vertices.Init(logicalDevice, vertexBufferSize, 1, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                    vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY);

    if(indexBufferSize > 0)
    {
        m_indices.Init(logicalDevice, indexBufferSize, 1, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                       vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_GPU_ONLY);
    }

    Buffer::CopyBuffer(*logicalDevice, indexStaging, m_indices, indexBufferSize);
    Buffer::CopyBuffer(*logicalDevice, vertexStaging, m_vertices, vertexBufferSize);

    // delete[] loaderInfo.vertexBuffer;
    // delete[] loaderInfo.indexBuffer;

    for(auto& node: m_nodes) { UpdateMaterialBatches(node); }

    SetupIndirectDrawBuffer();

    m_localAABB = CalculateModelAABB(m_nodes, loaderInfo.vertexBuffer, loaderInfo.indexBuffer);
}

void Model::Draw(vk::CommandBuffer cmd, vk::PipelineLayout& pipelineLayout)
{
    vkCmdBindIndexBuffer(cmd, m_indices.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vk::DescriptorSet nodeMatrixSet;
    auto              bufInfo = m_nodeMatrixBuffer.DescriptorInfo();
    DescriptorWriter(*ResourceManager::GetModelDescriptors().nodeLayout, ResourceManager::GetDescriptorPools().storageBufferPool.get())
        .WriteBuffer(0, &bufInfo)
        .Build(nodeMatrixSet);

    std::vector<vk::DescriptorSet> sets = {m_materialDataDescriptor, nodeMatrixSet};

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, static_cast<n32>(Globals::DescriptorSetIndices::Model), sets.size(),
                           sets.data(), 0, nullptr);

    vk::DeviceSize written{0};
    for(auto& [id, prim]: m_materialBatches)
    {
        auto mat = &m_materials[id];

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushConstantData), sizeof(n32), &mat->index);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, static_cast<n32>(Globals::DescriptorSetIndices::Model) + 2, 1,
                               &mat->descriptorSet, 0, nullptr);

        vkCmdDrawIndexedIndirect(cmd, m_indirectDrawBuffer.GetBuffer(), written, m_indirectCommands[id].size(),
                                 sizeof(vk::DrawIndexedIndirectCommand));

        written += m_indirectCommands[id].size() * sizeof(vk::DrawIndexedIndirectCommand);
    }
}

void Model::SetupIndirectDrawBuffer()
{
    vk::DeviceSize         totalWritten = 0;
    std::vector<n32>       nodeID;
    std::vector<glm::mat4> nodeMatricies;

    for(auto& [id, primitives]: m_materialBatches)
    {
        std::vector<vk::DrawIndexedIndirectCommand> commands;

        for(auto* primitive: primitives)
        {
            vk::DrawIndexedIndirectCommand command{};
            command.indexCount = primitive->indexCount;
            command.instanceCount = 1;
            command.firstIndex = primitive->firstIndex;
            command.vertexOffset = 0;
            command.firstInstance = 0;

            nodeID.push_back(primitive->owner->index);
            nodeMatricies.push_back(primitive->owner->GetMatrix());
            if(command.indexCount == 0) { continue; }

            commands.push_back(command);
        }

        totalWritten += commands.size() * sizeof(vk::DrawIndexedIndirectCommand);

        m_indirectCommands.emplace(id, std::move(commands));
    }

    // Node Matricies
    {
        Buffer stagingBuffer{m_logicalDevice,
                             nodeMatricies.size() * sizeof(glm::mat4),
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        m_nodeMatrixBuffer.Init(m_logicalDevice, nodeMatricies.size() * sizeof(glm::mat4), 1,
                                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
                                vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_CPU_COPY);
        stagingBuffer.WriteToBuffer((void*)nodeMatricies.data());
        stagingBuffer.Flush();
        stagingBuffer.UnMap();

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, m_nodeMatrixBuffer, nodeMatricies.size() * sizeof(glm::mat4));
    }

    // Node IDs
    {
        Buffer stagingBuffer{m_logicalDevice,
                             nodeID.size() * sizeof(n32),
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU};
        stagingBuffer.Map();
        m_nodeIDBuffer.Init(m_logicalDevice, nodeID.size() * sizeof(n32), 1,
                            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
                            vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_CPU_COPY);
        stagingBuffer.WriteToBuffer((void*)nodeID.data());
        stagingBuffer.Flush();
        stagingBuffer.UnMap();

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, m_nodeIDBuffer, nodeID.size() * sizeof(n32));
    }

    // Indirect
    if(totalWritten == 0) { return; }
    {
        Buffer stagingBuffer{m_logicalDevice,
                             totalWritten,
                             1,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             4};
        stagingBuffer.Map();

        m_indirectDrawBuffer.Init(m_logicalDevice, totalWritten, 1,
                                  vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                  vk::MemoryPropertyFlagBits::eDeviceLocal, VMA_MEMORY_USAGE_CPU_COPY, 4);

        vk::DeviceSize writtenOffset = 0;
        for(auto& [id, prim]: m_materialBatches)
        {
            stagingBuffer.WriteToBuffer((void*)m_indirectCommands[id].data(),
                                        m_indirectCommands[id].size() * sizeof(vk::DrawIndexedIndirectCommand), writtenOffset);

            writtenOffset += m_indirectCommands[id].size() * sizeof(vk::DrawIndexedIndirectCommand);
        }

        stagingBuffer.Flush();
        stagingBuffer.UnMap();

        Buffer::CopyBuffer(*m_logicalDevice, stagingBuffer, m_indirectDrawBuffer, totalWritten);
    }
}

void Model::Init(DescriptorSetLayout* materialLayout, DescriptorSetLayout* nodeLayout, DescriptorSetLayout* materialBufferLayout,
                 DescriptorPoolGrowable* imagePool, DescriptorPoolGrowable* uniformPool, DescriptorPoolGrowable* storagePool)
{
    if(m_initialized) { return; }
    HGINFO("Initializing model...");

    for(auto& [id, vec]: m_materialBatches)
    {
        auto material = &m_materials[id];

        if(material->descriptorSet == VK_NULL_HANDLE)
        {
            material->descriptorSet = imagePool->AllocateDescriptor(materialLayout->GetDescriptorSetLayout());
        }

        std::vector<vk::DescriptorImageInfo> imageDescriptors = {
            m_emptyTexture.GetDescriptorInfo(), m_emptyTexture.GetDescriptorInfo(),
            material->normalTexture ? material->normalTexture->GetDescriptorInfo() : m_emptyTexture.GetDescriptorInfo(),
            material->occlusionTexture ? material->occlusionTexture->GetDescriptorInfo() : m_emptyTexture.GetDescriptorInfo(),
            material->emissiveTexture ? material->emissiveTexture->GetDescriptorInfo() : m_emptyTexture.GetDescriptorInfo()};

        // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

        if(material->pbrWorkflows.metallicRoughness)
        {
            if(material->baseColorTexture) { imageDescriptors[0] = material->baseColorTexture->GetDescriptorInfo(); }
            if(material->metallicRoughnessTexture) { imageDescriptors[1] = material->metallicRoughnessTexture->GetDescriptorInfo(); }
        }

        if(material->pbrWorkflows.specularGlossiness)
        {

            if(material->extension.diffuseTexture) { imageDescriptors[0] = material->extension.diffuseTexture->GetDescriptorInfo(); }
            if(material->extension.specularGlossinessTexture)
            {
                imageDescriptors[1] = material->extension.specularGlossinessTexture->GetDescriptorInfo();
            }
        }

        std::array<vk::WriteDescriptorSet, 5> writeDescriptorSets{};
        for(size_t i = 0; i < imageDescriptors.size(); i++)
        {
            writeDescriptorSets[i].sType = vk::StructureType::eWriteDescriptorSet;
            writeDescriptorSets[i].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            writeDescriptorSets[i].descriptorCount = 1;
            writeDescriptorSets[i].dstSet = material->descriptorSet;
            writeDescriptorSets[i].dstBinding = static_cast<n32>(i);
            writeDescriptorSets[i].pImageInfo = &imageDescriptors[i];

            DescriptorWriter(*materialLayout, imagePool).WriteImage(static_cast<n32>(i), &imageDescriptors[i]).Overwrite(material->descriptorSet);
        }
    }

    if(m_materialDataDescriptor == VK_NULL_HANDLE)
    {
        m_materialDataDescriptor = storagePool->AllocateDescriptor(materialBufferLayout->GetDescriptorSetLayout());
    }

    CreateMaterialDataBuffer();

    auto bufInfo = m_materialDataBuffer.DescriptorInfo();
    DescriptorWriter(*materialBufferLayout, storagePool).WriteBuffer(0, &bufInfo).Overwrite(m_materialDataDescriptor);

    m_initialized = true;
}

void Model::UpdateMaterialBatches(Node* node)
{
    if(node->mesh)
    {
        for(auto* prim: node->mesh->primitives) { m_materialBatches[prim->material.index].push_back(prim); }
    }
    for(auto& c: node->children) { UpdateMaterialBatches(c); }
}

void CalculateNodeBoundsRecursive(BoundingBox&                      bounds,       // Input/Output: The AABB
                                  const Node*                       node,         // The current node to process
                                  const std::vector<Model::Vertex>& vertexBuffer, // Reference to your vertex data
                                  const std::vector<uint32_t>&      indexBuffer)       // Reference to your index data
{
    if(!node) { return; }

    // Calculate the final world matrix for this node
    glm::mat4 nodeWorldMatrix = node->GetMatrix(); // Use your existing GetMatrix()

    // --- Process Mesh Vertices ---
    if(node->mesh)
    {
        for(const Primitive* primitive: node->mesh->primitives)
        {
            if(!primitive) { continue; }

            if(primitive->hasIndices)
            {
                // Indexed geometry
                if(primitive->indexCount > 0)
                {
                    // Iterate through the indices for this primitive
                    for(n32 i = 0; i < primitive->indexCount; ++i)
                    {
                        // Get the index from the index buffer
                        uint32_t vertexIndex = indexBuffer[primitive->firstIndex + i];

                        // Get the vertex position from the vertex buffer
                        // Add bounds check for safety if needed
                        if(vertexIndex < vertexBuffer.size())
                        {
                            const glm::vec3& localPos = vertexBuffer[vertexIndex].position;

                            // Transform the vertex position
                            glm::vec4 worldPos = nodeWorldMatrix * glm::vec4(localPos, 1.0f);

                            // Update the bounding box
                            bounds.min = glm::min(bounds.min, glm::vec3(worldPos));
                            bounds.max = glm::max(bounds.max, glm::vec3(worldPos));
                            bounds.valid = true;
                        }
                        else
                        {
                            // Handle invalid index if necessary
                        }
                    }
                }
            }
            else
            {
                // Non-indexed geometry (less common for GLTF, but handle it)
                // Assume m_firstIndex is the start vertex offset
                if(primitive->vertexCount > 0)
                {
                    for(n32 i = 0; i < primitive->vertexCount; ++i)
                    {
                        // Calculate the vertex index directly
                        uint32_t vertexIndex = primitive->firstIndex + i; // Assuming m_firstIndex is vertex offset here

                        // Get the vertex position
                        if(vertexIndex < vertexBuffer.size())
                        {
                            const glm::vec3& localPos = vertexBuffer[vertexIndex].position;

                            // Transform the vertex position
                            glm::vec4 worldPos = nodeWorldMatrix * glm::vec4(localPos, 1.0f);

                            // Update the bounding box
                            bounds.min = glm::min(bounds.min, glm::vec3(worldPos));
                            bounds.max = glm::max(bounds.max, glm::vec3(worldPos));
                            bounds.valid = true;
                        }
                        else
                        {
                            // Handle invalid index if necessary
                        }
                    }
                }
            }
        }
    }

    // --- Recurse for Children ---
    for(const Node* child: node->children) { CalculateNodeBoundsRecursive(bounds, child, vertexBuffer, indexBuffer); }
}

BoundingBox Model::CalculateModelAABB(const std::vector<Node*>&         rootNodes,    // Pass the root nodes of your model hierarchy
                                      const std::vector<Model::Vertex>& vertexBuffer, // Your flat vertex buffer
                                      const std::vector<uint32_t>&      indexBuffer)       // Your flat index buffer
{
    BoundingBox boundingBox{}; // Initialized with invalid state

    // Process all root nodes in the scene
    for(const Node* rootNode: rootNodes)
    {
        // Start recursion from each root. No initial parent matrix needed
        // as node->GetMatrix() calculates the full world matrix internally.
        CalculateNodeBoundsRecursive(boundingBox, rootNode, vertexBuffer, indexBuffer);
    }

    // --- Finalize Corners (Optional) ---
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
