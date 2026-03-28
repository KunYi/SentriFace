# SCRFD ONNX Runner Workflow

## 1. 目的

這份文件定義如何直接使用：

- `third_party/models/buffalo_sc/det_500m.onnx`

對單張圖或一批圖片做實際 detection，並把結果輸出成 CSV，接到目前專案既有的 replay / manifest workflow。

---

## 2. 工具

目前專案提供：

```bash
tools/run_scrfd_onnx.py
```

這個工具會：

1. 載入 `SCRFD` ONNX model
2. 對 image 或 image directory 做 inference
3. 以官方 `scrfd.py` 的 decode 規則輸出 detections
4. 產生後續可轉成 manifest 的 CSV

---

## 3. 依賴

建議使用專案內 `.venv`：

```bash
.venv/bin/pip install onnx onnxruntime pillow
```

目前專案內已驗證 `.venv` 可直接載入 `onnxruntime`。

若要看 C++ 版整合邊界，可再參考：

- `docs/platform/onnxruntime_cpp_integration.md`

---

## 4. 用法

對單張圖：

```bash
.venv/bin/python tools/run_scrfd_onnx.py \
  third_party/models/buffalo_sc/det_500m.onnx \
  path/to/image.jpg \
  artifacts/scrfd_output.csv
```

對資料夾：

```bash
.venv/bin/python tools/run_scrfd_onnx.py \
  third_party/models/buffalo_sc/det_500m.onnx \
  artifacts/rtsp_check/preview_1fps \
  artifacts/rtsp_check/scrfd_output.csv
```

---

## 5. 和既有流程的銜接

產出的 CSV 可直接接：

```bash
tools/convert_scrfd_csv_to_manifest.py \
  artifacts/rtsp_check/scrfd_output.csv \
  artifacts/rtsp_check/detection_manifest.txt \
  --frame-manifest artifacts/rtsp_check/offline_manifest.txt
```

再交給：

```bash
./build/offline_sequence_runner \
  artifacts/rtsp_check/offline_manifest.txt \
  artifacts/rtsp_check/detection_manifest.txt
```

---

## 6. 目前意義

這條 workflow 的價值是：

- 已經不只是在 codebase 內準備 `SCRFD` 骨架
- 而是能直接用官方 `det_500m.onnx` 做真實 detection
- 並把結果接回目前 C++ 系統的 tracker/replay 驗證線

若想直接從 RTSP artifacts 一路跑到 replay，可參考：

- `docs/detector/scrfd_offline_replay_end_to_end.md`
