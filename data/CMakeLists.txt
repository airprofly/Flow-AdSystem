# 数据模块 CMakeLists.txt

# 生成 Protocol Buffers (暂时禁用,等待 .proto 文件创建)
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/openrtb")
set(GENERATED_PROTOBUF_PATH "${CMAKE_CURRENT_BINARY_DIR}/generated")

# 检查 proto 文件是否存在
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/openrtb/openrtb.proto")
    # 查找 protobuf 编译器
    find_program(PROTOC protoc)
    if(PROTOC)
        # 生成 C++ 代码
        set(PROTO_FILES openrtb/openrtb.proto)

        foreach(PROTO_FILE ${PROTO_FILES})
            get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

            set(PROTO_SRC "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.pb.cc")
            set(PROTO_HDR "${GENERATED_PROTOBUF_PATH}/${PROTO_NAME}.pb.h")

            add_custom_command(
                OUTPUT ${PROTO_SRC} ${PROTO_HDR}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_PROTOBUF_PATH}
                COMMAND ${PROTOC} --cpp_out=${GENERATED_PROTOBUF_PATH}
                    -I${PROTO_PATH} ${PROTO_FILE}
                DEPENDS ${PROTO_FILE}
                COMMENT "Generating protobuf files for ${PROTO_FILE}"
            )
        endforeach()

        # 创建 protobuf 库
        add_library(protobuf_generated STATIC ${PROTO_SRC} ${PROTO_HDR})
        target_include_directories(protobuf_generated PUBLIC
            ${GENERATED_PROTOBUF_PATH}
        )
        target_link_libraries(protobuf_generated PUBLIC protobuf::libprotobuf)
    else()
        message(WARNING "protoc not found, protobuf code generation disabled")
    endif()
else()
    message(STATUS "OpenRTB proto file not found, protobuf support disabled")
endif()

# 数据模型库
add_library(data_models INTERFACE)
target_include_directories(data_models INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/models
)

# 数据生成器
add_subdirectory(generator)
