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

# 频控模块测试
add_executable(frequency_test
    unit/frequency_test.cpp
)

target_link_libraries(frequency_test
    PRIVATE
        frequency_control
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(frequency_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Pacing 模块测试
add_executable(pacing_test
    unit/test_pacing.cpp
)

target_link_libraries(pacing_test
    PRIVATE
        pacing
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(pacing_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/core/pacing
)

# 核心模块集成测试: 索引 + 频控 + Pacing
add_executable(core_modules_integration_test
    integration/integration_test_index_frequency_pacing.cpp
)

target_link_libraries(core_modules_integration_test
    PRIVATE
        openrtb_parser
        ad_index
        frequency_control
        pacing
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(core_modules_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# CTR 模块测试
add_executable(ctr_test
    unit/ctr_test.cpp
)

target_link_libraries(ctr_test
    PRIVATE
        ctr_lib
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(ctr_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(ctr_test
    PRIVATE
        FLOW_ADSYSTEM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# CTR 端到端示例
add_executable(ctr_e2e_test
    unit/ctr_e2e_test.cpp
)

target_link_libraries(ctr_e2e_test
    PRIVATE
        ctr_lib
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(ctr_e2e_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(ctr_e2e_test
    PRIVATE
        FLOW_ADSYSTEM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# CTR 延迟基准
add_executable(ctr_latency_benchmark
    unit/ctr_latency_benchmark.cpp
)

target_link_libraries(ctr_latency_benchmark
    PRIVATE
        ctr_lib
)

target_include_directories(ctr_latency_benchmark
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(ctr_latency_benchmark
    PRIVATE
        FLOW_ADSYSTEM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
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

gtest_discover_tests(frequency_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)

gtest_discover_tests(pacing_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)

gtest_discover_tests(core_modules_integration_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)

gtest_discover_tests(ctr_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)

gtest_discover_tests(ctr_e2e_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)

# Ranker 模块测试
add_executable(ranker_test
    unit/ranker_test.cpp
)

target_link_libraries(ranker_test
    PRIVATE
        ranker
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(ranker_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

gtest_discover_tests(ranker_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    DISCOVERY_MODE PRE_TEST
)
