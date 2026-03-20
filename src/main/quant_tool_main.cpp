#include <iostream>
#include <random>
#include <fstream>
#include <filesystem>     // 用于路径操作和目录创建
#include <system_error>   // 用于 std::error_code
#include <cstring>        // strerror
#include "edgequant/version.h"
#include "edgequant/quantizer.h"
#include "edgequant/args_parser.h"

int main(int argc, char* argv[]) {
    auto args = edgequant::parse_arguments(argc, argv);

    std::cout << "工具包版本: " << TOOLKIT_VERSION << "\n";
    std::cout << "编译平台: " << COMPILE_PLATFORM << "\n";
    std::cout << "CMake版本: " << CMAKE_VERSION << "\n";
    std::cout << "编译时间: " << COMPILE_TIME << "\n\n";

    std::cout << "完成初始化.\n";

    // ===================================
    // 生成或读取张量
    // ===================================
    std::vector<float> tensor;
    if (!args.input_file.empty()) {
        std::ifstream in(args.input_file);
        if (!in.is_open()) {
            std::cerr << "无法打开输入文件: " << args.input_file << "\n";
            return 1;
        }
        float val;
        while (in >> val) tensor.push_back(val);
        std::cout << "从文件读取 " << tensor.size() << " 个元素\n";
    } else {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        tensor.resize(args.tensor_size);
        for (auto& v : tensor) v = dis(gen);
        std::cout << "生成随机张量，大小: " << tensor.size() << "\n";
    }

    // 打印前5个元素（避免输出太长）
    std::cout << "原始前5: ";
    for (size_t i = 0; i < std::min<size_t>(5, tensor.size()); ++i)
        std::cout << tensor[i] << " ";
    std::cout << "\n";

    edgequant::quantize_tensor(tensor.data(), tensor.size());

    std::cout << "量化后前5: ";
    for (size_t i = 0; i < std::min<size_t>(5, tensor.size()); ++i)
        std::cout << tensor[i] << " ";
    std::cout << "\n";

    // ===================================
    // 保存结果
    // ===================================

    std::filesystem::path output_path;
    if (!args.output_file.empty()) {
        output_path = args.output_file;
    } else {
        // 默认保存到 output/data/result.txt（自动创建 data 目录）

        // 获取可执行文件所在路径
        std::filesystem::path exe_path = std::filesystem::canonical(argv[0]);
        std::filesystem::path exe_dir = exe_path.parent_path();

        // 定位到项目 output 目录（../）
        std::filesystem::path project_root = exe_dir.parent_path().parent_path();

        output_path = project_root / "output" / "data" / "result.txt";
    }

    // 规范化路径
    output_path = output_path.lexically_normal();

    // 自动创建父目录（data/）
    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
    if (ec) {
        std::cerr << "创建输出目录失败: " << ec.message()
                << " (" << output_path.parent_path().string() << ")\n";
        return 1;
    }

    std::ofstream out(output_path, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "无法打开输出文件: " << output_path.string()
                << " (" << std::strerror(errno) << ")\n";
        return 1;
    }

    for (float v : tensor) {
        out << v << '\n';
    }

    if (out.fail()) {
        std::cerr << "写入文件失败: " << output_path.string() << "\n";
        return 1;
    }

    std::cout << "量化结果已保存到: " << output_path.string() << "\n";
        return 0;
}