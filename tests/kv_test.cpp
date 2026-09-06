#include <gtest/gtest.h>
#include <thread>
#include "kv_opt.h"

class KVTest : public ::testing::Test {
protected:
    // runs before EACH test
    void SetUp() override {
        kv = new KV_OPT(3, 10, 4); // cap=3, ttl=1s, vec_dim = 4

        vec1 = {1.0f, 2.0f, 3.0f, 4.0f};
        vec2 = {5.0f, 6.0f, 7.0f, 8.0f};
        vec3 = {9.0f, 10.0f, 11.0f, 12.0f};
        vec4 = {13.0f, 14.0f, 15.0f, 16.0f};

        // Intentionally dimension 5
        vec5 = {0.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    }

    // runs after EACH test
    void TearDown() override {
        delete kv;
    }

    KV_OPT* kv;
    std::vector<float> vec1;
    std::vector<float> vec2;
    std::vector<float> vec3;
    std::vector<float> vec4;
    std::vector<float> vec5;
};

TEST_F(KVTest, SetValues){

    ASSERT_EQ(kv->SET(1, vec1), true);
    ASSERT_EQ(kv->SET(2, vec2), true);
    ASSERT_EQ(kv->SET(3, vec5), false);
}

TEST_F(KVTest, DelValues){
    ASSERT_EQ(kv->SET(1, vec1), true);
    ASSERT_EQ(kv->DEL(1), true);
    ASSERT_EQ(kv->DEL(2), false);
    ASSERT_EQ(kv->DEL(1), false);
}

TEST_F(KVTest, GetValues){ 
    ASSERT_EQ(kv->SET(1, vec1), true);
    ASSERT_EQ(kv->SET(2, vec2), true);
    ASSERT_EQ(kv->SET(3, vec5), false);
    ASSERT_EQ(kv->GET(1),vec1);
    ASSERT_TRUE(kv->GET(3).empty());
}

TEST_F(KVTest, CapacityEviction) {
    // Capacity = 3

    ASSERT_TRUE(kv->SET(1, vec1));
    ASSERT_TRUE(kv->SET(2, vec2));
    ASSERT_TRUE(kv->SET(3, vec3));

    // Access key 1 so it becomes MRU
    ASSERT_EQ(kv->GET(1), vec1);

    // Key 2 is now LRU
    ASSERT_TRUE(kv->SET(4, vec4));

    ASSERT_TRUE(kv->GET(2).empty()); // evicted
    ASSERT_EQ(kv->GET(1), vec1);     // still present
    ASSERT_EQ(kv->GET(3), vec3);     // still present
    ASSERT_EQ(kv->GET(4), vec4);     // newly inserted
}

TEST_F(KVTest, ExpiredValues) {
    // Use a short TTL for this test instead of waiting 10 seconds.
    delete kv;
    kv = new KV_OPT(3, 1, 4);

    ASSERT_TRUE(kv->SET(1, vec1));

    std::this_thread::sleep_for(std::chrono::seconds(2));

    ASSERT_TRUE(kv->GET(1).empty());
}

//test cosine similariry 

TEST_F(KVTest, Similarity) {
    // Same direction -> cosine similarity = 1
    std::vector<float> a = {1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> b = {2.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, a));
    ASSERT_TRUE(kv->SET(2, b));

    auto result = kv->SIMILARITY(1, 2);

    ASSERT_TRUE(result.has_value());
    ASSERT_NEAR(result.value(), 1.0f, 1e-5f);
}


TEST_F(KVTest, SimilarityOpposite) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> b = {-1.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, a));
    ASSERT_TRUE(kv->SET(2, b));

    auto result = kv->SIMILARITY(1, 2);

    ASSERT_TRUE(result.has_value());
    ASSERT_NEAR(result.value(), -1.0f, 1e-5f);
}

TEST_F(KVTest, SimilarityOrthogonal) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, a));
    ASSERT_TRUE(kv->SET(2, b));

    auto result = kv->SIMILARITY(1, 2);

    ASSERT_TRUE(result.has_value());
    ASSERT_NEAR(result.value(), 0.0f, 1e-5f);
}

TEST_F(KVTest, SimilarityMissingKey) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, a));

    auto result = kv->SIMILARITY(1, 999);

    ASSERT_FALSE(result.has_value());
}

TEST_F(KVTest, SimilarityBothKeysMissing) {
    auto result = kv->SIMILARITY(1, 2);

    ASSERT_FALSE(result.has_value());
}

TEST_F(KVTest, SimilarityZeroVector) {
    std::vector<float> zero = {0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> a = {1.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, zero));
    ASSERT_TRUE(kv->SET(2, a));

    auto result = kv->SIMILARITY(1, 2);

    ASSERT_FALSE(result.has_value());
}
//test topk

TEST_F(KVTest, TopK) {
    // Replace the fixture's KV just for this test
    delete kv;
    kv = new KV_OPT(10, 10, 4);

    std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> v2 = {2.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> v3 = {1.0f, 1.0f, 0.0f, 0.0f};
    std::vector<float> v4 = {0.0f, 1.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, query));
    ASSERT_TRUE(kv->SET(2, v2));
    ASSERT_TRUE(kv->SET(3, v3));
    ASSERT_TRUE(kv->SET(4, v4));

    auto result = kv->TOPK(1, 3);

    ASSERT_EQ(result.size(), 3);

    ASSERT_EQ(result[0].first, 2);
    ASSERT_NEAR(result[0].second, 1.0f, 1e-5f);

    ASSERT_EQ(result[1].first, 3);
    ASSERT_NEAR(result[1].second, std::sqrt(2.0f) / 2.0f, 1e-5f);

    ASSERT_EQ(result[2].first, 4);
    ASSERT_NEAR(result[2].second, 0.0f, 1e-5f);
}

TEST_F(KVTest, TopKMoreThanAvailable) {
    std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> v2 = {2.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> v3 = {1.0f, 1.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, query));
    ASSERT_TRUE(kv->SET(2, v2));
    ASSERT_TRUE(kv->SET(3, v3));

    // Only 2 candidates exist, but ask for top 10
    auto result = kv->TOPK(1, 10);

    ASSERT_EQ(result.size(), 2);

    ASSERT_EQ(result[0].first, 2);
    ASSERT_EQ(result[1].first, 3);
}


TEST_F(KVTest, TopKMissingQuery) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, a));

    auto result = kv->TOPK(999, 2);

    ASSERT_TRUE(result.empty());
}

TEST_F(KVTest, TopKZeroQuery) {
    std::vector<float> zero = {0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> a = {1.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(kv->SET(1, zero));
    ASSERT_TRUE(kv->SET(2, a));

    auto result = kv->TOPK(1, 1);

    ASSERT_TRUE(result.empty());
}