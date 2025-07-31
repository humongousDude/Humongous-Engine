#include "logger.hpp"
#include "mock_asset_manager.hpp"
#include "mock_logical_device.hpp"
#include "resource_manager.hpp"
#include "gtest/gtest.h"

namespace Humongous
{

// TEST(ResourceManagerSuite, EmptyModelString)
// {
//     testing::NiceMock<MockLogicalDevice> device;
//     testing::NiceMock<MockAssetManager>  assetManager;
//     ResourceManager                      rm(device, assetManager);
//
//     EXPECT_NE(rm.RequestModel(""), nullptr);
// }
//
// TEST(ResourceManagerSuite, RequestDefaultModel)
// {
//     testing::NiceMock<MockLogicalDevice> device;
//     testing::NiceMock<MockAssetManager>  assetManager;
//     ResourceManager                      rm(device, assetManager);
//
//     EXPECT_NE(rm.RequestModel("DamagedHelmet.glb"), nullptr);
// }
//
// TEST(ResourceManagerSuite, RequestNodeMatrixIndexForInvalidModel)
// {
//     testing::NiceMock<MockLogicalDevice> device;
//     testing::NiceMock<MockAssetManager>  assetManager;
//     ResourceManager                      rm(device, assetManager);
//
//     EXPECT_EQ(rm.RequestModelNodeMatriciesIndex(-1), -1);
// }
//
// TEST(ResourceManagerSuite, RequestNodeMatrixIndexForValidModel)
// {
//     testing::NiceMock<MockLogicalDevice> device;
//     testing::NiceMock<MockAssetManager>  assetManager;
//     ResourceManager                      rm(device, assetManager);
//
//     EXPECT_NE(rm.RequestModel("DamagedHelmet.glb"), nullptr);
//     EXPECT_EQ(rm.RequestModelNodeMatriciesIndex(0), 0);
// }
//
// TEST(ResourceManagerSuite, RequestModelInstanceForValidModel)
// {
//     testing::NiceMock<MockLogicalDevice> device;
//     testing::NiceMock<MockAssetManager>  assetManager;
//     ResourceManager                      rm(device, assetManager);
//
//     EXPECT_NE(rm.RequestModel("DamagedHelmet.glb"), nullptr);
//     EXPECT_NE(rm.GetModelInstance(0), nullptr);
// }
//
// TEST(ResourceManagerSuite, RequestSkyboxForInvalidSkybox)
// {
// ResumeLogging();
//
// testing::NiceMock<MockLogicalDevice> device;
// testing::NiceMock<MockAssetManager>  assetManager;
// ResourceManager                      rm(device, assetManager);
// vk::CommandBuffer                    dummyCommandBuffer = vk::CommandBuffer(reinterpret_cast<VkCommandBuffer>(0xAABBCCDD)); // A non-null
// dummy
//
// EXPECT_CALL(device, BeginSingleTimeCommands()).WillRepeatedly(testing::Return(dummyCommandBuffer));
// // EXPECT_CALL(device, EndSingleTimeCommands(testing::_)).;
//
// EXPECT_EQ(rm.LoadSkybox("invalid skybox"), nullptr);
//
// PauseLogging();
// }
//
// TEST(ResourceManagerSuite, RequestSkyboxForValidSkybox)
// {
//     testing::NiceMock<MockLogicalDevice> device;
//     testing::NiceMock<MockAssetManager>  assetManager;
//     ResourceManager                      rm(device, assetManager);
//
//     EXPECT_NE(rm.LoadSkybox("papermill"), nullptr);
// }

} // namespace Humongous
