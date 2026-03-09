#include <gtest/gtest.h>
#include "core/common/SpatialHash.h"

using namespace usdcleaner;

TEST(SpatialHashTest, SinglePointReturnsIndex0) {
    SpatialHash hash(0.01f);
    size_t idx = hash.InsertOrFind(GfVec3f(1.0f, 2.0f, 3.0f), 1e-5f);
    EXPECT_EQ(idx, 0u);
}

TEST(SpatialHashTest, DuplicatePointReturnsSameIndex) {
    SpatialHash hash(0.01f);
    size_t idx1 = hash.InsertOrFind(GfVec3f(1.0f, 2.0f, 3.0f), 1e-5f);
    size_t idx2 = hash.InsertOrFind(GfVec3f(1.0f, 2.0f, 3.0f), 1e-5f);
    EXPECT_EQ(idx1, idx2);
}

TEST(SpatialHashTest, NearbyPointWithinEpsilonMerges) {
    SpatialHash hash(0.001f);
    float epsilon = 1e-4f;
    size_t idx1 = hash.InsertOrFind(GfVec3f(1.0f, 2.0f, 3.0f), epsilon);
    size_t idx2 = hash.InsertOrFind(GfVec3f(1.00005f, 2.0f, 3.0f), epsilon);
    EXPECT_EQ(idx1, idx2);
}

TEST(SpatialHashTest, DistantPointsGetDifferentIndices) {
    SpatialHash hash(0.01f);
    float epsilon = 1e-5f;
    size_t idx1 = hash.InsertOrFind(GfVec3f(0.0f, 0.0f, 0.0f), epsilon);
    size_t idx2 = hash.InsertOrFind(GfVec3f(1.0f, 0.0f, 0.0f), epsilon);
    EXPECT_NE(idx1, idx2);
}

TEST(SpatialHashTest, GenerateRemapTableForCubeVertices) {
    // 8 unique cube corners, each duplicated 3 times (24 total, like BIM export)
    VtVec3fArray points;
    GfVec3f corners[8] = {
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}
    };

    // Each corner appears 3 times
    for (int i = 0; i < 8; ++i) {
        points.push_back(corners[i]);
        points.push_back(corners[i]);
        points.push_back(corners[i]);
    }
    ASSERT_EQ(points.size(), 24u);

    SpatialHash hash(0.001f);
    RemapTable remap = hash.GenerateRemapTable(points, 1e-5f);

    // Count unique indices
    std::set<uint32_t> uniqueIndices(remap.begin(), remap.end());
    EXPECT_EQ(uniqueIndices.size(), 8u);
}

TEST(SpatialHashTest, CompactPointsReducesArray) {
    VtVec3fArray points = {
        GfVec3f(0, 0, 0),
        GfVec3f(1, 0, 0),
        GfVec3f(0, 0, 0),  // duplicate of 0
        GfVec3f(1, 0, 0)   // duplicate of 1
    };

    RemapTable remap = {0, 1, 0, 1};
    VtVec3fArray compacted = SpatialHash::CompactPoints(points, remap);

    EXPECT_EQ(compacted.size(), 2u);
    EXPECT_EQ(compacted[0], GfVec3f(0, 0, 0));
    EXPECT_EQ(compacted[1], GfVec3f(1, 0, 0));
}
