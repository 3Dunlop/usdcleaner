#include <gtest/gtest.h>
#include "core/materials/MaterialHasher.h"
#include "core/common/HashUtils.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>

using namespace usdcleaner;
PXR_NAMESPACE_USING_DIRECTIVE

class MaterialHasherTest : public ::testing::Test {
protected:
    std::string TestDataPath(const std::string& filename) {
        return std::string(TEST_DATA_DIR) + "/" + filename;
    }
};

TEST_F(MaterialHasherTest, IdenticalMaterialsProduceSameHash) {
    auto stage = UsdStage::Open(TestDataPath("duplicate_materials.usda"));
    ASSERT_TRUE(stage);

    MaterialHasher hasher;

    UsdShadeMaterial steel1(stage->GetPrimAtPath(
        SdfPath("/World/Materials/Steel_001")));
    UsdShadeMaterial steel2(stage->GetPrimAtPath(
        SdfPath("/World/Materials/Steel_002")));
    UsdShadeMaterial steel3(stage->GetPrimAtPath(
        SdfPath("/World/Materials/Steel_003")));

    ASSERT_TRUE(steel1);
    ASSERT_TRUE(steel2);
    ASSERT_TRUE(steel3);

    HashDigest hash1 = hasher.HashMaterial(steel1);
    HashDigest hash2 = hasher.HashMaterial(steel2);
    HashDigest hash3 = hasher.HashMaterial(steel3);

    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash2, hash3);
}

TEST_F(MaterialHasherTest, DifferentMaterialsProduceDifferentHashes) {
    auto stage = UsdStage::Open(TestDataPath("duplicate_materials.usda"));
    ASSERT_TRUE(stage);

    MaterialHasher hasher;

    UsdShadeMaterial steel(stage->GetPrimAtPath(
        SdfPath("/World/Materials/Steel_001")));
    UsdShadeMaterial concrete(stage->GetPrimAtPath(
        SdfPath("/World/Materials/Concrete_001")));

    ASSERT_TRUE(steel);
    ASSERT_TRUE(concrete);

    HashDigest hashSteel = hasher.HashMaterial(steel);
    HashDigest hashConcrete = hasher.HashMaterial(concrete);

    EXPECT_NE(hashSteel, hashConcrete);
}
