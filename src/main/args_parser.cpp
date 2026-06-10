#include "edgequant/args_parser.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace edgequant {

namespace {

void print_help(std::ostream& os) {
    os << "EdgeQuant Toolkit\n\n";
    os << "用法:\n";
    os << "  edgequant_tool [选项]\n\n";
    os << "旧 tensor demo 参数:\n";
    os << "  --size INT           张量大小，默认 100\n";
    os << "  --input TEXT         文本 float 张量输入文件路径\n";
    os << "  --output TEXT        tensor demo 输出文件路径\n\n";
    os << "工程接口参数:\n";
    os << "  --model TEXT         ONNX 模型路径\n";
    os << "  --calibration TEXT   校准数据目录\n";
    os << "  --output-dir TEXT    工程模式输出目录\n";
    os << "  --bit-width INT      量化位宽，默认 8\n";
    os << "  --platform TEXT      目标平台，默认 cpu\n";
    os << "  --config TEXT        可选配置文件路径\n";
    os << "  --mode TEXT          运行模式：auto / tensor-demo / onnx-report，默认 auto\n";
    os << "  -h, --help           显示帮助信息\n";
}

int parse_int_option(const std::string& option_name, const std::string& value) {
    try {
        std::size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    } catch (const std::exception&) {
        std::cerr << "错误：" << option_name << " 需要整数参数，收到: " << value << "\n";
        std::exit(1);
    }
}

std::string take_option_value(
    int& index,
    int argc,
    char* argv[],
    const std::string& option_name,
    const std::string& inline_value
) {
    if (!inline_value.empty()) {
        return inline_value;
    }
    if (index + 1 >= argc) {
        std::cerr << "错误：" << option_name << " 缺少参数值。\n";
        std::exit(1);
    }
    ++index;
    return argv[index];
}

void split_option(const std::string& raw, std::string& name, std::string& value) {
    const std::size_t equal_pos = raw.find('=');
    if (equal_pos == std::string::npos) {
        name = raw;
        value.clear();
        return;
    }
    name = raw.substr(0, equal_pos);
    value = raw.substr(equal_pos + 1);
}

bool is_valid_mode(const std::string& mode) {
    return mode == "auto" || mode == "tensor-demo" || mode == "onnx-report";
}

} // namespace

Args parse_arguments(int argc, char* argv[]) {
    Args args;

    for (int i = 1; i < argc; ++i) {
        std::string option_name;
        std::string inline_value;
        split_option(argv[i], option_name, inline_value);

        if (option_name == "-h" || option_name == "--help") {
            print_help(std::cout);
            std::exit(0);
        } else if (option_name == "--size") {
            args.tensor_size = parse_int_option(
                option_name,
                take_option_value(i, argc, argv, option_name, inline_value)
            );
        } else if (option_name == "--input") {
            args.input_file = take_option_value(i, argc, argv, option_name, inline_value);
        } else if (option_name == "--output") {
            args.output_file = take_option_value(i, argc, argv, option_name, inline_value);
        } else if (option_name == "--model") {
            args.model_file = take_option_value(i, argc, argv, option_name, inline_value);
        } else if (option_name == "--calibration") {
            args.calibration_dir = take_option_value(i, argc, argv, option_name, inline_value);
        } else if (option_name == "--output-dir") {
            args.output_dir = take_option_value(i, argc, argv, option_name, inline_value);
        } else if (option_name == "--bit-width") {
            args.bit_width = parse_int_option(
                option_name,
                take_option_value(i, argc, argv, option_name, inline_value)
            );
        } else if (option_name == "--platform") {
            args.platform = take_option_value(i, argc, argv, option_name, inline_value);
        } else if (option_name == "--config") {
            args.config_file = take_option_value(i, argc, argv, option_name, inline_value);
        } else if (option_name == "--mode") {
            args.mode = take_option_value(i, argc, argv, option_name, inline_value);
            if (!is_valid_mode(args.mode)) {
                std::cerr << "错误：不支持的运行模式: " << args.mode << "\n";
                print_help(std::cerr);
                std::exit(1);
            }
        } else {
            std::cerr << "错误：未知参数: " << option_name << "\n";
            print_help(std::cerr);
            std::exit(1);
        }
    }

    return args;
}

} // namespace edgequant
