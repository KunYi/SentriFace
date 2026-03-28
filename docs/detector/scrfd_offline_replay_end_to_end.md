# SCRFD Offline Replay End-To-End

## 1. 目的

這份文件定義從 RTSP 抽幀 artifacts 到 `offline_sequence_runner` 的一鍵化 `SCRFD` 驗證流程。

---

## 2. 工具

目前專案提供：

```bash
tools/run_scrfd_offline_replay.sh
```

它會自動完成：

1. `preview_1fps/` -> frame manifest
2. `det_500m.onnx` -> SCRFD CSV
3. CSV -> detection manifest
4. `offline_sequence_runner` replay

---

## 3. 前置條件

需先具備：

- `artifacts/rtsp_check/preview_1fps/`
- `.venv` 已安裝 `onnx / onnxruntime / pillow`
- `build/offline_sequence_runner` 已建置完成
- `third_party/models/buffalo_sc/det_500m.onnx` 已存在

---

## 4. 用法

若使用預設模型路徑：

```bash
tools/run_scrfd_offline_replay.sh artifacts/rtsp_check
```

若指定模型：

```bash
tools/run_scrfd_offline_replay.sh \
  artifacts/rtsp_check \
  third_party/models/buffalo_sc/det_500m.onnx
```

---

## 5. 產物

執行後會產生：

- `artifacts/rtsp_check/offline_manifest.txt`
- `artifacts/rtsp_check/scrfd_output.csv`
- `artifacts/rtsp_check/detection_manifest.txt`
- `build/offline_sequence_report.txt`

---

## 6. 意義

這條流程代表目前專案已具備：

- 官方 `SCRFD` ONNX model
- 真實 detection inference
- detection replay manifest
- C++ tracker replay 驗證

也就是說，`SCRFD` 已不再只是文件目標，而是已能接回目前主系統驗證線。
