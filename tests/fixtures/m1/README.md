# M1 Smoke Fixture

This fixture is intentionally tiny and exists only for M1 interface smoke tests.

- `model_stub.onnx` is a non-empty placeholder file. M1 only checks that the model path exists and records its size; it does not parse ONNX.
- `calibration/calibration_manifest.json` is a minimal manifest for path-existence checks.
- `calibration/samples_npy/*.npy` are placeholder sample files. M1 only counts files with the `.npy` extension.

Do not use this fixture as real calibration data.
