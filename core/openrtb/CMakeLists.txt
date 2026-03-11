cmake_minimum_required(VERSION 3.20)

# OpenRTB Parser Library
add_library(openrtb_parser STATIC
    openrtb_parser.cpp
    openrtb_models.h
    openrtb_parser.h
)

target_include_directories(openrtb_parser
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core/openrtb
        ${CMAKE_SOURCE_DIR}/data/models
        # RapidJSON is header-only; expose its include path to consumers
        ${rapidjson_SOURCE_DIR}/include
)

# 设置 C++ 标准
target_compile_features(openrtb_parser PUBLIC cxx_std_20)
