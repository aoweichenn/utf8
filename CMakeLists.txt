cmake_minimum_required(VERSION 3.20)

project(utf8 VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so/.dylib)" OFF)
option(UTF8_BUILD_TESTS "Build unit tests" ON)
option(UTF8_OFFLINE_BUILD "Fail if unicode cache is missing (Air-gapped mode)" OFF)
option(UTF8_BUILD_BENCHMARKS "Build microbenchmarks" OFF)
option(UTF8_BUILD_COVERAGE "Build with code coverage tracking (lcov)" OFF) # 🌟 新增覆盖率开关

# ==============================================================================
# 🌟 目录规划一：编译产物隔离 (按 Debug/Release 区分输出目录)
# ==============================================================================
# 获取构建类型，如果没有明确指定，默认使用 Debug
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif ()

# 将构建类型转为小写 (e.g., "Debug" -> "debug", "Release" -> "release")
string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)

# 动态组合输出根目录： output/debug/ 或 output/release/
set(UTF8_OUT_BASE "${CMAKE_CURRENT_SOURCE_DIR}/output/${BUILD_TYPE_LOWER}")

if (BUILD_SHARED_LIBS)
    set(LIB_TYPE_DIR "shared")
else ()
    set(LIB_TYPE_DIR "static")
endif ()

# 设置全局的二进制和库输出路径
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${UTF8_OUT_BASE}/lib/${LIB_TYPE_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${UTF8_OUT_BASE}/lib/${LIB_TYPE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${UTF8_OUT_BASE}/bin")

message(STATUS "🎯 [UTF8] Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "🎯 [UTF8] Output Dir: ${UTF8_OUT_BASE}")

# ==============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# ==============================================================================
# 🌟 目录规划二：生成代码与缓存 (提升到与 include 同级)
# ==============================================================================
set(UCD_VERSION "15.1.0")

set(TABLES_DATA_CPP "${CMAKE_CURRENT_SOURCE_DIR}/generated/property_tables.cpp")
set(CACHE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/unicode_cache/${UCD_VERSION}")
set(GEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_property.py")
set(MANIFEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/manifest.json")

# 🌟 修复 1：解决“鸡和蛋”问题。如果文件不存在，CMake 配置阶段直接写一个空 JSON
if (NOT EXISTS "${MANIFEST_FILE}")
    file(WRITE "${MANIFEST_FILE}" "{\n}\n")
    message(STATUS "🎯 [UTF8] Created empty manifest.json for bootstrapping")
endif ()

# 🌟 修复 2：加上 --update-lock 参数，允许自动补全缺失的哈希值
set(GEN_COMMAND $<TARGET_FILE:Python3::Interpreter> ${GEN_SCRIPT}
        --out ${TABLES_DATA_CPP}
        --ucd-version ${UCD_VERSION}
        --cache-dir ${CACHE_DIR}
        --update-lock)

if (UTF8_OFFLINE_BUILD)
    list(APPEND GEN_COMMAND "--offline")
endif ()

add_custom_command(
        OUTPUT ${TABLES_DATA_CPP}
        COMMAND ${GEN_COMMAND}
        COMMAND_EXPAND_LISTS
        DEPENDS ${GEN_SCRIPT} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/manifest.json"
        COMMENT "Generating industrial-grade UTF-8 property tables with strict SHA-256 locking..."
        VERBATIM
)
set_source_files_properties(${TABLES_DATA_CPP} PROPERTIES GENERATED TRUE)

# ==============================================================================
# 🌟 核心编译配置
# ==============================================================================
add_library(utf8 source/decode.cpp source/encode.cpp source/utils.cpp source/internal.hpp ${TABLES_DATA_CPP})
add_library(utf8::utf8 ALIAS utf8)

target_include_directories(utf8
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

if (BUILD_SHARED_LIBS)
    target_compile_definitions(utf8 PRIVATE UTF8_BUILD_SHARED)
    target_compile_definitions(utf8 INTERFACE UTF8_USE_SHARED)
    set_target_properties(utf8 PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
endif ()

if (MSVC)
    target_compile_options(utf8 PRIVATE /W4 /WX /Zc:__cplusplus)
else ()
    target_compile_options(utf8 PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif ()

if (UTF8_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.14.0)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(utf8_test tests/test_core.cpp)
    target_link_libraries(utf8_test PRIVATE utf8::utf8 gtest_main)
    add_test(NAME CoreTest COMMAND utf8_test)

    # ==========================================================================
    # 🌟 覆盖率插桩 (LCOV)
    # 强制开启 -O0 防止分支被优化掉，确保测出来的分支覆盖率绝对真实
    # ==========================================================================
    if (UTF8_BUILD_COVERAGE)
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(utf8 PRIVATE --coverage -O0 -g)
            target_link_options(utf8 PRIVATE --coverage)

            target_compile_options(utf8_test PRIVATE --coverage -O0 -g)
            target_link_options(utf8_test PRIVATE --coverage)
        else ()
            message(WARNING "Coverage tracking is only supported on GCC or Clang.")
        endif ()
    endif ()
endif ()

# ==============================================================================
# 🌟 工业级 Fuzzing 靶场
# ==============================================================================
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(utf8_fuzzer tests/fuzz_test.cpp)
    target_link_libraries(utf8_fuzzer PRIVATE utf8::utf8)

    target_compile_options(utf8_fuzzer PRIVATE -fsanitize=fuzzer,address -g -O2)
    target_link_options(utf8_fuzzer PRIVATE -fsanitize=fuzzer,address)

    # 动态输出到对应的模式下，如: output/debug/fuzz/
    set_target_properties(utf8_fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${UTF8_OUT_BASE}/fuzz")
endif ()

# ==============================================================================
# 🌟 性能基准测试 (Google Benchmark)
# ==============================================================================
if (UTF8_BUILD_BENCHMARKS)
    include(FetchContent)
    FetchContent_Declare(googlebench GIT_REPOSITORY https://github.com/google/benchmark.git GIT_TAG v1.8.3)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebench)

    add_executable(utf8_bench benchmarks/bench_core.cpp)
    target_link_libraries(utf8_bench PRIVATE utf8::utf8 benchmark::benchmark)

    if (MSVC)
        target_compile_options(utf8_bench PRIVATE /O2 /Ob2)
    else ()
        target_compile_options(utf8_bench PRIVATE -O3 -march=native -flto)
        target_link_options(utf8_bench PRIVATE -flto)
    endif ()
endif ()