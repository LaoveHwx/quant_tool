#!/bin/bash
set -e  # 遇到错误立即停止执行

# ================= 配置变量 =================
PROJECT_ROOT=$(pwd)
BUILD_DIR="${PROJECT_ROOT}/out/build"
OUTPUT_DIR="${PROJECT_ROOT}/output"
BUILD_TYPE="Release"

# 处理脚本参数 (支持 bash build.sh debug)
if [ "$1" == "debug" ]; then
    BUILD_TYPE="Debug"
    PRESET="ninja-debug"
else
    PRESET="ninja-release"
fi

echo "[INFO] 开始构建项目: ${BUILD_TYPE} 模式..."

# 1. 清理 
if [ "$2" == "clean" ]; then
    echo "[INFO] 清理旧的构建目录..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${OUTPUT_DIR}"
fi

# 2. 环境
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] 未找到 cmake，请先安装。"
    exit 1
fi

# 3. 执行 CMake 构建
# 使用Presets
cmake --preset ${PRESET}
cmake --build "${BUILD_DIR}/${PRESET}" --parallel $(nproc)

# 4. 权限设置
chmod +x ${OUTPUT_DIR}/bin/*

echo "======================================="
echo "[SUCCESS] 构建完成！"
echo "产物路径: ${OUTPUT_DIR}"
echo "运行命令: ./output/bin/edgequant_tool --size 10"
echo "======================================="