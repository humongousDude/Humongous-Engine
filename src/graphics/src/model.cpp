#include "abstractions/descriptor_writer.hpp"
#include "asserts.hpp"
#include <iostream>
#include <logger.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT

#include <model.hpp>

namespace Humongous
{
Primitive::Primitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, Material& material)
    : firstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), material(material)
{
    hasIndices = indexCount > 0;
};

void Primitive::SetBoundingBox(glm::vec3 min, glm::vec3 max)
{
    bb.min = min;
    bb.max = max;
    bb.valid = true;
}

// Mesh
Mesh::Mesh(LogicalDevice* device, glm::mat4 matrix)
{
    this->device = device;
    this->uniformBlock.matrix = matrix;

    uniformBuffer.uniformBuffer.Init(device, sizeof(UniformBlock), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    uniformBuffer.uniformBuffer.Map();

    uniformBuffer.descriptorInfo = uniformBuffer.uniformBuffer.DescriptorInfo();
};

Mesh::~Mesh()
{
    for(Primitive* p: primitives) { delete p; }
}

void Mesh::SetBoundingBox(glm::vec3 min, glm::vec3 max)
{
    bb.min = min;
    bb.max = max;
    bb.valid = true;
}

Model::Model(LogicalDevice* device, const std::string& modelPath, float scale)
{
    HGINFO("Creating model...");
    LoadFromFile(modelPath, device, device->GetGraphicsQueue(), scale);
    HGINFO("Created model");
}

Model::~Model() { Destroy(device->GetVkDevice()); }

void Model::Destroy(VkDevice device)
{
    for(auto& t: textures) { t.Destroy(); }
    emptyTexture.Destroy();

    for(auto node: nodes) { delete node; }
    nodes.resize(0);
    linearNodes.resize(0);
};

void Model::UpdateUBO(Node* node, glm::mat4 matrix)
{
    if(node->mesh)
    {
        node->mesh->uniformBlock.matrix = matrix;
        node->mesh->uniformBuffer.uniformBuffer.WriteToBuffer((void*)&node->mesh->uniformBlock, sizeof(node->mesh->uniformBlock));
    }
}

void Model::UpdateShaderMaterialBuffer(Node* node) {}

void Model::LoadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo,
                     float globalscale)
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

    // Node with children
    if(node.children.size() > 0)
    {
        for(size_t i = 0; i < node.children.size(); i++)
        {
            LoadNode(newNode, model.nodes[node.children[i]], node.children[i], model, loaderInfo, globalscale);
        }
    }

    // Node contains mesh data
    if(node.mesh > -1)
    {
        const tinygltf::Mesh mesh = model.meshes[node.mesh];
        Mesh*                newMesh = new Mesh(device, newNode->matrix);
        for(size_t j = 0; j < mesh.primitives.size(); j++)
        {
            const tinygltf::Primitive& primitive = mesh.primitives[j];
            uint32_t                   vertexStart = static_cast<uint32_t>(loaderInfo.vertexPos);
            uint32_t                   indexStart = static_cast<uint32_t>(loaderInfo.indexPos);
            uint32_t                   indexCount = 0;
            uint32_t                   vertexCount = 0;
            glm::vec3                  posMin{};
            glm::vec3                  posMax{};
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
                bufferPos = reinterpret_cast<const float*>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
                posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
                posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
                vertexCount = static_cast<uint32_t>(posAccessor.count);
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
                    // 	vert.weight0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
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

                indexCount = static_cast<uint32_t>(accessor.count);
                const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                switch(accessor.componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        {
                            const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
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
                new Primitive(indexStart, indexCount, vertexCount, primitive.material > -1 ? materials[primitive.material] : materials.back());
            newPrimitive->SetBoundingBox(posMin, posMax);
            newPrimitive->owner = newNode;
            newMesh->primitives.push_back(newPrimitive);
        }
        // Mesh BB from BBs of primitives
        for(auto p: newMesh->primitives)
        {
            if(p->bb.valid && !newMesh->bb.valid)
            {
                newMesh->bb = p->bb;
                newMesh->bb.valid = true;
            }
            newMesh->bb.min = glm::min(newMesh->bb.min, p->bb.min);
            newMesh->bb.max = glm::max(newMesh->bb.max, p->bb.max);
        }
        newNode->mesh = newMesh;
    }
    if(parent) { parent->children.push_back(newNode); }
    else { nodes.push_back(newNode); }
    linearNodes.push_back(newNode);
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

VkSamplerAddressMode Model::GetVkWrapMode(int32_t wrapMode)
{
    switch(wrapMode)
    {
        case -1:
        case 10497:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case 33071:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case 33648:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }

    std::cerr << "Unknown wrap mode for getVkWrapMode: " << wrapMode << std::endl;
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkFilter Model::GetVkFilterMode(int32_t filterMode)
{
    switch(filterMode)
    {
        case -1:
        case 9728:
            return VK_FILTER_NEAREST;
        case 9729:
            return VK_FILTER_LINEAR;
        case 9984:
            return VK_FILTER_NEAREST;
        case 9985:
            return VK_FILTER_NEAREST;
        case 9986:
            return VK_FILTER_LINEAR;
        case 9987:
            return VK_FILTER_LINEAR;
    }

    std::cerr << "Unknown filter mode for getVkFilterMode: " << filterMode << std::endl;
    return VK_FILTER_NEAREST;
}

void Model::LoadTextures(tinygltf::Model& gltfModel, LogicalDevice* device, VkQueue transferQueue)
{
    for(tinygltf::Texture& tex: gltfModel.textures)
    {
        tinygltf::Image         image = gltfModel.images[tex.source];
        Texture::TexSamplerInfo textureSampler;
        if(tex.sampler == -1)
        {
            // No sampler specified, use a default one
            textureSampler.magFilter = VK_FILTER_LINEAR;
            textureSampler.minFilter = VK_FILTER_LINEAR;
            textureSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            textureSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            textureSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
        else { textureSampler = textureSamplers[tex.sampler]; }
        Texture texture;
        texture.CreateFromGLTFImage(image, textureSampler, device, transferQueue);
        textures.push_back(texture);
    }

    emptyTexture.CreateFromFile("textures/empty.ktx", device, Texture::ImageType::TEX2D);
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
        textureSamplers.push_back(sampler);
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
            material.baseColorTexture = &textures[mat.values["baseColorTexture"].TextureIndex()];
            material.texCoordSets.baseColor = mat.values["baseColorTexture"].TextureTexCoord();
        }
        if(mat.values.find("metallicRoughnessTexture") != mat.values.end())
        {
            material.metallicRoughnessTexture = &textures[mat.values["metallicRoughnessTexture"].TextureIndex()];
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
            material.normalTexture = &textures[mat.additionalValues["normalTexture"].TextureIndex()];
            material.texCoordSets.normal = mat.additionalValues["normalTexture"].TextureTexCoord();
        }
        if(mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
        {
            material.emissiveTexture = &textures[mat.additionalValues["emissiveTexture"].TextureIndex()];
            material.texCoordSets.emissive = mat.additionalValues["emissiveTexture"].TextureTexCoord();
        }
        if(mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
        {
            material.occlusionTexture = &textures[mat.additionalValues["occlusionTexture"].TextureIndex()];
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
        // @TODO: Find out if there is a nicer way of reading these properties with recent tinygltf headers
        if(mat.extensions.find("KHR_materials_pbrSpecularGlossiness") != mat.extensions.end())
        {
            auto ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
            if(ext->second.Has("specularGlossinessTexture"))
            {
                auto index = ext->second.Get("specularGlossinessTexture").Get("index");
                material.extension.specularGlossinessTexture = &textures[index.Get<int>()];
                auto texCoordSet = ext->second.Get("specularGlossinessTexture").Get("texCoord");
                material.texCoordSets.specularGlossiness = texCoordSet.Get<int>();
                material.pbrWorkflows.specularGlossiness = true;
            }
            if(ext->second.Has("diffuseTexture"))
            {
                auto index = ext->second.Get("diffuseTexture").Get("index");
                material.extension.diffuseTexture = &textures[index.Get<int>()];
            }
            if(ext->second.Has("diffuseFactor"))
            {
                auto factor = ext->second.Get("diffuseFactor");
                for(uint32_t i = 0; i < factor.ArrayLen(); i++)
                {
                    auto val = factor.Get(i);
                    material.extension.diffuseFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                }
            }
            if(ext->second.Has("specularFactor"))
            {
                auto factor = ext->second.Get("specularFactor");
                for(uint32_t i = 0; i < factor.ArrayLen(); i++)
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

        u32 index = static_cast<u32>(materials.size());

        material.index = index;
        material.name = mat.name;
        materials.push_back(material);

        std::vector<Primitive*> empty{};
        materialBatches.emplace(index, empty);
    }

    // Push a default material at the end of the list for meshes with no material assigned
    materials.push_back(Material());

    std::vector<Primitive*> empt{};
    materialBatches.emplace(materialBatches.size(), empt);

    // doesn't work

    // std::vector<Material>::iterator it;
    // for(it = materials.begin(); it != materials.end();)
    // {
    //     if(it->alphaMode == Material::ALPHAMODE_MASK || it->alphaMode == Material::ALPHAMODE_BLEND)
    //     {
    //         auto mov = std::move(*it);
    //
    //         // it = materials.erase(it);
    //         auto nextIt = std::next(it);
    //         std::rotate(it, nextIt, materials.end());
    //
    //         u32 oldIndex = mov.index;
    //         u32 newIndex = materials.size() - 1;
    //
    //         mov.index = newIndex;
    //
    //         auto itBatch = materialBatches.find(oldIndex);
    //         if(itBatch != materialBatches.end())
    //         {
    //             materialBatches[newIndex] = std::move(itBatch->second);
    //             materialBatches.erase(itBatch);
    //         }
    //
    //         it = nextIt;
    //     }
    //     else { ++it; }
    // }
}

void Model::LoadFromFile(std::string filename, LogicalDevice* device, VkQueue transferQueue, float scale)
{
    tinygltf::Model    gltfModel;
    tinygltf::TinyGLTF gltfContext;

    std::string error;
    std::string warning;

    this->device = device;

    bool   binary = false;
    size_t extpos = filename.rfind('.', filename.length());
    if(extpos != std::string::npos) { binary = (filename.substr(extpos + 1, filename.length() - extpos) == "glb"); }

    bool fileLoaded = binary ? gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, filename.c_str())
                             : gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filename.c_str());

    LoaderInfo loaderInfo{};
    size_t     vertexCount = 0;
    size_t     indexCount = 0;

    if(fileLoaded)
    {
        LoadTextureSamplers(gltfModel);
        LoadTextures(gltfModel, device, transferQueue);
        LoadMaterials(gltfModel);

        const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

        // Get vertex and index buffer sizes up-front
        for(size_t i = 0; i < scene.nodes.size(); i++) { GetNodeProps(gltfModel.nodes[scene.nodes[i]], gltfModel, vertexCount, indexCount); }
        loaderInfo.vertexBuffer = new Vertex[vertexCount];
        loaderInfo.indexBuffer = new uint32_t[indexCount];

        // TODO: scene handling with no default scene
        for(size_t i = 0; i < scene.nodes.size(); i++)
        {
            const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
            LoadNode(nullptr, node, scene.nodes[i], gltfModel, loaderInfo, scale);
        }
        /* if(gltfModel.animations.size() > 0) { loadAnimations(gltfModel); }
        loadSkins(gltfModel); */

        for(auto node: linearNodes)
        {
            // // Assign skins
            // if(node->skinIndex > -1) { node->skin = skins[node->skinIndex]; }
            // Initial pose
            if(node->mesh) { node->Update(); }
        }
    }
    else
    {
        // TODO: throw
        HGERROR(error.c_str());
        return;
    }

    // extensions = gltfModel.extensionsUsed;

    size_t vertexBufferSize = vertexCount * sizeof(Vertex);
    size_t indexBufferSize = indexCount * sizeof(uint32_t);

    HGASSERT(vertexBufferSize > 0);

    Buffer vertexStaging{device,
                         vertexBufferSize,
                         1,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};
    vertexStaging.Map();

    // Create staging buffers
    // Vertex data
    vertexStaging.WriteToBuffer((void*)loaderInfo.vertexBuffer);
    // Index data
    Buffer indexStaging{};
    if(indexBufferSize > 0)
    {
        indexStaging.Init(device, indexBufferSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        indexStaging.Map();
        indexStaging.WriteToBuffer((void*)loaderInfo.indexBuffer);
    }

    // Create device local buffers
    // Vertex buffer
    vertices.Init(device, vertexBufferSize, 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    // Index buffer
    if(indexBufferSize > 0)
    {
        indices.Init(device, indexBufferSize, 1, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    }

    // Copy from staging buffers
    Buffer::CopyBuffer(*device, indexStaging, indices, indexBufferSize);
    Buffer::CopyBuffer(*device, vertexStaging, vertices, vertexBufferSize);

    delete[] loaderInfo.vertexBuffer;
    delete[] loaderInfo.indexBuffer;

    GetSceneDimensions();
}

void Model::DrawNode(Node* node, VkCommandBuffer commandBuffer, VkPipelineLayout& pipelineLayout)
{
    UpdateUBO(node, node->GetMatrix());
    UpdateShaderMaterialBuffer(node);

    if(node->mesh)
    {
        for(Primitive* primitive: node->mesh->primitives)
        {
            std::vector<VkDescriptorSet> descriptorSets{primitive->material.descriptorSet, descriptorSetMaterials};

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, static_cast<uint32_t>(descriptorSets.size()),
                                    descriptorSets.data(), 0, nullptr);
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Model::PushConstantData), sizeof(u32),
                               &primitive->material.index);

            vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
        }
    }
    for(auto& child: node->children) { DrawNode(child, commandBuffer, pipelineLayout); }
}

void Model::CalculateBoundingBox(Node* node, Node* parent)
{
    BoundingBox* parentBvh = parent ? &parent->bvh : nullptr;

    if(node->mesh)
    {
        if(node->mesh->bb.valid)
        {
            node->aabb = node->mesh->bb.GetAABB(node->GetMatrix());
            if(node->children.size() == 0)
            {
                node->bvh.min = node->aabb.min;
                node->bvh.max = node->aabb.max;
                node->bvh.valid = true;
            }
        }
    }

    if(parentBvh)
    {
        parentBvh->min = glm::min(parentBvh->min, node->bvh.min);
        parentBvh->max = glm::min(parentBvh->max, node->bvh.max);
    }

    for(auto& child: node->children) { CalculateBoundingBox(child, node); }
}

void Model::GetSceneDimensions()
{
    // Calculate binary volume hierarchy for all nodes in the scene
    for(auto node: linearNodes) { CalculateBoundingBox(node, nullptr); }

    dimensions.min = glm::vec3(FLT_MAX);
    dimensions.max = glm::vec3(-FLT_MAX);

    for(auto node: linearNodes)
    {
        if(node->bvh.valid)
        {
            dimensions.min = glm::min(dimensions.min, node->bvh.min);
            dimensions.max = glm::max(dimensions.max, node->bvh.max);
        }
    }

    // Calculate scene aabb
    aabb = glm::scale(glm::mat4(1.0f), glm::vec3(dimensions.max[0] - dimensions.min[0], dimensions.max[1] - dimensions.min[1],
                                                 dimensions.max[2] - dimensions.min[2]));
    aabb[3][0] = dimensions.min[0];
    aabb[3][1] = dimensions.min[1];
    aabb[3][2] = dimensions.min[2];
}

Node* Model::FindNode(Node* parent, uint32_t index)
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

Node* Model::NodeFromIndex(uint32_t index)
{
    Node* nodeFound = nullptr;
    for(auto& node: nodes)
    {
        nodeFound = FindNode(node, index);
        if(nodeFound) { break; }
    }
    return nodeFound;
}

void Model::Draw(VkCommandBuffer commandBuffer, VkPipelineLayout& pipelineLayout)
{
    const VkDeviceSize offsets[] = {0};
    vkCmdBindIndexBuffer(commandBuffer, this->indices.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    for(auto& [id, prim]: materialBatches)
    {
        auto mat = &materials[id];

        if(mat->descriptorSet == VK_NULL_HANDLE)
        {
            HGERROR("Invalid material descriptor set, did you forget to allocate it?");
            HGERROR("Material: %d, %s", id, mat->name.c_str());
        }

        for(auto& primitive: prim)
        {
            std::vector<VkDescriptorSet> descriptorSets{mat->descriptorSet, descriptorSetMaterials};

            if(primitive->owner->mesh) { descriptorSets.push_back(primitive->owner->mesh->uniformBuffer.descriptorSet); }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, static_cast<uint32_t>(descriptorSets.size()),
                                    descriptorSets.data(), 0, nullptr);

            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Model::PushConstantData), sizeof(u32),
                               &mat->index);

            vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
        }
    }

    // for(auto& node: nodes) { DrawNode(node, commandBuffer, pipelineLayout); }
}

void Model::Init(DescriptorSetLayout* materialLayout, DescriptorSetLayout* nodeLayout, DescriptorSetLayout* materialBufferLayout,
                 DescriptorPoolGrowable* imagePool, DescriptorPoolGrowable* uniformPool, DescriptorPoolGrowable* storagePool)
{
    if(initialized) { return; }
    HGINFO("Initializing model...");

    for(auto& [id, vec]: materialBatches)
    {
        auto material = &materials[id];

        if(material->descriptorSet == VK_NULL_HANDLE)
        {
            material->descriptorSet = imagePool->AllocateDescriptor(materialLayout->GetDescriptorSetLayout());
        }

        std::vector<VkDescriptorImageInfo> imageDescriptors = {
            emptyTexture.GetDescriptorInfo(), emptyTexture.GetDescriptorInfo(),
            material->normalTexture ? material->normalTexture->GetDescriptorInfo() : emptyTexture.GetDescriptorInfo(),
            material->occlusionTexture ? material->occlusionTexture->GetDescriptorInfo() : emptyTexture.GetDescriptorInfo(),
            material->emissiveTexture ? material->emissiveTexture->GetDescriptorInfo() : emptyTexture.GetDescriptorInfo()};

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

        std::array<VkWriteDescriptorSet, 5> writeDescriptorSets{};
        for(size_t i = 0; i < imageDescriptors.size(); i++)
        {
            writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSets[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSets[i].descriptorCount = 1;
            writeDescriptorSets[i].dstSet = material->descriptorSet;
            writeDescriptorSets[i].dstBinding = static_cast<uint32_t>(i);
            writeDescriptorSets[i].pImageInfo = &imageDescriptors[i];

            DescriptorWriter(*materialLayout, imagePool).WriteImage(static_cast<u32>(i), &imageDescriptors[i]).Overwrite(material->descriptorSet);
        }

        // vkUpdateDescriptorSets(device->GetVkDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
    }

    for(auto& node: nodes)
    {
        SetupNodeDescriptorSet(node, uniformPool, nodeLayout);
        UpdateMaterialBatches(node);
    }

    if(descriptorSetMaterials == VK_NULL_HANDLE)
    {
        descriptorSetMaterials = storagePool->AllocateDescriptor(materialBufferLayout->GetDescriptorSetLayout());
    }

    CreateMaterialBuffer();

    auto bufInfo = shaderMaterialBuffer.DescriptorInfo();
    DescriptorWriter(*materialBufferLayout, storagePool).WriteBuffer(0, &bufInfo).Overwrite(descriptorSetMaterials);

    initialized = true;
}

void Model::UpdateMaterialBatches(Node* node)
{
    if(node->mesh)
    {
        for(auto* prim: node->mesh->primitives) { materialBatches[prim->material.index].push_back(prim); }
    }

    for(auto& c: node->children) { UpdateMaterialBatches(c); }
}

void Model::SetupNodeDescriptorSet(Node* node, DescriptorPoolGrowable* descriptorPool, DescriptorSetLayout* layout)
{
    if(node->mesh)
    {
        if(node->mesh->uniformBuffer.descriptorSet == VK_NULL_HANDLE)
        {
            node->mesh->uniformBuffer.descriptorSet = descriptorPool->AllocateDescriptor(layout->GetDescriptorSetLayout());
        }

        auto bufInfo = node->mesh->uniformBuffer.uniformBuffer.DescriptorInfo();
        DescriptorWriter(*layout, descriptorPool).WriteBuffer(0, &bufInfo).Overwrite(node->mesh->uniformBuffer.descriptorSet);
    }

    for(auto& c: node->children) { SetupNodeDescriptorSet(c, descriptorPool, layout); }
}

void Model::CreateMaterialBuffer()
{
    std::vector<ShaderMaterial> shaderMaterials{};
    for(auto& material: materials)
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
            // Metallic roughness workflow
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
            // Specular glossiness workflow
            shaderMaterial.workflow = static_cast<float>(PBR_WORKFLOW_SPECULAR_GLOSSINESS);
            shaderMaterial.PhysicalDescriptorTextureSet =
                material.extension.specularGlossinessTexture != nullptr ? material.texCoordSets.specularGlossiness : -1;
            shaderMaterial.colorTextureSet = material.extension.diffuseTexture != nullptr ? material.texCoordSets.baseColor : -1;
            shaderMaterial.diffuseFactor = material.extension.diffuseFactor;
            shaderMaterial.specularFactor = glm::vec4(material.extension.specularFactor, 1.0f);
        }

        shaderMaterials.push_back(shaderMaterial);
    }

    VkDeviceSize bufferSize = shaderMaterials.size() * sizeof(ShaderMaterial);
    Buffer       stagingBuffer{device,
                         bufferSize,
                         3,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU};
    stagingBuffer.Map();
    stagingBuffer.WriteToBuffer((void*)shaderMaterials.data(), bufferSize);

    shaderMaterialBuffer.Init(device, bufferSize, 3, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    Buffer::CopyBuffer(*device, stagingBuffer, shaderMaterialBuffer, bufferSize);
}

} // namespace Humongous
