#include "asset_manager.hpp"
#include "audio_source.hpp"
#include "cstring"
#include "filesystem"
#include "logger.hpp"

namespace Humongous::Systems
{

void AssetManager::Internal_Init(const std::vector<std::string>* paths)
{
    namespace fs = std::filesystem;
    const fs::path assetDir = HGASSETDIRPATH;

    HGINFO("Asset directory is: %s", HGASSETDIRPATH);

    if(!fs::exists(assetDir) || !std::filesystem::is_directory(assetDir))
    {
        HGFATAL("Unable to access asset directory!");
        return;
    }

    for(const auto& entry: fs::recursive_directory_iterator(assetDir))
    {
        if(!entry.is_regular_file()) { continue; }

        if(strcmp(entry.path().extension().string().substr(1).c_str(), "spv") == 0)
        {
            std::string fileNameWithExtension = entry.path().filename().string();
            size_t      dotPos = fileNameWithExtension.find('.', fileNameWithExtension.find('.') + 1);
            std::string fileName = entry.path().stem().string().substr(0, dotPos);

            m_shaderMap.emplace(fileName, entry.path().string());
        }
        else if(strcmp(entry.path().extension().string().substr(1).c_str(), "glb") == 0)
        {
            m_modelMap.emplace(entry.path().stem().string(), entry.path().string());
        }
        else if(strcmp(entry.path().extension().string().substr(1).c_str(), "ktx") == 0)
        {
            m_textureMap.emplace(entry.path().stem().string(), entry.path().string());
        }
        else if(strcmp(entry.path().extension().string().substr(1).c_str(), "wav") == 0)
        {
            m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
            HGTRACE("ADDED: %s, %s", entry.path().stem().string().c_str(), entry.path().string().c_str());
        }
        else if(strcmp(entry.path().extension().string().substr(1).c_str(), "mp3") == 0)
        {
            m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
            HGTRACE("ADDED: %s, %s", entry.path().stem().string().c_str(), entry.path().string().c_str());
        }
        else if(strcmp(entry.path().extension().string().substr(1).c_str(), "mp4") == 0)
        {
            m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
            HGTRACE("ADDED: %s, %s", entry.path().stem().string().c_str(), entry.path().string().c_str());
        }
        else if(strcmp(entry.path().extension().string().substr(1).c_str(), "ogg") == 0)
        {
            m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
            HGTRACE("ADDED: %s, %s", entry.path().stem().string().c_str(), entry.path().string().c_str());
        }
        else if(strcmp(entry.path().extension().string().substr(1).c_str(), "flac") == 0)
        {
            m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
            HGTRACE("ADDED: %s, %s", entry.path().stem().string().c_str(), entry.path().string().c_str());
        }
    }

    if(!paths) { return; }

    for(const std::string& p: *paths)
    {
        fs::path path = static_cast<fs::path>(p);

        if(!fs::exists(path) || !std::filesystem::is_directory(path))
        {
            HGERROR("Unable to access requested asset directory: %s", p.c_str());
            continue;
        }
        HGINFO("Looking for extra assets in directoy: %s", p.c_str());

        for(const auto& entry: fs::recursive_directory_iterator(path))
        {
            if(!entry.is_regular_file()) { continue; }

            if(strcmp(entry.path().extension().string().substr(1).c_str(), "spv") == 0)
            {
                std::string fileNameWithExtension = entry.path().filename().string();
                size_t      dotPos = fileNameWithExtension.find('.', fileNameWithExtension.find('.') + 1);
                std::string fileName = entry.path().stem().string().substr(0, dotPos);

                m_shaderMap.emplace(fileName, entry.path().string());
            }
            else if(strcmp(entry.path().extension().string().substr(1).c_str(), "glb") == 0)
            {
                m_modelMap.emplace(entry.path().stem().string(), entry.path().string());
            }
            else if(strcmp(entry.path().extension().string().substr(1).c_str(), "gltf") == 0)
            {
                m_modelMap.emplace(entry.path().stem().string(), entry.path().string());
            }
            else if(strcmp(entry.path().extension().string().substr(1).c_str(), "ktx") == 0)
            {
                m_textureMap.emplace(entry.path().stem().string(), entry.path().string());
            }
            else if(strcmp(entry.path().extension().string().substr(1).c_str(), "wav") == 0)
            {
                m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
                HGDEBUG("Found audio with name: %s", entry.path().stem().string().c_str());
            }
            else if(strcmp(entry.path().extension().string().substr(1).c_str(), "mp3") == 0)
            {
                m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
                HGDEBUG("Found audio with name: %s", entry.path().stem().string().c_str());
            }
            else if(strcmp(entry.path().extension().string().substr(1).c_str(), "mp4") == 0)
            {
                m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
                HGDEBUG("Found audio with name: %s", entry.path().stem().string().c_str());
            }
            else if(strcmp(entry.path().extension().string().substr(1).c_str(), "ogg") == 0)
            {
                m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
                HGDEBUG("Found audio with name: %s", entry.path().stem().string().c_str());
            }
            else if(strcmp(entry.path().extension().string().substr(1).c_str(), "flac") == 0)
            {
                m_audioMap.emplace(entry.path().stem().string(), entry.path().string());
                HGDEBUG("Found audio with name: %s", entry.path().stem().string().c_str());
            }
        }
    }
}

std::string AssetManager::Internal_GetAsset(const AssetType type, const std::string_view asset)
{
    switch(type)
    {
        case AssetType::SHADER:
            if(m_shaderMap.find(static_cast<std::string>(asset)) != m_shaderMap.end()) { return m_shaderMap.at(static_cast<std::string>(asset)); }
            else
            {
                HGWARN("Unable to find requested shader! returning empty string!");
                return "";
            }
            break;

        case AssetType::MODEL:
            if(m_modelMap.find(static_cast<std::string>(asset)) != m_modelMap.end()) { return m_modelMap.at(static_cast<std::string>(asset)); }
            else
            {
                HGWARN("Unable to find requested model! returning default model!");
                return GetAsset(AssetType::MODEL, "default");
            }
            break;

        case AssetType::TEXTURE:
            if(m_textureMap.find(static_cast<std::string>(asset)) != m_textureMap.end())
            {
                return m_textureMap.at(static_cast<std::string>(asset));
            }
            else
            {
                HGWARN("Unable to find requested texture! returning empty texture! ");
                return GetAsset(AssetType::TEXTURE, "empty");
            }
            break;

        case AssetType::AUDIO:
            if(m_audioMap.find(static_cast<std::string>(asset)) != m_audioMap.end()) { return m_audioMap.at(static_cast<std::string>(asset)); }
            else
            {
                HGWARN("Unable to find requested audio! returning default audio!");
                return m_audioMap.at(static_cast<std::string>("default"));
            }
            break;
    }
}

} // namespace Humongous::Systems
