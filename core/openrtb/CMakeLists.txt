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

# OpenRTB Response Builder Library
add_library(openrtb_response STATIC
    openrtb_response_builder.cpp
    auction_decision.cpp
    response_serializer.cpp
    openrtb_response_models.h
    openrtb_response_builder.h
    auction_decision.h
    response_serializer.h
)

target_include_directories(openrtb_response
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core/openrtb
        ${CMAKE_SOURCE_DIR}/core/ranker
        ${CMAKE_SOURCE_DIR}/data/models
)

target_link_libraries(openrtb_response
    PUBLIC
        openrtb_parser
)

# 依赖 nlohmann/json (从 FetchContent 获取)
target_link_libraries(openrtb_response PUBLIC nlohmann_json::nlohmann_json)

# 设置 C++ 标准
target_compile_features(openrtb_response PUBLIC cxx_std_20)
