#include <iostream>
#include <random>
#include <fstream>
#include "edgequant/version.h"
#include "edgequant/quantizer.h"
#include "edgequant/args_parser.h"



int main(int argc, char* argv[]) {
    auto args = edgequant::parse_arguments(argc, argv);


    std::cout << "工具包版本" << TOOLKIT_VERSION << "\n";
    std::cout << "编译平台: " << COMPILE_PLATFORM << "\n";
    std::cout << "CMake版本: " << CMAKE_VERSION << "\n";
    std::cout << "编译时间: " << COMPILE_TIME << "\n\n";


    // // 对比配置时版本和运行时环境版本，避免混淆
    // std::cout << "运行时检测到的 CMake 版本（通过 cmake --version 模拟）： " << "\n";

    std::cout << "完成初始化." << "\n";
    
    /*===================================*/
    // 测试张量
    /*===================================*/
    std::vector<float> tensor;
    if (!args.input_file.empty()) {
        // 简单从文件读（每行一个浮点数）
        std::ifstream in(args.input_file);
        if (!in) {
            std::cerr << "无法打开输入文件: " << args.input_file << "\n";
            return 1;
        }
        float val;
        while (in >> val) tensor.push_back(val);
        std::cout << "从文件读取 " << tensor.size() << " 个元素\n";
        } else {
            // 生成随机张量
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

            tensor.resize(args.tensor_size);
            for (auto& v : tensor) v = dis(gen);
            std::cout << "生成随机张量，大小: " << tensor.size() << "\n";
        }

        // 打印前5个元素（避免太大）
        std::cout << "原始前5: ";
        for (size_t i = 0; i < std::min<size_t>(5, tensor.size()); ++i)
            std::cout << tensor[i] << " ";
        std::cout << "\n";

        edgequant::quantize_tensor(tensor.data(), tensor.size());

        std::cout << "量化后前5: ";
        for (size_t i = 0; i < std::min<size_t>(5, tensor.size()); ++i)
            std::cout << tensor[i] << " ";
        std::cout << "\n";

        if (!args.output_file.empty()) {
            std::ofstream out(args.output_file);
            if (out) {
                for (float v : tensor) out << v << "\n";
                std::cout << "量化结果已保存到: " << args.output_file << "\n";
            }
        }


    return 0;
}