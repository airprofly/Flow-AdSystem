cmake_minimum_required(VERSION 3.20)
project(FlowAdSystem VERSION 1.0.0 LANGUAGES CXX)

# 设置 CMake 策略，避免因新版本 CMake 引入的行为变化导致构建失败
cmake_policy(SET CMP0135 NEW)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

# 启用 ctest 支持
enable_testing()

include(FetchContent)

# CI/离线环境下避免每次构建都尝试更新远端依赖
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# --- 关键配置写在 MakeAvailable 之前 ---
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE) # 阻止安装到系统

FetchContent_MakeAvailable(googletest)

set(ONNXRUNTIME_VERSION "1.19.2" CACHE STRING "ONNX Runtime version")
set(ONNXRUNTIME_PACKAGE_URL
  "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz"
  CACHE STRING "ONNX Runtime package URL"
)

FetchContent_Declare(
  onnxruntime
  URL ${ONNXRUNTIME_PACKAGE_URL}
)

# 2. 使其可用
FetchContent_MakeAvailable(onnxruntime)

FetchContent_Declare(
  rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_TAG        master  # 或者指定具体版本号如 v1.1.0
)

# 禁用 RapidJSON 自带的测试/示例/文档，避免与项目 GTest 冲突
set(RAPIDJSON_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_DOC      OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(rapidjson)
# RapidJSON 是纯头文件库，MakeAvailable 只需暴露 include 路径

# nlohmann/json - 用于 JSON 序列化
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)

FetchContent_MakeAvailable(nlohmann_json)

# TBB (Threading Building Block) - 用于 C++17 并行算法
option(FLOW_ADSYSTEM_ENABLE_TBB "Enable TBB for C++17 parallel algorithms" ON)

if(FLOW_ADSYSTEM_ENABLE_TBB)
    FetchContent_Declare(
        tbb
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG        v2021.11.0
    )

    # TBB 配置
    set(TBB_TEST OFF CACHE BOOL "" FORCE)
    set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(tbb)
endif()

# 包含子目录
add_subdirectory(core/openrtb)
add_subdirectory(core/indexing)
add_subdirectory(core/frequency)
add_subdirectory(core/pacing)
add_subdirectory(core/ctr)
add_subdirectory(core/ranker)
add_subdirectory(tests)
add_subdirectory(data)
