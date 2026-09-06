include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(kv_tests kv_test.cpp)
target_link_libraries(kv_tests PRIVATE kv_lib gtest gtest_main)

add_executable(kv_tests_128 kv_test_128.cpp)
target_link_libraries(kv_tests_128 PRIVATE kv_lib gtest gtest_main)

include(GoogleTest)
gtest_discover_tests(kv_tests)