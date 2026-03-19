#include "edgequant/quantizer.h"
#include "edgequant/version.h"  // 包含 PLATFORM 宏
#include <iostream>

namespace edgequant {

void quantize_tensor(float* data, size_t size) {
    std::cout << "[Quantizer] 开始量化张量，平台: " << COMPILE_PLATFORM << std::endl;
    std::cout << "[Quantizer] 张量大小: " << size << " 元素" << std::endl;

    for (size_t i = 0; i < size; ++i) {
        // 简单对称量化：[-1,1] -> [-127,127]，模拟 INT8
        float scaled = data[i] * 127.0f;
        int8_t q = static_cast<int8_t>(scaled);
        data[i] = static_cast<float>(q) / 127.0f;  // 反量化回 float
    }

    std::cout << "[Quantizer] 量化完成。" << std::endl;
}

} // namespace edgequant