#pragma once

#include <cstddef>
#include <string>

namespace edgequant {

struct OnnxWeightExportOptions {
    std::string model_file;
    std::string calibration_dir;
    std::string output_dir;
    std::string platform = "cpu";
    int bit_width = 8;
};

struct OnnxWeightExportResult {
    bool success = false;
    std::string message;
    std::size_t exported_tensor_count = 0;
    std::size_t skipped_initializer_count = 0;
    std::size_t weight_bytes = 0;
    std::string report_path;
    std::string quant_params_path;
    std::string int8_weight_path;
};

OnnxWeightExportResult export_onnx_weights_to_m2_artifacts(
    const OnnxWeightExportOptions& options
);

} // namespace edgequant
