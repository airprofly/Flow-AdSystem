# Ranker Module CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(flow_ad_ranker)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Ranker 库
add_library(ranker STATIC
    ad_candidate.hpp
    ecpm_calculator.hpp
    ecpm_calculator.cpp
    sorting_strategy.hpp
    topk_selector.hpp
    topk_selector.cpp
    ranker.hpp
    ranker.cpp
)

target_include_directories(ranker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# 并行算法支持
# 如果主 CMakeLists.txt 中启用了 TBB,则链接 TBB
if(TARGET TBB::tbb)
    target_link_libraries(ranker PUBLIC TBB::tbb)
    target_compile_definitions(ranker PRIVATE FLOW_ADSYSTEM_HAS_PARALLEL_ALGORITHMS=1)
endif()

# 导出库
set_target_properties(ranker PROPERTIES
    OUTPUT_NAME "flow_ad_ranker"
    VERSION 1.0.0
    SOVERSION 1
)

# 安装
install(TARGETS ranker
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    ad_candidate.hpp
    ecpm_calculator.hpp
    sorting_strategy.hpp
    topk_selector.hpp
    ranker.hpp
    DESTINATION include/flow_ad_system/ranker
)
