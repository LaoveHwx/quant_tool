#include "edgequant/onnx_weight_exporter.h"

#include "edgequant/version.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace edgequant {
namespace {

struct ByteSpan {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

struct ProtoField {
    std::uint32_t number = 0;
    std::uint32_t wire_type = 0;
    std::uint64_t varint = 0;
    ByteSpan bytes;
};

class ProtoReader {
public:
    explicit ProtoReader(ByteSpan span) : span_(span) {}

    bool eof() const {
        return pos_ >= span_.size;
    }

    ProtoField read_field() {
        ProtoField field;
        const std::uint64_t key = read_varint();
        field.number = static_cast<std::uint32_t>(key >> 3);
        field.wire_type = static_cast<std::uint32_t>(key & 0x07);

        switch (field.wire_type) {
            case 0:
                field.varint = read_varint();
                break;
            case 1:
                field.bytes = read_bytes(8);
                break;
            case 2: {
                const std::uint64_t length = read_varint();
                if (length > std::numeric_limits<std::size_t>::max()) {
                    throw std::runtime_error("length-delimited field is too large");
                }
                field.bytes = read_bytes(static_cast<std::size_t>(length));
                break;
            }
            case 5:
                field.bytes = read_bytes(4);
                break;
            default:
                throw std::runtime_error("unsupported protobuf wire type");
        }

        return field;
    }

private:
    std::uint64_t read_varint() {
        std::uint64_t value = 0;
        int shift = 0;
        while (shift <= 63) {
            if (pos_ >= span_.size) {
                throw std::runtime_error("unexpected EOF while reading varint");
            }
            const std::uint8_t byte = span_.data[pos_++];
            value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0) {
                return value;
            }
            shift += 7;
        }
        throw std::runtime_error("varint is too long");
    }

    ByteSpan read_bytes(std::size_t length) {
        if (length > span_.size - pos_) {
            throw std::runtime_error("unexpected EOF while reading bytes");
        }
        ByteSpan out{span_.data + pos_, length};
        pos_ += length;
        return out;
    }

    ByteSpan span_;
    std::size_t pos_ = 0;
};

struct OnnxNode {
    std::string name;
    std::string op_type;
    std::vector<std::string> inputs;
};

struct OnnxTensor {
    std::string name;
    int data_type = 0;
    std::vector<std::int64_t> dims;
    std::vector<float> values;
};

struct ExportTensor {
    std::string name;
    std::string source_node;
    std::vector<std::int64_t> shape;
    float scale = 1.0f;
    std::size_t offset_bytes = 0;
    std::size_t size_bytes = 0;
};

std::string bytes_to_string(ByteSpan bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.data), bytes.size);
}

std::uint32_t read_little_u32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0])
        | (static_cast<std::uint32_t>(data[1]) << 8)
        | (static_cast<std::uint32_t>(data[2]) << 16)
        | (static_cast<std::uint32_t>(data[3]) << 24);
}

float read_little_float32(const std::uint8_t* data) {
    const std::uint32_t bits = read_little_u32(data);
    float value = 0.0f;
    static_assert(sizeof(value) == sizeof(bits), "float32 expected");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::vector<std::uint64_t> read_packed_varints(ByteSpan bytes) {
    std::vector<std::uint64_t> values;
    std::size_t pos = 0;
    while (pos < bytes.size) {
        std::uint64_t value = 0;
        int shift = 0;
        while (shift <= 63) {
            if (pos >= bytes.size) {
                throw std::runtime_error("unexpected EOF in packed varints");
            }
            const std::uint8_t byte = bytes.data[pos++];
            value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0) {
                values.push_back(value);
                break;
            }
            shift += 7;
        }
        if (shift > 63) {
            throw std::runtime_error("packed varint is too long");
        }
    }
    return values;
}

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
                    escaped << "\\u00";
                    const char* digits = "0123456789abcdef";
                    escaped << digits[(ch >> 4) & 0x0f] << digits[ch & 0x0f];
                } else {
                    escaped << static_cast<char>(ch);
                }
                break;
        }
    }
    return escaped.str();
}

bool path_is_regular_file(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

bool path_is_directory(const fs::path& path) {
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
    if (!path_is_directory(samples_dir)) {
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

std::vector<std::uint8_t> read_file_bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("无法打开 ONNX 模型文件: " + path.string());
    }
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size <= 0) {
        throw std::runtime_error("ONNX 模型文件为空: " + path.string());
    }
    input.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("读取 ONNX 模型文件失败: " + path.string());
    }
    return data;
}

OnnxNode parse_node(ByteSpan bytes) {
    OnnxNode node;
    ProtoReader reader(bytes);
    while (!reader.eof()) {
        const ProtoField field = reader.read_field();
        if (field.number == 1 && field.wire_type == 2) {
            node.inputs.push_back(bytes_to_string(field.bytes));
        } else if (field.number == 3 && field.wire_type == 2) {
            node.name = bytes_to_string(field.bytes);
        } else if (field.number == 4 && field.wire_type == 2) {
            node.op_type = bytes_to_string(field.bytes);
        }
    }
    return node;
}

OnnxTensor parse_tensor(ByteSpan bytes) {
    OnnxTensor tensor;
    ByteSpan raw_data;
    std::vector<float> float_data;

    ProtoReader reader(bytes);
    while (!reader.eof()) {
        const ProtoField field = reader.read_field();
        if (field.number == 1 && field.wire_type == 0) {
            tensor.dims.push_back(static_cast<std::int64_t>(field.varint));
        } else if (field.number == 1 && field.wire_type == 2) {
            for (std::uint64_t dim : read_packed_varints(field.bytes)) {
                tensor.dims.push_back(static_cast<std::int64_t>(dim));
            }
        } else if (field.number == 2 && field.wire_type == 0) {
            tensor.data_type = static_cast<int>(field.varint);
        } else if (field.number == 4 && field.wire_type == 5) {
            float_data.push_back(read_little_float32(field.bytes.data));
        } else if (field.number == 4 && field.wire_type == 2) {
            if (field.bytes.size % sizeof(float) != 0) {
                throw std::runtime_error("float_data packed bytes are not aligned");
            }
            for (std::size_t i = 0; i < field.bytes.size; i += sizeof(float)) {
                float_data.push_back(read_little_float32(field.bytes.data + i));
            }
        } else if (field.number == 8 && field.wire_type == 2) {
            tensor.name = bytes_to_string(field.bytes);
        } else if (field.number == 9 && field.wire_type == 2) {
            raw_data = field.bytes;
        }
    }

    if (tensor.data_type == 1) {
        if (raw_data.data != nullptr && raw_data.size > 0) {
            if (raw_data.size % sizeof(float) != 0) {
                throw std::runtime_error("raw_data bytes are not float32 aligned for tensor: " + tensor.name);
            }
            for (std::size_t i = 0; i < raw_data.size; i += sizeof(float)) {
                tensor.values.push_back(read_little_float32(raw_data.data + i));
            }
        } else {
            tensor.values = std::move(float_data);
        }
    }
    return tensor;
}

void parse_graph(
    ByteSpan bytes,
    std::vector<OnnxNode>& nodes,
    std::map<std::string, OnnxTensor>& initializers
) {
    ProtoReader reader(bytes);
    while (!reader.eof()) {
        const ProtoField field = reader.read_field();
        if (field.number == 1 && field.wire_type == 2) {
            nodes.push_back(parse_node(field.bytes));
        } else if (field.number == 5 && field.wire_type == 2) {
            OnnxTensor tensor = parse_tensor(field.bytes);
            if (!tensor.name.empty()) {
                initializers[tensor.name] = std::move(tensor);
            }
        }
    }
}

void parse_model(
    const std::vector<std::uint8_t>& model_bytes,
    std::vector<OnnxNode>& nodes,
    std::map<std::string, OnnxTensor>& initializers
) {
    ProtoReader reader(ByteSpan{model_bytes.data(), model_bytes.size()});
    bool graph_found = false;
    while (!reader.eof()) {
        const ProtoField field = reader.read_field();
        if (field.number == 7 && field.wire_type == 2) {
            parse_graph(field.bytes, nodes, initializers);
            graph_found = true;
        }
    }

    if (!graph_found) {
        throw std::runtime_error("ONNX 模型中未找到 graph 字段");
    }
}

bool is_weight_export_op(const std::string& op_type) {
    return op_type == "Conv" || op_type == "Gemm" || op_type == "MatMul" || op_type == "Linear";
}

std::size_t tensor_element_count(const std::vector<std::int64_t>& dims) {
    if (dims.empty()) {
        return 0;
    }

    std::size_t count = 1;
    for (std::int64_t dim : dims) {
        if (dim <= 0) {
            return 0;
        }
        if (count > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(dim)) {
            throw std::runtime_error("tensor shape is too large");
        }
        count *= static_cast<std::size_t>(dim);
    }
    return count;
}

float compute_symmetric_scale(const std::vector<float>& values) {
    float max_abs = 0.0f;
    for (float value : values) {
        if (std::isfinite(value)) {
            max_abs = std::max(max_abs, std::fabs(value));
        }
    }
    return max_abs > 0.0f ? max_abs / 127.0f : 1.0f;
}

std::int8_t quantize_value(float value, float scale) {
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    const float scaled = std::round(value / scale);
    const int clamped = std::max(-127, std::min(127, static_cast<int>(scaled)));
    return static_cast<std::int8_t>(clamped);
}

std::vector<ExportTensor> export_quantized_weights(
    const std::vector<OnnxNode>& nodes,
    const std::map<std::string, OnnxTensor>& initializers,
    const fs::path& weight_path,
    std::size_t& skipped_initializer_count
) {
    std::ofstream weight_out(weight_path, std::ios::binary | std::ios::trunc);
    if (!weight_out.is_open()) {
        throw std::runtime_error("无法打开 INT8 权重输出文件: " + weight_path.string());
    }

    std::vector<ExportTensor> exported;
    std::set<std::string> exported_names;

    for (std::size_t node_index = 0; node_index < nodes.size(); ++node_index) {
        const OnnxNode& node = nodes[node_index];
        if (!is_weight_export_op(node.op_type) || node.inputs.size() < 2) {
            continue;
        }

        const std::string& weight_name = node.inputs[1];
        const auto tensor_iter = initializers.find(weight_name);
        if (tensor_iter == initializers.end() || exported_names.count(weight_name) > 0) {
            continue;
        }

        const OnnxTensor& tensor = tensor_iter->second;
        if (tensor.data_type != 1 || tensor.values.empty()) {
            continue;
        }

        const std::size_t expected_count = tensor_element_count(tensor.dims);
        if (expected_count == 0 || expected_count != tensor.values.size()) {
            continue;
        }

        ExportTensor exported_tensor;
        exported_tensor.name = tensor.name;
        exported_tensor.source_node = node.name.empty()
            ? (node.op_type + "_" + std::to_string(node_index))
            : node.name;
        exported_tensor.shape = tensor.dims;
        exported_tensor.scale = compute_symmetric_scale(tensor.values);
        const std::streamoff current_offset = weight_out.tellp();
        if (current_offset < 0) {
            throw std::runtime_error("获取 INT8 权重写入偏移失败");
        }
        exported_tensor.offset_bytes = static_cast<std::size_t>(current_offset);

        for (float value : tensor.values) {
            const std::int8_t q = quantize_value(value, exported_tensor.scale);
            weight_out.write(reinterpret_cast<const char*>(&q), sizeof(q));
        }
        if (!weight_out) {
            throw std::runtime_error("写入 INT8 权重失败: " + weight_path.string());
        }

        exported_tensor.size_bytes = tensor.values.size();
        exported.push_back(std::move(exported_tensor));
        exported_names.insert(weight_name);
    }

    skipped_initializer_count = 0;
    for (const auto& item : initializers) {
        if (exported_names.count(item.first) == 0) {
            ++skipped_initializer_count;
        }
    }

    if (exported.empty()) {
        throw std::runtime_error("未找到可导出的 Conv/Gemm/MatMul/Linear float32 权重 initializer");
    }

    return exported;
}

void write_quant_params(
    const fs::path& params_path,
    const fs::path& model_path,
    const std::vector<ExportTensor>& tensors
) {
    std::ofstream out(params_path, std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("无法打开 quant_params.json: " + params_path.string());
    }

    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"tool_name\": \"edgequant_tool\",\n";
    out << "  \"tool_version\": \"" << json_escape(TOOLKIT_VERSION) << "\",\n";
    out << "  \"bit_width\": 8,\n";
    out << "  \"byte_order\": \"little_endian\",\n";
    out << "  \"weight_file\": \"int8_weight.bin\",\n";
    out << "  \"model\": {\n";
    out << "    \"source_format\": \"onnx\",\n";
    out << "    \"source_path\": \"" << json_escape(model_path.string()) << "\"\n";
    out << "  },\n";
    out << "  \"tensors\": [\n";
    for (std::size_t i = 0; i < tensors.size(); ++i) {
        const ExportTensor& tensor = tensors[i];
        out << "    {\n";
        out << "      \"name\": \"" << json_escape(tensor.name) << "\",\n";
        out << "      \"source_node\": \"" << json_escape(tensor.source_node) << "\",\n";
        out << "      \"role\": \"weight\",\n";
        out << "      \"data_type\": \"int8\",\n";
        out << "      \"shape\": [";
        for (std::size_t j = 0; j < tensor.shape.size(); ++j) {
            if (j > 0) {
                out << ", ";
            }
            out << tensor.shape[j];
        }
        out << "],\n";
        out << "      \"scale\": " << tensor.scale << ",\n";
        out << "      \"zero_point\": 0,\n";
        out << "      \"quantization\": \"per_tensor\",\n";
        out << "      \"quantization_axis\": null,\n";
        out << "      \"offset_bytes\": " << tensor.offset_bytes << ",\n";
        out << "      \"size_bytes\": " << tensor.size_bytes << "\n";
        out << "    }" << (i + 1 == tensors.size() ? "\n" : ",\n");
    }
    out << "  ]\n";
    out << "}\n";

    if (!out) {
        throw std::runtime_error("写入 quant_params.json 失败: " + params_path.string());
    }
}

void write_report(
    const fs::path& report_path,
    const OnnxWeightExportOptions& options,
    const fs::path& model_path,
    const fs::path& calibration_path,
    const fs::path& output_dir,
    std::uintmax_t model_size,
    bool calibration_manifest_exists,
    std::size_t calibration_sample_count,
    std::size_t tensor_count,
    std::size_t skipped_initializer_count,
    std::size_t weight_bytes
) {
    std::ofstream out(report_path, std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("无法打开 quant_report.json: " + report_path.string());
    }

    out << "{\n";
    out << "  \"report_language\": \"zh-CN\",\n";
    out << "  \"tool_name\": \"edgequant_tool\",\n";
    out << "  \"tool_version\": \"" << json_escape(TOOLKIT_VERSION) << "\",\n";
    out << "  \"status\": \"success\",\n";
    out << "  \"status_note\": \"M2-A 已完成 ONNX initializer 权重量化；activation calibration 和完整 Runtime 对接尚未实现。\",\n";
    out << "  \"onnx_supported\": true,\n";
    out << "  \"calibration_supported\": true,\n";
    out << "  \"model_path\": \"" << json_escape(model_path.string()) << "\",\n";
    out << "  \"model_exists\": true,\n";
    out << "  \"model_size_bytes\": " << model_size << ",\n";
    out << "  \"calibration_path\": \"" << json_escape(calibration_path.string()) << "\",\n";
    out << "  \"calibration_exists\": true,\n";
    out << "  \"calibration_manifest_exists\": "
        << (calibration_manifest_exists ? "true" : "false") << ",\n";
    out << "  \"calibration_sample_count\": " << calibration_sample_count << ",\n";
    out << "  \"output_dir\": \"" << json_escape(output_dir.string()) << "\",\n";
    out << "  \"bit_width\": " << options.bit_width << ",\n";
    out << "  \"platform\": \"" << json_escape(options.platform) << "\",\n";
    out << "  \"exported_tensor_count\": " << tensor_count << ",\n";
    out << "  \"skipped_initializer_count\": " << skipped_initializer_count << ",\n";
    out << "  \"int8_weight_size_bytes\": " << weight_bytes << ",\n";
    out << "  \"artifacts\": {\n";
    out << "    \"int8_weight\": \"int8_weight.bin\",\n";
    out << "    \"quant_params\": \"quant_params.json\"\n";
    out << "  },\n";
    out << "  \"next_action\": \"后续需要实现 activation calibration、per-channel 量化和 Runtime 加载验证。\"\n";
    out << "}\n";

    if (!out) {
        throw std::runtime_error("写入 quant_report.json 失败: " + report_path.string());
    }
}

OnnxWeightExportResult make_error_result(const std::string& message) {
    OnnxWeightExportResult result;
    result.success = false;
    result.message = message;
    return result;
}

} // namespace

OnnxWeightExportResult export_onnx_weights_to_m2_artifacts(
    const OnnxWeightExportOptions& options
) {
    try {
        if (options.bit_width != 8) {
            return make_error_result("M2-A 当前仅支持 bit_width=8");
        }
        if (options.model_file.empty()) {
            return make_error_result("M2-A 需要提供 --model");
        }
        if (options.calibration_dir.empty()) {
            return make_error_result("M2-A 需要提供 --calibration");
        }
        if (options.output_dir.empty()) {
            return make_error_result("M2-A 需要提供 --output-dir");
        }

        const fs::path model_path = fs::path(options.model_file).lexically_normal();
        const fs::path calibration_path = fs::path(options.calibration_dir).lexically_normal();
        const fs::path output_dir = fs::path(options.output_dir).lexically_normal();

        if (!path_is_regular_file(model_path)) {
            return make_error_result("模型文件不存在或不是普通文件: " + model_path.string());
        }
        if (!path_is_directory(calibration_path)) {
            return make_error_result("校准数据目录不存在: " + calibration_path.string());
        }

        std::error_code ec;
        fs::create_directories(output_dir, ec);
        if (ec) {
            return make_error_result("创建输出目录失败: " + ec.message());
        }

        std::vector<OnnxNode> nodes;
        std::map<std::string, OnnxTensor> initializers;
        parse_model(read_file_bytes(model_path), nodes, initializers);

        const fs::path weight_path = output_dir / "int8_weight.bin";
        std::size_t skipped_initializer_count = 0;
        const std::vector<ExportTensor> tensors = export_quantized_weights(
            nodes,
            initializers,
            weight_path,
            skipped_initializer_count
        );

        const std::uintmax_t weight_size = safe_file_size(weight_path);
        const fs::path params_path = output_dir / "quant_params.json";
        const fs::path report_path = output_dir / "quant_report.json";

        write_quant_params(params_path, model_path, tensors);
        write_report(
            report_path,
            options,
            model_path,
            calibration_path,
            output_dir,
            safe_file_size(model_path),
            path_is_regular_file(calibration_path / "calibration_manifest.json"),
            count_calibration_samples(calibration_path),
            tensors.size(),
            skipped_initializer_count,
            static_cast<std::size_t>(weight_size)
        );

        OnnxWeightExportResult result;
        result.success = true;
        result.message = "M2-A ONNX initializer 权重量化完成";
        result.exported_tensor_count = tensors.size();
        result.skipped_initializer_count = skipped_initializer_count;
        result.weight_bytes = static_cast<std::size_t>(weight_size);
        result.report_path = report_path.string();
        result.quant_params_path = params_path.string();
        result.int8_weight_path = weight_path.string();
        return result;
    } catch (const std::exception& exc) {
        return make_error_result(exc.what());
    }
}

} // namespace edgequant
