# SCRFD Detector Backend Spec

## 1. 目的

這份文件定義未來 `SCRFD-500M` detector backend 的實作前提。

它的定位是：

- 對接目前已建立的 `detector::IFaceDetector`
- 保持 `SCRFD-500M` 為主線
- 允許先在 CPU-first 架構下完成整體資料流驗證
- 之後再落到 `RKNN / RV1106`

---

## 2. 對應介面

未來正式 backend 建議命名為：

- `ScrfdFaceDetector`

目前專案中已建立同名 backend skeleton，位置如下：

```text
include/detector/face_detector.hpp
src/detector/face_detector.cpp
```

目前已確認的一個官方模型落點是：

- `third_party/models/buffalo_sc/det_500m.onnx`

它應實作：

```cpp
class IFaceDetector {
 public:
  virtual ~IFaceDetector() = default;
  virtual bool ValidateInputFrame(const sentriface::camera::Frame& frame) const = 0;
  virtual DetectorInfo GetInfo() const = 0;
  virtual DetectionBatch Detect(const sentriface::camera::Frame& frame) = 0;
};
```

---

## 3. 輸入輸出要求

### 3.1 輸入

輸入應來自：

- `camera::Frame`

第一版建議維持 detector 輸入尺寸可配置，但預設仍應落在：

- `640 x 640 x 3`

或其他經實測後更合適的 SCRFD 輸入尺寸。

### 3.2 輸出

輸出應直接轉成：

- `tracker::Detection`

欄位至少包含：

- bbox
- score
- 5 landmarks
- timestamp

### 3.3 第一版 config 邊界

目前 `ScrfdFaceDetector` 已固定第一版配置概念：

- `ScrfdRuntime`
- `model_path`
- `DetectionConfig`
- `nms_threshold`
- `max_detections`
- `enable_landmarks`
- `feature_strides`
- `anchors_per_location`

這些欄位不代表最終 runtime 已完成，但代表後續 CPU 與 RKNN backend 應共用這組高層配置語意。

### 3.4 已固定的 stage-1 data path

目前 `ScrfdFaceDetector` 也已固定兩個關鍵步驟的 code-level contract：

- `PrepareInputTensor(frame, tensor)`
- `DecodeHeadCandidates(head, input_width, input_height, stride)`
- `DecodeMultiLevelCandidates(heads, input_width, input_height)`
- `PostprocessCandidates(candidates, timestamp_ms)`
- `IScrfdRuntime::Run(input, candidates)`

這代表後續不管接：

- CPU runtime
- RKNN runtime

都應遵守相同的高層資料流：

1. `camera::Frame`
2. preprocess 成 SCRFD input tensor
3. runtime inference
4. `score / bbox / landmark` 多個 head 輸出
5. decode 成 raw candidates
6. score filter + NMS + top-k
7. `tracker::Detection`

目前 runtime 本體仍是 stub，但 preprocess / decode / postprocess 的介面責任已先固定。
目前 CPU `onnxruntime` 路徑也已具備基本 smoke backend，且 preprocess 已往 Python runner 對齊。
目前 `ScrfdFaceDetector` 主線也已內建：

- resize
- letterbox padding
- detection 座標回投影到原始影像

因此 detector 已可直接處理：

- `640x640`
- `1280x720`
- 其他 3-channel 輸入尺寸

而不再要求上游一定先手動轉成模型輸入大小。

### 3.5 Runtime adapter 邊界

目前也已把 runtime 層抽成：

- `IScrfdRuntime`

其責任是：

1. 接收已完成 preprocess 的 `ScrfdInputTensor`
2. 執行特定 backend inference
3. 輸出 `ScrfdRawCandidate`

這讓後續整合：

- CPU runtime
- ONNX Runtime
- RKNN runtime

時，不需要再改 `ScrfdFaceDetector` 的高層資料流。

目前也已補上兩個 runtime adapter 型別：

- `ScrfdTensorView`
- `ScrfdInferenceOutputs`

以及兩個對應 helper：

- `ValidateScrfdInferenceOutputs(...)`
- `BuildScrfdHeadViews(...)`

這代表未來 backend 只需要負責：

1. 取得各 level 的 score / bbox / landmark tensor
2. 填入 `ScrfdInferenceOutputs`
3. 建立 `ScrfdHeadOutputView`
4. 交給既有 decode / postprocess

而不需要每個 backend 各自重寫 head layout 驗證邏輯。

### 3.6 已完成的 C++ decode 邊界

目前 `ScrfdFaceDetector` 已具備：

- single-head decode
- multi-level head aggregation
- 從多個 feature strides 匯總 raw candidates

這表示後續正式 runtime 需要補上的重點已經更收斂為：

- 真實 backend inference
- 將 backend tensors 映射成 `ScrfdHeadOutputView`
- 必要時補齊 CPU / RKNN 各自的 tensor layout adapter

而不是重新設計 SCRFD 的 decode 流程本身。

---

## 3.7 已驗證的 head 結構

目前已實際檢查：

- `third_party/models/buffalo_sc/det_500m.onnx`

並確認其 head 結構符合：

- 3 scales
- score / box / landmark 三分支
- `feature_strides = {8, 16, 32}`
- `anchors_per_location = 2`

對應驗證文件：

- `docs/detector/scrfd_model_io_verified.md`

若要直接用目前已下載的 ONNX model 做實際 detection，可參考：

- `docs/detector/scrfd_onnx_runner_workflow.md`

---

## 3.6 目前優先整合模型

基於已實測下載與 package inspection，當前最建議的第一個正式整合模型是：

- `buffalo_sc.zip` 內的 `det_500m.onnx`

理由：

- 來源已驗證為官方 release asset
- 檔名清楚對應 detector
- package 體積較 `buffalo_s.zip` 小，較適合先做系統整合與流程驗證

---

## 4. 板端導向原則

雖然目前架構允許先在 CPU 上做資料流驗證，但 `ScrfdFaceDetector` 的設計仍必須以：

- `RV1106G3 + RKNN`

為主。

因此正式 backend 實作時要特別注意：

- model input size
- NPU 使用率
- latency
- landmarks 穩定性
- 與 embedding 推論的時間分配

---

## 5. 建議實作順序

1. 保持目前 `SequenceFaceDetector` 作為 mock backend
2. 使用 `ManifestFaceDetector` 驗證外部 `SCRFD` 輸出是否能穩定導入 tracker
3. 在文件與介面上先把 `SCRFD` 路徑固定
4. 等 RTSP / 真實影像樣本更完整後，再實作正式 `ScrfdFaceDetector`
5. 實作後，優先驗證：
   - detection 數量
   - bbox 穩定性
   - landmarks 穩定性
   - 是否壓縮到 tracker/embedding 的可用算力

在進入真實 runtime 前，先維持目前的 stub backend 有兩個目的：

- 不讓 `SCRFD` 整合點繼續只停留在文件
- 避免未來接 CPU 版與 RKNN 版時，重複改動 detector 抽象

---

## 6. Offline Export Adapter

在正式 `ScrfdFaceDetector` 尚未落地前，允許先採用 desktop/CPU 版本的 `SCRFD` 做離線推論，再把結果轉成：

- `docs/pipeline/offline_detection_replay_spec.md`

定義的 text manifest。

建議對應關係如下：

- `SCRFD bbox` -> `<x> <y> <w> <h>`
- `SCRFD score` -> `<score>`
- `SCRFD 5 landmarks` -> `<l0x> <l0y> ... <l4x> <l4y>`
- frame time 或 frame index -> `<timestamp_ms>`

若桌面推論結果先經過 CSV 中繼格式，可直接搭配：

- `tools/convert_scrfd_csv_to_manifest.py`
- `docs/detector/scrfd_offline_export_workflow.md`

這樣可以先完成：

- RTSP 抽幀
- 桌面 `SCRFD` 推論
- detection replay
- tracker 回放驗證

等到這條線穩定後，再把同一組 I/O 行為收斂到真正的 `ScrfdFaceDetector` backend。
