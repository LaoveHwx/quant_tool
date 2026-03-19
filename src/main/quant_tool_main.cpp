#include <iostream>
#include "edgequant/version.h"     // 自动生成的版本头文件
#include "edgequant/quantizer.h"

int main() {
    std::cout << "EdgeQuant Toolkit v" << TOOLKIT_VERSION << std::endl;
    std::cout << "Compile Platform: " << COMPILE_PLATFORM << std::endl;
    std::cout << "CMake Version used: " << CMAKE_VERSION << std::endl;
    std::cout << "Compiled at: " << COMPILE_TIME << std::endl;
    // 对比配置时版本和运行时环境版本，避免混淆
    std::cout << "CMake command version: " << CMAKE_VERSION << std::endl;
    std::cout << "Runtime detected CMake version (via cmake --version simulation): " << std::endl;

    std::cout << "Ready for quantization test." << std::endl;
    return 0;
}