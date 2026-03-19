#pragma once

#include <string>
#include <vector>

namespace edgequant {

struct Args {
    int tensor_size = 100;              // 默认张量大小
    std::string input_file = "";        // 输入文件（可选）
    std::string output_file = "";       // 输出文件（可选）
    bool show_help = false;
};

Args parse_arguments(int argc, char* argv[]);


} // namespace edgequant