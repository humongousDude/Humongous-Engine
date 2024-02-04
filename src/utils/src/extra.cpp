#include <extra.hpp>

#include <fstream>

namespace Humongous
{
namespace Utils
{

std::vector<char> ReadFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if(!file.is_open()) { throw std::runtime_error("failed to open file!"); }

    size_t fileSize = (size_t)file.tellg();

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}
} // namespace Utils
} // namespace Humongous
