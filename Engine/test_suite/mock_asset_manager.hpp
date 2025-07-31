#pragma once

#include "asset_manager.hpp"
#include <gmock/gmock.h>

namespace Humongous
{

class MockAssetManager : public IAssetManager
{
public:
    MockAssetManager()
    {
        ON_CALL(*this, GetAsset(AssetType::MODEL, "DamagedHelmet.glb"))
            .WillByDefault(testing::Return(HGASSETDIRPATH + std::string("models/DamagedHelmet.glb")));
        ON_CALL(*this, GetAsset(AssetType::MODEL, "default.glb"))
            .WillByDefault(testing::Return(HGASSETDIRPATH + std::string("models/default.glb")));
        ON_CALL(*this, GetAsset(AssetType::MODEL, "box.glb")).WillByDefault(testing::Return(HGASSETDIRPATH + std::string("models/box.glb")));
        ON_CALL(*this, GetAsset(AssetType::MODEL, "bad_cube.glb"))
            .WillByDefault(testing::Return(HGASSETDIRPATH + std::string("models/bad_cube.glb")));
        ON_CALL(*this, GetAsset(AssetType::MODEL, "buster_drone.glb"))
            .WillByDefault(testing::Return(HGASSETDIRPATH + std::string("models/buster_drone.glb")));
        ON_CALL(*this, GetAsset(AssetType::MODEL, "duplicate_vertex_cone.glb"))
            .WillByDefault(testing::Return(HGASSETDIRPATH + std::string("models/duplicate_vertex_cone.glb")));
        ON_CALL(*this, GetAsset(AssetType::TEXTURE, "papermill"))
            .WillByDefault(testing::Return(HGASSETDIRPATH + std::string("textures/papermill.ktx")));
    }

    MOCK_METHOD(std::string, GetAsset, (const AssetType type, const std::string_view asset), (const, override));
};
} // namespace Humongous
