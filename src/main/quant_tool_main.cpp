#include <iostream>

#include "edgequant/version.h"// 自动生成的版本头文件
#include "edgequant/quantizer.h"

int main() {
    std::cout << "EdgeQuant Toolkit v" << TOOLKIT_VERSION << std::endl;
    std::cout << "Compile Platform: " << COMPILE_PLATFORM << std::endl;
    std::cout << "CMake Version used: " << CMAKE_VERSION << std::endl;
    std::cout << "Compiled at: " << COMPILE_TIME << std::endl<< std::endl;


    // 对比配置时版本和运行时环境版本，避免混淆
    std::cout << "CMake command version: " << CMAKE_VERSION << std::endl;
    std::cout << "Runtime detected CMake version (via cmake --version simulation): " << std::endl;

    std::cout << "Ready for quantization." << std::endl;
    
    /*===================================*/
    // 测试张量
    /*===================================*/
    float tensor[] = {0.1f, 0.5f, 0.9f, -0.3f, 1.2f};
    size_t size = sizeof(tensor) / sizeof(float);

    std::cout << "原始张量: ";
    for (size_t i = 0; i < size; ++i) std::cout << tensor[i] << " ";
    std::cout << std::endl;

    edgequant::quantize_tensor(tensor, size);

    std::cout << "量化后张量: ";
    for (size_t i = 0; i < size; ++i) std::cout << tensor[i] << " ";
    std::cout << std::endl;


    return 0;
}