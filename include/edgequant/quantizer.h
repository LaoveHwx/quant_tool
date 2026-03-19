#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace edgequant {

// 极简张量量化函数（模拟 PTQ）
void quantize_tensor(float* data, size_t size);

// 未来扩展：返回量化误差或统计信息
// std::vector<float> compute_quant_error(const float* orig, const float* quant, size_t size);

} // namespace edgequant