#include "defines.hpp"
#include "logical_device.hpp"
#include <memory>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "texture.hpp"
#include <glm/fwd.hpp>
#include <glm/glm.hpp>

#include <abstractions/buffer.hpp>
#include <abstractions/descriptor_writer.hpp>

namespace Humongous
{
struct ModelPushConstants
{
    glm::mat4 model{1.f};
    alignas(16) glm::mat3 normal{1.f};
    alignas(16) VkDeviceAddress vertexAddress{0};
};

// TODO: maybe move this?
struct Vertex
{
    alignas(16) glm::vec3 position{};
    alignas(16) glm::vec3 color{};
    alignas(8) glm::vec2 uv{};

    static std::vector<VkVertexInputBindingDescription>   GetBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};

class Model
{
public:
    Model(LogicalDevice& logicalDevice, const std::vector<Vertex>& vertices, const std::vector<u32>& indices, const std::string& imagePath);
    ~Model();

    void WriteDescriptorSet(DescriptorSetLayout& layout, DescriptorPool& pool, VkDescriptorSet descriptorSet);

    void Bind(VkCommandBuffer cmd);
    void Draw(VkCommandBuffer cmd);

    VkDeviceAddress GetVertexBufferAddress() const
    {
        m_vertexBuffer->UpdateAddress(m_vertexBuffer->GetUsageFlags());
        return m_vertexBuffer->GetDeviceAddress();
    }

private:
    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;

    std::unique_ptr<Texture> m_texture;

    LogicalDevice& m_logicalDevice;

    u32 m_vertexCount;
    u32 m_indexCount;

    void CreateVertexBuffer(const std::vector<Vertex>& vertices);
    void CreateIndexBuffer(const std::vector<u32>& indices);
};
} // namespace Humongous
