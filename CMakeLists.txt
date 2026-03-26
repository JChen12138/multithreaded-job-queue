cmake_minimum_required(VERSION 3.15)

project(MultithreadedJobQueue VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SERVER "Build the main server executable" ON)
option(BUILD_BENCH "Build the benchmark executable" ON)
option(BUILD_TESTING "Build unit tests" ON)

if(MINGW)
    add_link_options(-Wl,-subsystem,console)
endif()

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_library(project_warnings INTERFACE)
target_compile_features(project_warnings INTERFACE cxx_std_17)

add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(BUILD_SERVER)
    find_package(cxxopts CONFIG REQUIRED)
    find_package(prometheus-cpp CONFIG REQUIRED)

    add_executable(server
        main.cpp
        src/JobQueue.cpp
        src/ThreadPool.cpp
        src/Metrics.cpp
        src/MetricsServer.cpp
    )

    target_link_libraries(server PRIVATE
        project_includes
        project_warnings
        spdlog::spdlog
        fmt::fmt
        cxxopts::cxxopts
        prometheus-cpp::core
        prometheus-cpp::pull
        ws2_32
        mswsock
    )
endif()

if(BUILD_BENCH)
    add_executable(bench
        bench.cpp
        src/JobQueue.cpp
        src/ThreadPool.cpp
    )

    target_compile_definitions(bench PRIVATE DISABLE_METRICS)
    target_link_libraries(bench PRIVATE
        project_includes
        project_warnings
        spdlog::spdlog
        fmt::fmt
    )
endif()

if(BUILD_TESTING)
    include(CTest)
    enable_testing()

    find_package(Catch2 CONFIG REQUIRED)

    add_executable(test_job_queue
        src/JobQueue.cpp
        test/test_job_queue.cpp
    )

    target_link_libraries(test_job_queue PRIVATE
        project_includes
        project_warnings
        Catch2::Catch2
        spdlog::spdlog
        fmt::fmt
    )

    add_executable(test_edge_cases
        src/JobQueue.cpp
        src/ThreadPool.cpp
        test/test_edge_cases.cpp
    )

    target_compile_definitions(test_edge_cases PRIVATE DISABLE_METRICS)
    target_link_libraries(test_edge_cases PRIVATE
        project_includes
        project_warnings
        Catch2::Catch2
        spdlog::spdlog
        fmt::fmt
    )

    add_executable(test_LRUCache
        test/test_LRUCache.cpp
    )

    target_link_libraries(test_LRUCache PRIVATE
        project_includes
        project_warnings
        Catch2::Catch2
    )

    add_test(NAME test_job_queue COMMAND test_job_queue)
    add_test(NAME test_edge_cases COMMAND test_edge_cases)
    add_test(NAME test_LRUCache COMMAND test_LRUCache)
endif()
