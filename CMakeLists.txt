cmake_minimum_required(VERSION 3.14)
project(KVStore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# core library — used by both server and tests
add_library(kv_lib kv.cpp)
target_include_directories(kv_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# main server binary
add_executable(kv_server main.cpp)
target_link_libraries(kv_server PRIVATE kv_lib)

# tests, opt-in
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()