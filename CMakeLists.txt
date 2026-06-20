cmake_minimum_required(VERSION 3.20)
project(url_shortener VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

include(FetchContent)

find_package(Crow QUIET)
if(NOT Crow_FOUND)
    message(STATUS "Crow not found locally, fetching via FetchContent...")
    FetchContent_Declare(
        Crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG        v1.2.0
        GIT_SHALLOW    TRUE
    )
    set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Crow)
else()
    message(STATUS "Found system Crow")
endif()

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
pkg_check_modules(PQ REQUIRED IMPORTED_TARGET libpq)

find_library(HIREDIS_LIB hiredis REQUIRED)
find_path(HIREDIS_INCLUDE hiredis/hiredis.h REQUIRED)

find_library(REDIS_PLUS_PLUS_LIB redis++ REQUIRED)
find_path(REDIS_PLUS_PLUS_INCLUDE sw/redis++/redis++.h REQUIRED)

find_package(prometheus-cpp CONFIG REQUIRED)
find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/database/PostgresPool.cpp
    src/redis/RedisClient.cpp
    src/metrics/Metrics.cpp
)

add_executable(url_shortener ${SOURCES})

target_include_directories(url_shortener PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${HIREDIS_INCLUDE}
    ${REDIS_PLUS_PLUS_INCLUDE}
)

target_link_libraries(url_shortener PRIVATE
    Crow::Crow
    PkgConfig::PQXX
    PkgConfig::PQ
    ${HIREDIS_LIB}
    ${REDIS_PLUS_PLUS_LIB}
    prometheus-cpp::pull
    prometheus-cpp::core
    Threads::Threads
)

enable_testing()

set(TEST_SOURCES
    tests/test_base62.cpp
    tests/test_lru_cache.cpp
    tests/test_rate_limiter.cpp
    tests/test_consistent_hash.cpp
    tests/test_thread_pool.cpp
)

add_executable(url_shortener_tests ${TEST_SOURCES})

target_include_directories(url_shortener_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(url_shortener_tests PRIVATE
    GTest::gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(url_shortener_tests)

install(TARGETS url_shortener DESTINATION bin)
