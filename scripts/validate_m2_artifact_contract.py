#!/usr/bin/env python3
import argparse
import json
import math
import sys
from pathlib import Path


def fail(message: str) -> None:
    print(f"[M2][FAIL] {message}", file=sys.stderr)
    raise SystemExit(1)


def load_json(path: Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as handle:
            value = json.load(handle)
    except FileNotFoundError:
        fail(f"Missing required file: {path}")
    except json.JSONDecodeError as exc:
        fail(f"Invalid JSON in {path}: {exc}")
    if not isinstance(value, dict):
        fail(f"JSON root must be an object: {path}")
    return value


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def require_non_empty_string(value: object, field: str) -> str:
    require(isinstance(value, str) and bool(value), f"{field} must be a non-empty string")
    return value


def require_int(value: object, field: str, minimum: int | None = None) -> int:
    require(isinstance(value, int) and not isinstance(value, bool), f"{field} must be an integer")
    if minimum is not None:
        require(value >= minimum, f"{field} must be >= {minimum}")
    return value


def require_positive_number(value: object, field: str) -> float:
    require(isinstance(value, (int, float)) and not isinstance(value, bool), f"{field} must be a number")
    value_float = float(value)
    require(math.isfinite(value_float) and value_float > 0.0, f"{field} must be > 0")
    return value_float


def validate_report(report: dict) -> None:
    require(report.get("status") == "success", 'quant_report.json status must be "success"')
    require(report.get("onnx_supported") is True, "quant_report.json onnx_supported must be true")
    require(report.get("calibration_supported") is True, "quant_report.json calibration_supported must be true")
    require_non_empty_string(report.get("model_path"), "quant_report.json model_path")
    require_non_empty_string(report.get("output_dir"), "quant_report.json output_dir")

    artifacts = report.get("artifacts")
    require(isinstance(artifacts, dict), "quant_report.json artifacts must be an object")
    require(artifacts.get("int8_weight") == "int8_weight.bin", "artifacts.int8_weight must be int8_weight.bin")
    require(artifacts.get("quant_params") == "quant_params.json", "artifacts.quant_params must be quant_params.json")


def validate_tensor(tensor: object, index: int, weight_size: int) -> tuple[int, int]:
    prefix = f"quant_params.json tensors[{index}]"
    require(isinstance(tensor, dict), f"{prefix} must be an object")

    require_non_empty_string(tensor.get("name"), f"{prefix}.name")
    require_non_empty_string(tensor.get("source_node"), f"{prefix}.source_node")
    require(tensor.get("role") in {"weight", "activation"}, f"{prefix}.role must be weight or activation")
    require(tensor.get("data_type") == "int8", f"{prefix}.data_type must be int8")

    shape = tensor.get("shape")
    require(isinstance(shape, list) and bool(shape), f"{prefix}.shape must be a non-empty array")
    for dim_index, dim in enumerate(shape):
        require_int(dim, f"{prefix}.shape[{dim_index}]", minimum=1)

    require_positive_number(tensor.get("scale"), f"{prefix}.scale")
    require_int(tensor.get("zero_point"), f"{prefix}.zero_point")

    quantization = tensor.get("quantization")
    require(quantization in {"per_tensor", "per_channel"}, f"{prefix}.quantization must be per_tensor or per_channel")
    quant_axis = tensor.get("quantization_axis")
    if quantization == "per_tensor":
        require(quant_axis is None, f"{prefix}.quantization_axis must be null for per_tensor")
    else:
        require_int(quant_axis, f"{prefix}.quantization_axis", minimum=0)

    offset = require_int(tensor.get("offset_bytes"), f"{prefix}.offset_bytes", minimum=0)
    size = require_int(tensor.get("size_bytes"), f"{prefix}.size_bytes", minimum=1)
    require(offset + size <= weight_size, f"{prefix} byte range exceeds int8_weight.bin size")
    return offset, size


def validate_params(params: dict, weight_size: int) -> None:
    require(params.get("schema_version") == 1, "quant_params.json schema_version must be 1")
    require(params.get("tool_name") == "edgequant_tool", "quant_params.json tool_name must be edgequant_tool")
    require(params.get("bit_width") == 8, "quant_params.json bit_width must be 8")
    require(params.get("byte_order") == "little_endian", "quant_params.json byte_order must be little_endian")
    require(params.get("weight_file") == "int8_weight.bin", "quant_params.json weight_file must be int8_weight.bin")

    tensors = params.get("tensors")
    require(isinstance(tensors, list) and bool(tensors), "quant_params.json tensors must be a non-empty array")

    ranges = [validate_tensor(tensor, index, weight_size) for index, tensor in enumerate(tensors)]
    ranges.sort()
    for previous, current in zip(ranges, ranges[1:]):
        previous_end = previous[0] + previous[1]
        require(previous_end <= current[0], "tensor byte ranges must not overlap")


def validate_artifact_dir(artifact_dir: Path) -> None:
    require(artifact_dir.is_dir(), f"Artifact directory does not exist: {artifact_dir}")

    report_path = artifact_dir / "quant_report.json"
    params_path = artifact_dir / "quant_params.json"
    weight_path = artifact_dir / "int8_weight.bin"

    report = load_json(report_path)
    params = load_json(params_path)

    require(weight_path.is_file(), f"Missing required file: {weight_path}")
    weight_size = weight_path.stat().st_size
    require(weight_size > 0, "int8_weight.bin must be non-empty")

    validate_report(report)
    validate_params(params, weight_size)

    print("[M2][PASS] artifact contract validated")
    print(f"[M2] Artifact directory: {artifact_dir}")
    print(f"[M2] int8_weight.bin bytes: {weight_size}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate EdgeQuant M2 artifact contract.")
    parser.add_argument("artifact_dir", type=Path, help="Directory containing M2 artifacts")
    args = parser.parse_args()
    validate_artifact_dir(args.artifact_dir)


if __name__ == "__main__":
    main()
