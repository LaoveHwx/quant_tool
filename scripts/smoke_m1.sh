#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

BIN="${ROOT_DIR}/output/bin/edgequant_tool"
FIXTURE_DIR="${ROOT_DIR}/tests/fixtures/m1"
MODEL="${FIXTURE_DIR}/model_stub.onnx"
CALIBRATION="${FIXTURE_DIR}/calibration"
SMOKE_DIR="${ROOT_DIR}/output/m1_smoke"
TENSOR_OUTPUT="${SMOKE_DIR}/tensor_result.txt"
REPORT="${SMOKE_DIR}/quant_report.json"
BAD_PATH_DIR="${SMOKE_DIR}/bad_path"

fail() {
    echo "[M1][FAIL] $*" >&2
    exit 1
}

require_file() {
    local path="$1"
    [[ -s "${path}" ]] || fail "Expected non-empty file: ${path}"
}

require_text() {
    local haystack="$1"
    local needle="$2"
    [[ "${haystack}" == *"${needle}"* ]] || fail "Expected text not found: ${needle}"
}

require_report_field() {
    local needle="$1"
    grep -Fq "${needle}" "${REPORT}" || fail "Report missing field: ${needle}"
}

echo "[M1] Build project"
bash build.sh

[[ -x "${BIN}" ]] || fail "Executable not found: ${BIN}"
mkdir -p "${SMOKE_DIR}" "${BAD_PATH_DIR}"

echo "[M1] Check --help exposes engineering CLI"
HELP_OUTPUT="$("${BIN}" --help)"
for opt in --model --calibration --output-dir --bit-width --platform --config --mode; do
    require_text "${HELP_OUTPUT}" "${opt}"
done

echo "[M1] Run tensor-demo compatibility path"
"${BIN}" --size 10 --output "${TENSOR_OUTPUT}"
require_file "${TENSOR_OUTPUT}"

echo "[M1] Run ONNX report path against repo fixture"
"${BIN}" \
    --model "${MODEL}" \
    --calibration "${CALIBRATION}" \
    --output-dir "${SMOKE_DIR}" \
    --bit-width 8 \
    --platform cpu

require_file "${REPORT}"
require_report_field '"report_language": "zh-CN"'
require_report_field '"status": "unsupported"'
require_report_field '"onnx_supported": false'
require_report_field '"calibration_supported": false'
require_report_field '"model_exists": true'
require_report_field '"calibration_exists": true'
require_report_field '"calibration_manifest_exists": true'
require_report_field '"calibration_sample_count": 3'
require_report_field '"bit_width": 8'
require_report_field '"platform": "cpu"'

echo "[M1] Check bad model path returns non-zero"
set +e
"${BIN}" \
    --model "${FIXTURE_DIR}/missing_model.onnx" \
    --calibration "${CALIBRATION}" \
    --output-dir "${BAD_PATH_DIR}" \
    >"${BAD_PATH_DIR}/stdout.log" \
    2>"${BAD_PATH_DIR}/stderr.log"
BAD_PATH_RC=$?
set -e

[[ "${BAD_PATH_RC}" -ne 0 ]] || fail "Bad model path unexpectedly returned success"
grep -Fq "模型文件不存在" "${BAD_PATH_DIR}/stderr.log" \
    || fail "Bad model path did not print the expected Chinese error"

echo "[M1][PASS] smoke test completed"
echo "[M1] Tensor output: ${TENSOR_OUTPUT}"
echo "[M1] Report output: ${REPORT}"
