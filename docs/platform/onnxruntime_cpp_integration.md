# ONNX Runtime C++ Integration

## 1. 現況

目前專案有兩條不同層級的 `ONNX Runtime` 路徑：

- Python `onnxruntime`
- C++ `onnxruntime`

這兩條路的狀態不同：

- Python 路徑：**現在已可實跑**
- C++ 路徑：**目前已可透過本機 ONNX Runtime C/C++ package 建置與執行 smoke path**

也就是說，若問題是：

- 「現在能不能真的用 `onnxruntime` 跑 `det_500m.onnx`？」

答案是：

- **可以，現在就能，用 Python runner**

目前本專案已實際下載並驗證：

- `third_party/onnxruntime/onnxruntime-linux-x64-1.24.4`

且已成功用下列方式 configure/build：

```bash
cmake -S . -B build_ort \
  -DSENTRIFACE_ENABLE_ONNXRUNTIME=ON \
  -DSENTRIFACE_ONNXRUNTIME_ROOT=third_party/onnxruntime/onnxruntime-linux-x64-1.24.4

cmake --build build_ort
ctest --test-dir build_ort --output-on-failure
```

若問題是：

- 「C++ `ScrfdFaceDetector` 什麼時候能直接用 `onnxruntime` 跑？」

答案是：

- **現在已經能載入 ONNX Runtime C/C++ package，並執行 C++ smoke path**

---

## 2. 已可實跑的路徑

目前已驗證可用：

```bash
.venv/bin/python tools/run_scrfd_onnx.py \
  third_party/models/buffalo_sc/det_500m.onnx \
  <image_dir> \
  build/scrfd_validation.csv \
  --score-threshold 0.02
```

這條路徑會：

1. 使用 Python `onnxruntime`
2. 載入官方 `det_500m.onnx`
3. 執行真實 inference
4. decode 成 detection CSV

目前這已經不是 mock，而是真正跑到 ONNX model。

---

## 3. 為什麼 C++ 版還沒直接打開

雖然 `ScrfdFaceDetector` 的：

- preprocess
- head decode
- multi-level aggregation
- postprocess

都已經就位，現在也已補上：

- C++ runtime binary dependency
- `Ort::Session` 建立
- output tensor 抽取
- tensor 分類成 `ScrfdInferenceOutputs`

因此目前差的已經不是「能不能跑 ONNX Runtime」，而是：

- preprocess 是否要完全對齊 Python 版
- 是否要再補更正式的真圖 regression / benchmark
- 後續 RKNN 對接

---

## 4. 專案已準備好的 CMake 邊界

目前 `CMakeLists.txt` 已加入：

- `SENTRIFACE_ENABLE_ONNXRUNTIME`
- `SENTRIFACE_ONNXRUNTIME_ROOT`

使用方式：

```bash
cmake -S . -B build \
  -DSENTRIFACE_ENABLE_ONNXRUNTIME=ON \
  -DSENTRIFACE_ONNXRUNTIME_ROOT=/path/to/onnxruntime
```

其中 `SENTRIFACE_ONNXRUNTIME_ROOT` 應包含：

- `include/onnxruntime_cxx_api.h`
- `lib/libonnxruntime.so`

或對應平台的等價檔案。

---

## 5. 接下來真正要補的 C++ backend 工作

目前 C++ backend 已能：

1. 建立 `Ort::Env`
2. 建立 `Ort::Session`
3. 對 `ScrfdInputTensor` 執行 inference
4. 讀出 9 個 outputs
5. 轉成 `ScrfdInferenceOutputs`
6. 走既有 decode / postprocess

另外目前 `PrepareInputTensor(...)` 也已往 Python runner 對齊，包含：

- `CHW`
- 預設輸入通道期望為 `BGR`
- `(value - 127.5) / 128.0`
- resize + letterbox padding
- detection 座標映回原始輸入影像

接下來應優先補的是：

1. 真實圖片 regression
2. latency / throughput 測量
3. 把同樣的高層資料流遷移到 RKNN backend

目前專案已經把第 5 到第 7 步前面的資料邊界先固定好，所以真正要寫的範圍已經比一開始小很多。

---

## 6. 建議結論

短期內建議這樣分工：

- Python `onnxruntime` 仍可作為最快的驗證工具
- C++ `onnxruntime` 現在已可作為正式 smoke backend
- 接下來應把重心放在數值對齊與真實資料驗證，而不是再停留在 runtime 整合層

這樣不會卡住主線，也不會讓 C++ 整合方向漂移。
