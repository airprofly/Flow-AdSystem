# Author: airprofly
cmake_minimum_required(VERSION 3.20)

# Pacing 模块库
add_library(pacing STATIC
    pacing_manager.cpp
)

target_include_directories(pacing
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core/pacing
)

target_compile_features(pacing PUBLIC cxx_std_20)

# 链接 pthread (线程支持)
find_package(Threads REQUIRED)
target_link_libraries(pacing PUBLIC Threads::Threads)
