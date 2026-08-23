#include <gtest/gtest.h>
#include <thread>
#include "kv.h"

class KVTest : public ::testing::Test {
protected:
    // runs before EACH test
    void SetUp() override {
        kv = new KV(3, std::chrono::seconds(3)); // cap=3, ttl=1s
    }

    // runs after EACH test
    void TearDown() override {
        delete kv;
    }

    KV* kv;
};

TEST_F(KVTest, SetValues){
    ASSERT_EQ(kv->SET("key1", "val1"), true);
    ASSERT_EQ(kv->SET("key2", "val2"), true);
    ASSERT_EQ(kv->SET("key3", "val3"), true);
}

TEST_F(KVTest, DelValues){
    ASSERT_EQ(kv->SET("key1", "val1"), true);
    ASSERT_EQ(kv->DEL("key1"), true);
    ASSERT_EQ(kv->DEL("key1"), false);
    ASSERT_EQ(kv->DEL("key2"), false);
}

TEST_F(KVTest, GetValues){ 
    ASSERT_EQ(kv->SET("key1", "val1"), true);
    ASSERT_EQ(kv->GET("key1"),"val1");
    ASSERT_EQ(kv->GET("never existed"),"");
}

TEST_F(KVTest, CapacityEviction) {
    // capacity is 3
    kv->SET("key1", "val1");
    kv->SET("key2", "val2");
    kv->SET("key3", "val3");
    kv->SET("key4", "val4"); // triggers eviction of LRU (key1)

    ASSERT_EQ(kv->GET("key1"), "");     // evicted
    ASSERT_EQ(kv->GET("key4"), "val4"); // present
}

TEST_F(KVTest, ExpiredValues){
    ASSERT_EQ(kv->SET("key1", "val1"), true);
    std::this_thread::sleep_for(std::chrono::seconds(4)); // TTL was 3s
    ASSERT_EQ(kv->GET("key1"), ""); // should be expired -> not found
}