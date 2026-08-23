find_package(GTest REQUIRED)
add_executable(kv_tests kv_test.cpp kv.cpp)
target_link_libraries(kv_tests GTest::gtest GTest::gtest_main)