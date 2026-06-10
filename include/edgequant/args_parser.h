#pragma once

#include <string>
#include <vector>

namespace edgequant {

struct Args {
    int tensor_size = 100;              // 默认张量大小
    std::string input_file = "";        // 输入文件（可选）
    std::string output_file = "";       // 输出文件（可选）
    std::string model_file = "";        // ONNX 模型路径（可选）
    std::string calibration_dir = "";   // 校准数据目录（可选）
    std::string output_dir = "";        // 工程模式输出目录（可选）
    std::string config_file = "";       // 配置文件路径（可选）
    std::string platform = "cpu";       // 目标平台
    std::string mode = "auto";          // auto / tensor-demo / onnx-report
    int bit_width = 8;                  // 量化位宽
    bool show_help = false;
};

Args parse_arguments(int argc, char* argv[]);


} // namespace edgequant
