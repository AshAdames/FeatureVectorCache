cmake_minimum_required(VERSION 3.14)

project(KVStore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Asio

include(FetchContent)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
)

FetchContent_MakeAvailable(asio)

# Core KV library

add_library(kv_lib
    kv_opt.cpp
)

target_include_directories(kv_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Network library

add_library(network
    network/session.cpp
)

target_include_directories(network
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${asio_SOURCE_DIR}/asio/include
)

target_compile_definitions(network
    PUBLIC
        ASIO_STANDALONE
)


# Server
add_executable(kv_server
    main.cpp
)

target_link_libraries(kv_server
    PRIVATE
        kv_lib
        network
)

# Threads for Asio

find_package(Threads REQUIRED)

target_link_libraries(network
    PRIVATE
        Threads::Threads
)



# Benchmark client (standalone load generator, not a unit test)
option(BUILD_BENCH "Build benchmark client" ON)

if(BUILD_BENCH)
    add_executable(bench_client benchmarks/bench_feat_client.cpp)
    target_include_directories(bench_client
        PRIVATE
            ${asio_SOURCE_DIR}/asio/include
    )
    target_compile_definitions(bench_client PRIVATE ASIO_STANDALONE)
    target_link_libraries(bench_client PRIVATE Threads::Threads)
endif()

# Tests

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()