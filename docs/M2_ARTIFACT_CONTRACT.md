# M2 Artifact Contract

M2 的目标是让 quant-tool 生成 LiteEdgeINT Runtime 可以消费的真实 INT8 产物。

M2 不应只生成空文件或占位文件。只有满足本文契约的输出目录，才允许 bridge 或 Runtime 进入后续权重加载阶段。

## Required Files

一个成功的 M2 输出目录必须包含：

```text
quant_report.json
quant_params.json
int8_weight.bin
```

禁止把 M1 的 `status=unsupported` 报告当作 M2 成功产物。

## Success Rules

`quant_report.json` 必须满足：

- `status == "success"`
- `onnx_supported == true`
- `calibration_supported == true`
- `artifacts.int8_weight == "int8_weight.bin"`
- `artifacts.quant_params == "quant_params.json"`
- `model_path` 指向输入 ONNX 模型
- `output_dir` 指向当前输出目录

`quant_params.json` 必须满足：

- `schema_version == 1`
- `tool_name == "edgequant_tool"`
- `bit_width == 8`
- `byte_order == "little_endian"`
- `weight_file == "int8_weight.bin"`
- `tensors` 是非空数组

`int8_weight.bin` 必须满足：

- 文件存在
- 文件大小大于 0
- `quant_params.json` 中所有 tensor 的 `offset_bytes + size_bytes` 不得超过文件大小

## Tensor Entry

`quant_params.json.tensors[]` 中每个 tensor 必须包含：

```json
{
  "name": "conv1.weight",
  "source_node": "conv1",
  "role": "weight",
  "data_type": "int8",
  "shape": [4, 1, 3, 3],
  "scale": 0.0125,
  "zero_point": 0,
  "quantization": "per_tensor",
  "quantization_axis": null,
  "offset_bytes": 0,
  "size_bytes": 36
}
```

字段约束：

- `name`：非空字符串。
- `source_node`：非空字符串。
- `role`：`weight` 或 `activation`。
- `data_type`：M2 固定为 `int8`。
- `shape`：非空整数数组，每个维度必须大于 0。
- `scale`：大于 0 的数字。
- `zero_point`：整数，M2 建议固定为 0。
- `quantization`：`per_tensor` 或 `per_channel`。
- `quantization_axis`：`per_tensor` 时为 `null`，`per_channel` 时为整数。
- `offset_bytes`：大于等于 0 的整数。
- `size_bytes`：大于 0 的整数。

## M2 Boundary

M2 第一阶段建议只支持有限模型范围：

- ONNX initializer 权重读取。
- Conv / Gemm / MatMul / Linear 中至少一种权重类型。
- INT8 per-tensor 权重量化。
- 生成 `int8_weight.bin` 和 `quant_params.json`。

M2-A 显式入口：

```bash
edgequant_tool --mode onnx-weight-export ...
```

默认 `auto` 模式仍保持 M1 行为：传入 `--model` 时只生成 unsupported report，避免既有 bridge 被突然切换到 Runtime 可消费路径。

M2 第一阶段可以暂不支持：

- 全图 activation calibration。
- per-channel 量化。
- 所有 ONNX 算子。
- Runtime 自动加载执行。

如果上述能力尚未完成，quant-tool 必须继续返回 `status=unsupported`，不得生成可被 Runtime 误消费的成功产物。
