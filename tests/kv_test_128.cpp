#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <vector>

#include "kv_opt.h"

class KVTest : public ::testing::Test {
protected:

    static constexpr int DIM = 128;

    void SetUp() override {
        kv = new KV_OPT(3, 10, DIM);

        vec1 = make_vector(1.0f);
        vec2 = make_vector(2.0f);
        vec3 = make_vector(3.0f);
        vec4 = make_vector(4.0f);

        // Intentionally dimension 129
        vec5 = std::vector<float>(DIM + 1, 0.0f);
    }

    void TearDown() override {
        delete kv;
    }

    // Creates a vector with every element equal to value
    static std::vector<float> make_vector(float value) {
        return std::vector<float>(DIM, value);
    }

    KV_OPT* kv;

    std::vector<float> vec1;
    std::vector<float> vec2;
    std::vector<float> vec3;
    std::vector<float> vec4;
    std::vector<float> vec5;
};


// --------------------------------------------------
// SET
// --------------------------------------------------

TEST_F(KVTest, SetValues) {

    ASSERT_TRUE(kv->SET(1, vec1));
    ASSERT_TRUE(kv->SET(2, vec2));

    // Wrong dimension
    ASSERT_FALSE(kv->SET(3, vec5));
}


// --------------------------------------------------
// DEL
// --------------------------------------------------

TEST_F(KVTest, DelValues) {

    ASSERT_TRUE(kv->SET(1, vec1));

    ASSERT_TRUE(kv->DEL(1));

    ASSERT_FALSE(kv->DEL(2));
    ASSERT_FALSE(kv->DEL(1));
}


// --------------------------------------------------
// GET
// --------------------------------------------------

TEST_F(KVTest, GetValues) {

    ASSERT_TRUE(kv->SET(1, vec1));
    ASSERT_TRUE(kv->SET(2, vec2));

    ASSERT_FALSE(kv->SET(3, vec5));

    ASSERT_EQ(kv->GET(1), vec1);

    // Key doesn't exist
    ASSERT_TRUE(kv->GET(3).empty());
}


// --------------------------------------------------
// LRU / CAPACITY
// --------------------------------------------------

TEST_F(KVTest, CapacityEviction) {

    ASSERT_TRUE(kv->SET(1, vec1));
    ASSERT_TRUE(kv->SET(2, vec2));
    ASSERT_TRUE(kv->SET(3, vec3));

    // Access key 1 -> becomes MRU
    ASSERT_EQ(kv->GET(1), vec1);

    // Key 2 should now be LRU
    ASSERT_TRUE(kv->SET(4, vec4));

    // Key 2 should have been evicted
    ASSERT_TRUE(kv->GET(2).empty());

    // These should remain
    ASSERT_EQ(kv->GET(1), vec1);
    ASSERT_EQ(kv->GET(3), vec3);
    ASSERT_EQ(kv->GET(4), vec4);
}


// --------------------------------------------------
// TTL
// --------------------------------------------------

TEST_F(KVTest, ExpiredValues) {

    delete kv;

    // 1 second TTL
    kv = new KV_OPT(3, 1, DIM);

    ASSERT_TRUE(kv->SET(1, vec1));

    std::this_thread::sleep_for(
        std::chrono::seconds(2)
    );

    ASSERT_TRUE(kv->GET(1).empty());
}


// --------------------------------------------------
// COSINE SIMILARITY
// --------------------------------------------------

TEST_F(KVTest, Similarity) {

    std::vector<float> a(DIM, 0.0f);
    std::vector<float> b(DIM, 0.0f);

    a[0] = 1.0f;
    b[0] = 2.0f;

    ASSERT_TRUE(kv->SET(1, a));
    ASSERT_TRUE(kv->SET(2, b));

    auto result = kv->SIMILARITY(1, 2);

    ASSERT_TRUE(result.has_value());

    // Same direction -> 1
    ASSERT_NEAR(result.value(), 1.0f, 1e-5f);
}


TEST_F(KVTest, SimilarityOpposite) {

    std::vector<float> a(DIM, 0.0f);
    std::vector<float> b(DIM, 0.0f);

    a[0] = 1.0f;
    b[0] = -1.0f;

    ASSERT_TRUE(kv->SET(1, a));
    ASSERT_TRUE(kv->SET(2, b));

    auto result = kv->SIMILARITY(1, 2);

    ASSERT_TRUE(result.has_value());

    // Opposite direction -> -1
    ASSERT_NEAR(result.value(), -1.0f, 1e-5f);
}


TEST_F(KVTest, SimilarityOrthogonal) {

    std::vector<float> a(DIM, 0.0f);
    std::vector<float> b(DIM, 0.0f);

    a[0] = 1.0f;
    b[1] = 1.0f;

    ASSERT_TRUE(kv->SET(1, a));
    ASSERT_TRUE(kv->SET(2, b));

    auto result = kv->SIMILARITY(1, 2);

    ASSERT_TRUE(result.has_value());

    // Orthogonal -> 0
    ASSERT_NEAR(result.value(), 0.0f, 1e-5f);
}


TEST_F(KVTest, SimilarityMissingKey) {

    std::vector<float> a(DIM, 0.0f);
    a[0] = 1.0f;

    ASSERT_TRUE(kv->SET(1, a));

    auto result = kv->SIMILARITY(1, 999);

    ASSERT_FALSE(result.has_value());
}


TEST_F(KVTest, SimilarityBothKeysMissing) {

    auto result = kv->SIMILARITY(1, 2);

    ASSERT_FALSE(result.has_value());
}


TEST_F(KVTest, SimilarityZeroVector) {

    std::vector<float> zero(DIM, 0.0f);

    std::vector<float> a(DIM, 0.0f);
    a[0] = 1.0f;

    ASSERT_TRUE(kv->SET(1, zero));
    ASSERT_TRUE(kv->SET(2, a));

    auto result = kv->SIMILARITY(1, 2);

    // Cosine similarity undefined when one vector has magnitude 0
    ASSERT_FALSE(result.has_value());
}


// --------------------------------------------------
// TOP K
// --------------------------------------------------

TEST_F(KVTest, TopK) {

    // Give TOPK enough capacity
    delete kv;
    kv = new KV_OPT(10, 10, DIM);

    std::vector<float> query(DIM, 0.0f);
    std::vector<float> v2(DIM, 0.0f);
    std::vector<float> v3(DIM, 0.0f);
    std::vector<float> v4(DIM, 0.0f);

    // Query: [1, 0, 0, ...]
    query[0] = 1.0f;

    // Same direction -> 1
    v2[0] = 2.0f;

    // 45 degrees -> sqrt(2)/2
    v3[0] = 1.0f;
    v3[1] = 1.0f;

    // Orthogonal -> 0
    v4[1] = 1.0f;

    ASSERT_TRUE(kv->SET(1, query));
    ASSERT_TRUE(kv->SET(2, v2));
    ASSERT_TRUE(kv->SET(3, v3));
    ASSERT_TRUE(kv->SET(4, v4));

    auto result = kv->TOPK(1, 3);

    ASSERT_EQ(result.size(), 3);

    ASSERT_EQ(result[0].first, 2);
    ASSERT_NEAR(result[0].second, 1.0f, 1e-5f);

    ASSERT_EQ(result[1].first, 3);
    ASSERT_NEAR(
        result[1].second,
        std::sqrt(2.0f) / 2.0f,
        1e-5f
    );

    ASSERT_EQ(result[2].first, 4);
    ASSERT_NEAR(result[2].second, 0.0f, 1e-5f);
}


TEST_F(KVTest, TopKMoreThanAvailable) {

    std::vector<float> query(DIM, 0.0f);
    std::vector<float> v2(DIM, 0.0f);
    std::vector<float> v3(DIM, 0.0f);

    query[0] = 1.0f;

    v2[0] = 2.0f;

    v3[0] = 1.0f;
    v3[1] = 1.0f;

    ASSERT_TRUE(kv->SET(1, query));
    ASSERT_TRUE(kv->SET(2, v2));
    ASSERT_TRUE(kv->SET(3, v3));

    // Only two candidates
    auto result = kv->TOPK(1, 10);

    ASSERT_EQ(result.size(), 2);

    ASSERT_EQ(result[0].first, 2);
    ASSERT_EQ(result[1].first, 3);
}


TEST_F(KVTest, TopKMissingQuery) {

    std::vector<float> a(DIM, 0.0f);
    a[0] = 1.0f;

    ASSERT_TRUE(kv->SET(1, a));

    auto result = kv->TOPK(999, 2);

    ASSERT_TRUE(result.empty());
}


TEST_F(KVTest, TopKZeroQuery) {

    std::vector<float> zero(DIM, 0.0f);

    std::vector<float> a(DIM, 0.0f);
    a[0] = 1.0f;

    ASSERT_TRUE(kv->SET(1, zero));
    ASSERT_TRUE(kv->SET(2, a));

    auto result = kv->TOPK(1, 1);

    ASSERT_TRUE(result.empty());
}