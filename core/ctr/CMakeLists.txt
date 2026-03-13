# CTR 预估模块
cmake_minimum_required(VERSION 3.20)

# 收集源文件
set(CTR_CORE_HEADERS
    core/feature_spec.h
    core/feature_context.h
    core/model_config.h
)

set(CTR_EXTRACTOR_HEADERS
    extractor/feature_extractor.h
    extractor/user_feature_extractor.h
    extractor/ad_feature_extractor.h
    extractor/context_feature_extractor.h
)

set(CTR_EXTRACTOR_SOURCES
    extractor/user_feature_extractor.cpp
    extractor/ad_feature_extractor.cpp
    extractor/context_feature_extractor.cpp
)

set(CTR_ENGINE_HEADERS
    engine/model_inference_engine.h
    engine/onnx_runtime_engine.h
)

set(CTR_ENGINE_SOURCES
    engine/onnx_runtime_engine.cpp
)

set(CTR_HEADERS
    ctr_manager.h
)

set(CTR_SOURCES
    ctr_manager.cpp
)

# 创建 CTR 库
add_library(ctr_lib STATIC
    ${CTR_EXTRACTOR_SOURCES}
    ${CTR_ENGINE_SOURCES}
    ${CTR_SOURCES}
)

target_include_directories(ctr_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${onnxruntime_SOURCE_DIR}/include
        ${rapidjson_SOURCE_DIR}/include
)

# 链接 ONNX Runtime
target_link_libraries(ctr_lib
    PUBLIC
        ${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so
)

# 设置 C++ 标准
target_compile_features(ctr_lib PUBLIC cxx_std_20)

# 安装规则
install(TARGETS ctr_lib
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    ${CTR_CORE_HEADERS}
    ${CTR_EXTRACTOR_HEADERS}
    ${CTR_ENGINE_HEADERS}
    ${CTR_HEADERS}
    DESTINATION include/ctr
)
