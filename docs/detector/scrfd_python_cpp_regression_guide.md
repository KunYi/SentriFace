# SCRFD Python C++ Regression Guide

## 1. 目的

這份文件定義如何對同一張真圖，同時執行：

- Python `onnxruntime`
- C++ `ScrfdFaceDetector + onnxruntime`

並比較：

- detection 數量
- `max_score`

---

## 2. 工具

目前專案提供：

- `build_ort/scrfd_ppm_runner`
- `tools/run_scrfd_cpp_regression.sh`

其中：

- Python 路徑直接吃原始圖片
- C++ 路徑目前先透過 `PPM` 作為最小圖片輸入格式
- regression script 目前會先把輸入圖轉成 `640x640` padded `PPM`

---

## 3. 前提

執行前需要：

1. 已建好 `build_ort`
2. `SENTRIFACE_ENABLE_ONNXRUNTIME=ON`
3. 已有：
   - `third_party/models/buffalo_sc/det_500m.onnx`
   - `.venv`

---

## 4. 用法

```bash
tools/run_scrfd_cpp_regression.sh <input_image> <output_dir> [score_threshold] [max_num]
```

範例：

```bash
tools/run_scrfd_cpp_regression.sh \
  artifacts/webcam_smoke/webcam_capture.jpg \
  artifacts/scrfd_regression \
  0.02 \
  5
```

---

## 5. 輸出

它會輸出：

- `input.ppm`
- `python_scrfd.csv`
- `cpp_scrfd.csv`
- `regression_summary.txt`

---

## 6. 判讀方式

第一版先看：

- `python_detections`
- `cpp_detections`
- `python_max_score`
- `cpp_max_score`
- `max_score_delta`
- `avg_iou`
- `avg_score_delta`
- `avg_landmark_distance`

這份 regression 的目的不是要求兩邊一開始就完全一致，而是：

- 確認 C++ 路徑已接近 Python 路徑
- 發現 preprocess / decode / NMS 的偏差是否仍過大

目前第一版特別要注意：

- regression script 會先把輸入圖整理成同一張 `640x640` padded `PPM`
- Python 與 C++ 都以這張圖作為共同輸入基準
- 這樣 bbox 與 landmarks 的比較才有意義

目前這份 regression 的價值也包含驗證：

- `ScrfdFaceDetector` 主線內建的 resize + letterbox 邏輯
- 與 Python runner 的共同基準是否仍然對齊

目前已在本機 webcam 圖上觀察到一組更進一步的共同前處理基準結果：

- `python_detections=5`
- `cpp_detections=5`
- `python_max_score=0.676480`
- `cpp_max_score=0.676480`
- `max_score_delta=0.000000`
- `matched_pairs=5`
- `avg_iou=0.999974`
- `avg_score_delta=0.000000`
- `avg_landmark_distance=0.000397`

這表示：

- C++ `onnxruntime` 路徑已不再是只有 smoke compile
- 在共同 `640x640` padded 輸入下，C++ 與 Python runner 已幾乎完全對齊
- 後續主要工作重心可轉向把相同 resize/pad 邏輯內建到 detector 主線

---

## 7. 目前定位

這條 regression 線是下一階段 detector 收斂的重要工具，特別適合：

- preprocess 對齊後
- 開始驗證 C++ backend 與 Python runner 是否一致

它不直接取代板端驗證，但能大幅降低把問題帶上 RV1106 才開始除錯的風險。
