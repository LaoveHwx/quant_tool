# ===================== 基础配置（华为内部规范）=====================
cmake_minimum_required(VERSION 3.18)

project(edgequant VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC -Wall -Wextra")

# 输出目录标准化（华为规范）
set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR}/lib)
set(CMAKE_INSTALL_PREFIX ${OUTPUT_DIR}/install)

# ===================== 跨平台检测 =====================
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(PLATFORM "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(PLATFORM "arm64")
elseif(DEFINED ENV{ASCEND_TARGET_PLATFORM})
    set(PLATFORM "ascend")
else()
    set(PLATFORM "unknown")
endif()
message(STATUS "当前编译平台: ${PLATFORM}")

# ===================== Ascend CANN 依赖（mock 机制） =====================
set(ASCEND_ROOT "/usr/local/Ascend/ascend-toolkit/latest")
if(PLATFORM STREQUAL "ascend")
    set(ASCEND_INC ${ASCEND_ROOT}/include)
    set(ASCEND_LIB ${ASCEND_ROOT}/lib64)
else()
    set(ASCEND_INC ${CMAKE_SOURCE_DIR}/third_party/ascend_mock/include)
    set(ASCEND_LIB ${CMAKE_SOURCE_DIR}/third_party/ascend_mock/lib)
endif()
include_directories(${ASCEND_INC})
link_directories(${ASCEND_LIB})

# ===================== 核心模块：量化库（共享库） =====================
add_library(edgequant_quantizer SHARED
    src/quantizer/quant_algorithm.cpp
    src/quantizer/tensor_utils.cpp
    src/quantizer/ascend_adapter.cpp
)
target_include_directories(edgequant_quantizer
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(edgequant_quantizer
    PRIVATE ascendcl pthread dl   # mock 时也能链接
)

# ===================== 主程序 =====================
add_executable(edgequant_tool
    src/main/quant_tool_main.cpp
)
target_link_libraries(edgequant_tool PRIVATE edgequant_quantizer)

# ===================== 版本信息自动生成（华为必做） =====================
string(TIMESTAMP COMPILE_TIME "%Y-%m-%d %H:%M:%S")
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_SOURCE_DIR}/include/edgequant/version.h
    @ONLY
)

# ===================== 安装规则（标准化部署） =====================
install(TARGETS edgequant_tool edgequant_quantizer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)
install(FILES ${CMAKE_SOURCE_DIR}/config/quant_config.json DESTINATION config)