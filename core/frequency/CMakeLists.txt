cmake_minimum_required(VERSION 3.20)

# Frequency Control Library
add_library(frequency_control STATIC
    frequency_manager.cpp
)

target_include_directories(frequency_control
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core/frequency
)

target_compile_features(frequency_control PUBLIC cxx_std_20)

# 链接 pthread (用于线程支持)
target_link_libraries(frequency_control
    PUBLIC
        pthread
)
