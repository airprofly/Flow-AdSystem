cmake_minimum_required(VERSION 3.20)

# Ad Index Library
add_library(ad_index STATIC
    inverted_index.cpp
    ad_store.cpp
    ad_index.cpp
    index_loader.cpp
)

target_include_directories(ad_index
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core/indexing
        ${CMAKE_SOURCE_DIR}/core/openrtb
        ${CMAKE_SOURCE_DIR}/data/models
        ${rapidjson_SOURCE_DIR}/include
)

target_link_libraries(ad_index
    PUBLIC
        openrtb_parser
)

target_compile_features(ad_index PUBLIC cxx_std_20)
