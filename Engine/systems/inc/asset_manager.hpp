#pragma once

#include "singleton.hpp"
#include <string>
#include <unordered_map>
#include <vector>

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
        MODEL,
        TEXTURE
    };

    // paths must not contain any directorys or files with unicode characters.
    void Init(const std::vector<std::string>* paths = nullptr);

    /***
     * returns a std::string path to the requested asset.
     * if it doesn't find a model, it returns the path to a default model
     * if it doesn't find a texture, it returns a path to an empty texture
     * if it doesn't find a shader, it return ""
     * */
    static std::string GetAsset(const AssetType type, const std::string_view asset) { return Get().Internal_GetAsset(type, asset); }

private:
    std::string Internal_GetAsset(const AssetType type, const std::string_view asset);

    std::unordered_map<std::string, std::string> m_shaderMap;
    std::unordered_map<std::string, std::string> m_modelMap;
    std::unordered_map<std::string, std::string> m_textureMap;
};

} // namespace Systems
} // namespace Humongous
