#include "mock_asset_manager.hpp"
#include "mock_logical_device.hpp"
#include "resource_manager.hpp"
#include "gtest/gtest.h"

namespace Humongous
{

// Resource manager functions should fail gracefully in the event of errors. I'm unsure of the correct behaviour for functions that return shared
// pointers Should they return nullptrs or pointers to invalid objects?

TEST(ResourceManagerSuite, EmptyModelString)
{
    testing::NiceMock<MockLogicalDevice> device;
    testing::NiceMock<MockAssetManager>  assetManager;
    ResourceManager                      rm(device, assetManager);

    EXPECT_NE(rm.RequestModel(""), nullptr);
}

TEST(ResourceManagerSuite, RequestDefaultModel)
{
    testing::NiceMock<MockLogicalDevice> device;
    testing::NiceMock<MockAssetManager>  assetManager;
    ResourceManager                      rm(device, assetManager);

    EXPECT_NE(rm.RequestModel("DamagedHelmet.glb"), nullptr);
}

TEST(ResourceManagerSuite, RequestNodeMatrixIndexForInvalidModel)
{
    testing::NiceMock<MockLogicalDevice> device;
    testing::NiceMock<MockAssetManager>  assetManager;
    ResourceManager                      rm(device, assetManager);

    EXPECT_EQ(rm.RequestModelNodeMatriciesIndex(-1), -1);
}

TEST(ResourceManagerSuite, RequestNodeMatrixIndexForValidModel)
{
    testing::NiceMock<MockLogicalDevice> device;
    testing::NiceMock<MockAssetManager>  assetManager;
    ResourceManager                      rm(device, assetManager);

    EXPECT_NE(rm.RequestModel("DamagedHelmet.glb"), nullptr);
    EXPECT_EQ(rm.RequestModelNodeMatriciesIndex(0), 0);
}

TEST(ResourceManagerSuite, RequestModelInstanceForValidModel)
{
    testing::NiceMock<MockLogicalDevice> device;
    testing::NiceMock<MockAssetManager>  assetManager;
    ResourceManager                      rm(device, assetManager);

    EXPECT_NE(rm.RequestModel("DamagedHelmet.glb"), nullptr);
    EXPECT_EQ(rm.GetModelInstance(0), nullptr);
}

TEST(ResourceManagerSuite, RequestSkyboxForInvalidSkybox)
{
    testing::NiceMock<MockLogicalDevice> device;
    testing::NiceMock<MockAssetManager>  assetManager;
    ResourceManager                      rm(device, assetManager);

    EXPECT_NE(rm.LoadSkybox("invalid skybox"), nullptr);
}

TEST(ResourceManagerSuite, RequestSkyboxForValidSkybox)
{
    testing::NiceMock<MockLogicalDevice> device;
    testing::NiceMock<MockAssetManager>  assetManager;
    ResourceManager                      rm(device, assetManager);

    EXPECT_NE(rm.LoadSkybox("papermill"), nullptr);
}

} // namespace Humongous
