#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Humongous
{

class IAssetManager
{
public:
    enum class AssetType
    {
        SHADER,
        MODEL,
        TEXTURE,
        AUDIO
    };

    virtual ~IAssetManager() = default;

    virtual std::string GetAsset(const AssetType type, const std::string_view asset) const = 0;
};

class AssetManager : public IAssetManager
{
public:
    // paths must not contain any directorys or files with unicode characters.
    AssetManager(const std::vector<std::string>* paths = nullptr);

    /***
     * returns a std::string path to the requested asset.
     * if it doesn't find a model, it returns the path to a default model
     * if it doesn't find a texture, it returns a path to an empty texture
     * if it doesn't find a shader, it returns ""
     * if it doesn't find an audio source, it returns a default one
     * */
    std::string GetAsset(const AssetType type, const std::string_view asset) const override;

private:
    void Init(const std::vector<std::string>* paths = nullptr);

    std::unordered_map<std::string, std::string> m_shaderMap;
    std::unordered_map<std::string, std::string> m_modelMap;
    std::unordered_map<std::string, std::string> m_textureMap;
    std::unordered_map<std::string, std::string> m_audioMap;
};

} // namespace Humongous
