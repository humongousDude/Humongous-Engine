#pragma once

#include "singleton.hpp"
#include <string>
#include <unordered_map>
namespace Humongous
{
namespace Systems
{

class AssetManager : public Singleton<AssetManager>
{
public:
    enum class AssetType
    {
        SHADER,
        MODEL
    };

    void Init();

    // returns a std::string path to the requested asset. returns "" if it doesn't exist
    std::string GetAsset(const AssetType type, const std::string_view asset);

private:
    std::unordered_map<std::string, std::string> m_shaderMap;
    std::unordered_map<std::string, std::string> m_modelMap;
};

} // namespace Systems
} // namespace Humongous
