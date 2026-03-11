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

# Ad Indexing 测试
add_executable(indexing_test
    unit/indexing_test.cpp
)

target_link_libraries(indexing_test
    PRIVATE
        ad_index
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(indexing_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# 集成测试：OpenRTB + 索引模块
add_executable(integration_test_openrtb_indexing
    integration/integration_test_openrtb_indexing.cpp
)

target_link_libraries(integration_test_openrtb_indexing
    PRIVATE
        openrtb_parser
        ad_index
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(integration_test_openrtb_indexing
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# 添加测试
include(GoogleTest)
gtest_discover_tests(openrtb_parser_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)

gtest_discover_tests(indexing_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)

gtest_discover_tests(integration_test_openrtb_indexing
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)
