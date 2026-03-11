cmake_minimum_required(VERSION 3.20)
project(FlowAdSystem VERSION 1.0.0 LANGUAGES CXX)

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

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# --- 关键配置写在 MakeAvailable 之前 ---
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE) # 阻止安装到系统

FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_TAG        master  # 或者指定具体版本号如 v1.1.0
)

# 禁用 RapidJSON 自带的测试/示例/文档，避免与项目 GTest 冲突
set(RAPIDJSON_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_DOC      OFF CACHE BOOL "" FORCE)

# RapidJSON 是纯头文件库，MakeAvailable 只需暴露 include 路径
FetchContent_MakeAvailable(rapidjson)

# 包含子目录
add_subdirectory(core/openrtb)
add_subdirectory(core/indexing)
add_subdirectory(tests)
add_subdirectory(data)
