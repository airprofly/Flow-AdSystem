cmake_minimum_required(VERSION 3.20)

# GTest is provided via FetchContent in the root CMakeLists.txt

# OpenRTB Parser 测试
add_executable(openrtb_parser_test
    unit/openrtb_parser_test.cpp
)

target_link_libraries(openrtb_parser_test
    PRIVATE
        openrtb_parser
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(openrtb_parser_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# 添加测试
include(GoogleTest)
gtest_discover_tests(openrtb_parser_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)
