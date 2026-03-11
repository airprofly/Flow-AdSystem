# 广告数据生成器 CMakeLists.txt

# 可执行文件目标
add_executable(ad_generator
    main.cpp
    ad_generator.cpp
    data_loader.cpp
)

# 包含目录
target_include_directories(ad_generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../models
)

# 链接库
target_link_libraries(ad_generator PRIVATE
    data_models
)

# 如果有 protobuf 生成的库,也链接它
if(TARGET protobuf_generated)
    target_link_libraries(ad_generator PRIVATE protobuf_generated)
endif()

# 设置输出目录
set_target_properties(ad_generator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装规则
install(TARGETS ad_generator
    RUNTIME DESTINATION bin
)
