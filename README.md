# EdgeAIquant_toolkit

边缘端 AI 模型量化工具链工程化脚手架。

当前优先目标不是完整 ONNX 量化器，而是先固定清晰、可验证、不会误报成功的工程接口，为后续 LiteEdgeINT C++ Runtime 和 gem5 链路做准备。

## 核心特性

- 工程化构建：使用 CMake Presets，支持 Debug / Release 构建。
- 旧 demo 兼容：保留 `--size` / `--input` / `--output` 文本 float 张量模式。
- M1 工程接口：新增 `--model` / `--calibration` / `--output-dir` / `--bit-width` / `--platform` / `--config` / `--mode`。
- ONNX report 模式：当前不伪装支持真实 ONNX 量化，只生成中文 `quant_report.json`。
- 硬件适配预留：保留 Ascend mock 适配层。

## Ubuntu 22.04 环境准备

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build
```

## 构建

```bash
bash build.sh
```

构建产物：

```text
output/bin/edgequant_tool
```

## M1 一键验收

```bash
bash scripts/smoke_m1.sh
```

该脚本会自动完成：

- 构建项目。
- 检查 `--help` 中是否包含 M1 新增 CLI。
- 验证旧 `tensor-demo` 模式仍可输出结果。
- 使用 repo 内置 fixture 运行 `onnx-report`。
- 检查 `quant_report.json` 的关键字段。
- 验证错误模型路径返回非 0。

成功时会看到：

```text
[M1][PASS] smoke test completed
```

M1 smoke 产物默认写入：

```text
output/m1_smoke/tensor_result.txt
output/m1_smoke/quant_report.json
```

## 运行示例

旧 tensor demo：

```bash
./output/bin/edgequant_tool --size 20
```

ONNX report 模式：

```bash
./output/bin/edgequant_tool \
  --model tests/fixtures/m1/model_stub.onnx \
  --calibration tests/fixtures/m1/calibration \
  --output-dir output/m1_smoke \
  --bit-width 8 \
  --platform cpu
```

当前 report 模式只做输入检查和报告生成，不解析 ONNX，不生成真实 INT8 权重。

## 项目结构

```text
src/                    量化核心算法与 CLI 入口
include/edgequant/       公共头文件
config/                  默认配置
third_party/ascend_mock/  Ascend mock 头文件和占位库
tests/fixtures/m1/       M1 smoke 测试夹具
scripts/smoke_m1.sh      M1 一键验收脚本
```

## M1 基线

已通过 Ubuntu 22.04 smoke 验收的 M1 基线 tag：

```text
v0.1.0-m1
```

M1 仍未实现：

- 真实 ONNX parser。
- 真实 INT8 权重生成。
- activation calibration 统计。
- C++ Runtime 对接。
