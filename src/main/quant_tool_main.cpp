#include <iostream>
#include <random>
#include <fstream>
#include <filesystem>     // 用于路径操作和目录创建
#include <system_error>   // 用于 std::error_code
#include <cstring>        // strerror
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdint>
#include <cctype>
#include "edgequant/version.h"
#include "edgequant/quantizer.h"
#include "edgequant/args_parser.h"

namespace fs = std::filesystem;

namespace {

std::string json_escape(const std::string& value) {
    std::ostringstream escaped;
    for (unsigned char ch : value) {
        switch (ch) {
            case '"':
                escaped << "\\\"";
                break;
            case '\\':
                escaped << "\\\\";
                break;
            case '\b':
                escaped << "\\b";
                break;
            case '\f':
                escaped << "\\f";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(ch) << std::dec << std::setfill(' ');
                } else {
                    escaped << static_cast<char>(ch);
                }
                break;
        }
    }
    return escaped.str();
}

bool is_regular_file(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

bool is_directory(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_directory(path, ec);
}

std::uintmax_t safe_file_size(const fs::path& path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    return ec ? 0 : size;
}

std::size_t count_calibration_samples(const fs::path& calibration_dir) {
    const fs::path samples_dir = calibration_dir / "samples_npy";
    if (!is_directory(samples_dir)) {
        return 0;
    }

    std::size_t count = 0;
    std::error_code ec;
    fs::directory_iterator iter(samples_dir, ec);
    const fs::directory_iterator end;
    while (!ec && iter != end) {
        std::error_code entry_ec;
        if (iter->is_regular_file(entry_ec) && iter->path().extension() == ".npy") {
            ++count;
        }
        iter.increment(ec);
    }
    return count;
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

struct QuantReport {
    fs::path model_path;
    bool model_exists = false;
    std::uintmax_t model_size_bytes = 0;
    fs::path calibration_path;
    bool calibration_exists = false;
    bool calibration_manifest_exists = false;
    std::size_t calibration_sample_count = 0;
    fs::path output_dir;
    int bit_width = 8;
    std::string platform = "cpu";
};

bool write_quant_report(const fs::path& report_path, const QuantReport& report) {
    std::ofstream out(report_path, std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "无法打开报告文件: " << report_path.string()
                  << " (" << std::strerror(errno) << ")\n";
        return false;
    }

    out << "{\n";
    out << "  \"report_language\": \"zh-CN\",\n";
    out << "  \"tool_name\": \"edgequant_tool\",\n";
    out << "  \"tool_version\": \"" << json_escape(TOOLKIT_VERSION) << "\",\n";
    out << "  \"status\": \"unsupported\",\n";
    out << "  \"status_note\": \"当前版本尚未实现 ONNX 模型量化，仅完成输入检查和报告生成。\",\n";
    out << "  \"onnx_supported\": false,\n";
    out << "  \"calibration_supported\": false,\n";
    out << "  \"model_path\": \"" << json_escape(report.model_path.u8string()) << "\",\n";
    out << "  \"model_exists\": " << (report.model_exists ? "true" : "false") << ",\n";
    out << "  \"model_size_bytes\": " << report.model_size_bytes << ",\n";
    out << "  \"calibration_path\": \"" << json_escape(report.calibration_path.u8string()) << "\",\n";
    out << "  \"calibration_exists\": " << (report.calibration_exists ? "true" : "false") << ",\n";
    out << "  \"calibration_manifest_exists\": "
        << (report.calibration_manifest_exists ? "true" : "false") << ",\n";
    out << "  \"calibration_sample_count\": " << report.calibration_sample_count << ",\n";
    out << "  \"output_dir\": \"" << json_escape(report.output_dir.u8string()) << "\",\n";
    out << "  \"bit_width\": " << report.bit_width << ",\n";
    out << "  \"platform\": \"" << json_escape(report.platform) << "\",\n";
    out << "  \"next_action\": \"后续需要实现 ONNX 权重解析和 calibration 统计。\"\n";
    out << "}\n";

    if (out.fail()) {
        std::cerr << "写入报告文件失败: " << report_path.string() << "\n";
        return false;
    }
    return true;
}

int run_onnx_report(const edgequant::Args& args) {
    fs::path output_dir = args.output_dir.empty()
        ? (fs::path("output") / "quantized")
        : fs::path(args.output_dir);
    output_dir = output_dir.lexically_normal();

    std::error_code ec;
    fs::create_directories(output_dir, ec);
    if (ec) {
        std::cerr << "创建输出目录失败: " << ec.message()
                  << " (" << output_dir.string() << ")\n";
        return 1;
    }

    QuantReport report;
    report.model_path = fs::path(args.model_file).lexically_normal();
    report.calibration_path = fs::path(args.calibration_dir).lexically_normal();
    report.output_dir = output_dir;
    report.bit_width = args.bit_width;
    report.platform = args.platform;
    report.model_exists = !args.model_file.empty() && is_regular_file(report.model_path);
    report.model_size_bytes = report.model_exists ? safe_file_size(report.model_path) : 0;
    report.calibration_exists = !args.calibration_dir.empty() && is_directory(report.calibration_path);
    report.calibration_manifest_exists =
        report.calibration_exists && is_regular_file(report.calibration_path / "calibration_manifest.json");
    report.calibration_sample_count =
        report.calibration_exists ? count_calibration_samples(report.calibration_path) : 0;

    const fs::path report_path = output_dir / "quant_report.json";
    if (!write_quant_report(report_path, report)) {
        return 1;
    }

    bool has_error = false;
    if (args.model_file.empty()) {
        std::cerr << "错误：onnx-report 模式需要提供 --model 参数。\n";
        has_error = true;
    } else if (!report.model_exists) {
        std::cerr << "错误：模型文件不存在或不是普通文件: " << report.model_path.string() << "\n";
        has_error = true;
    }

    if (args.calibration_dir.empty()) {
        std::cerr << "错误：onnx-report 模式需要提供 --calibration 参数。\n";
        has_error = true;
    } else if (!report.calibration_exists) {
        std::cerr << "错误：校准数据目录不存在: " << report.calibration_path.string() << "\n";
        has_error = true;
    }

    if (args.bit_width != 8) {
        std::cerr << "错误：M1 当前仅支持 --bit-width 8，收到: " << args.bit_width << "\n";
        has_error = true;
    }

    if (has_error) {
        std::cerr << "检查失败，已写入报告: " << report_path.string() << "\n";
        return 1;
    }

    std::cout << "当前版本尚未实现 ONNX 模型量化，仅完成输入检查和报告生成。\n";
    std::cout << "ONNX 支持状态: false\n";
    std::cout << "校准 manifest 存在: "
              << (report.calibration_manifest_exists ? "true" : "false") << "\n";
    std::cout << "校准样本数量: " << report.calibration_sample_count << "\n";
    std::cout << "检查报告已生成: " << report_path.string() << "\n";
    return 0;
}

int run_tensor_demo(const edgequant::Args& args, char* argv[]) {
    std::cout << "运行模式: tensor-demo\n";

    // ===================================
    // 生成或读取张量
    // ===================================
    std::vector<float> tensor;
    if (!args.input_file.empty()) {
        const fs::path input_path = args.input_file;
        if (to_lower_ascii(input_path.extension().string()) == ".onnx") {
            std::cerr << "错误：--input 仅支持文本 float 张量文件，ONNX 模型请使用 --model。\n";
            return 1;
        }

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

} // namespace

int main(int argc, char* argv[]) {
    auto args = edgequant::parse_arguments(argc, argv);

    std::cout << "工具包版本: " << TOOLKIT_VERSION << "\n";
    std::cout << "编译平台: " << COMPILE_PLATFORM << "\n";
    std::cout << "CMake版本: " << CMAKE_VERSION << "\n";
    std::cout << "编译时间: " << COMPILE_TIME << "\n\n";

    std::cout << "完成初始化.\n";

    if (args.mode == "onnx-report" || (args.mode == "auto" && !args.model_file.empty())) {
        return run_onnx_report(args);
    }

    return run_tensor_demo(args, argv);
}
