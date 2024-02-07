#include "logical_device.hpp"
#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <abstractions/buffer.hpp>
#include <abstractions/descriptor_writer.hpp>

namespace Humongous
{
// TODO: maybe move this?
struct Vertex
{
    glm::vec2 position;
    glm::vec3 color;

    static std::vector<VkVertexInputBindingDescription>   GetBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};

class Model
{
public:
    Model(LogicalDevice& logicalDevice, const std::vector<Vertex>& vertices);
    ~Model();

    void Bind(VkCommandBuffer cmd);
    void Draw(VkCommandBuffer cmd);

private:
    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;

    LogicalDevice& m_logicalDevice;

    u32 m_vertexCount;
    u32 m_indexCount;

    void CreateVertexBuffer(const std::vector<Vertex>& vertices);
};
} // namespace Humongous
