cmake_minimum_required(VERSION 3.18)

project(edgequant VERSION 0.1.0 LANGUAGES CXX)

# ================= 基础配置 =================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC -Wall -Wextra")

# ================= 输出目录 =================
set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR}/lib)

set(CMAKE_INSTALL_PREFIX ${OUTPUT_DIR}/install)

# ================= 平台检测 =================
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

# ================= Ascend mock =================
if(PLATFORM STREQUAL "ascend")
    set(ASCEND_INC "/usr/local/Ascend/ascend-toolkit/latest/include")
    set(ASCEND_LIB "/usr/local/Ascend/ascend-toolkit/latest/lib64")
else()
    set(ASCEND_INC ${CMAKE_SOURCE_DIR}/third_party/ascend_mock/include)
    set(ASCEND_LIB ${CMAKE_SOURCE_DIR}/third_party/ascend_mock/lib)
endif()

include_directories(${ASCEND_INC})

# 让链接器能找到 mock lib的改进
link_directories(${ASCEND_LIB})

# ================= 核心库 =================
add_library(edgequant_quantizer SHARED
    src/quantizer/quant_algorithm.cpp
    src/quantizer/tensor_utils.cpp
    src/quantizer/ascend_adapter.cpp
)

target_include_directories(edgequant_quantizer
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

# 不链接 ascendcl（避免错误）
target_link_libraries(edgequant_quantizer
    PRIVATE pthread dl
)

# ===================== CLI11 引入 =====================
include(FetchContent)

add_subdirectory(third_party/CLI11)
# ================= 主程序 =================
add_executable(edgequant_tool
    src/main/quant_tool_main.cpp
    src/main/args_parser.cpp
)

target_link_libraries(edgequant_tool
    PRIVATE edgequant_quantizer
    PRIVATE CLI11::CLI11
)

# 确保运行时能找到 .so
set_target_properties(edgequant_tool PROPERTIES
    BUILD_RPATH ${OUTPUT_DIR}/lib
)

# ================= version.h =================
string(TIMESTAMP COMPILE_TIME "%Y-%m-%d %H:%M:%S")

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_SOURCE_DIR}/include/edgequant/version.h
    @ONLY
)

# ================= install =================
install(TARGETS edgequant_tool edgequant_quantizer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)
install(FILES ${CMAKE_SOURCE_DIR}/config/quant_config.json DESTINATION config)