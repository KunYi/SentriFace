# Detector Module Interface Spec

## 1. 目的

這份文件定義第一版 `detector` 模組介面。

第一版先專注於：

- detection 輸入輸出規格
- detector 抽象介面
- mock / sequence backend

暫時不綁定任何特定模型 runtime。

---

## 2. 介面定位

目前模組位置：

```text
include/detector/face_detector.hpp
src/detector/face_detector.cpp
```

主要類型：

- `DetectionConfig`
- `DetectionBatch`
- `DetectorInfo`
- `IFaceDetector`
- `SequenceFaceDetector`
- `ManifestFaceDetector`
- `ScrfdFaceDetector`

`DetectionConfig` 目前除了基本輸入尺寸與 threshold 外，也承載第一版門禁導向的排序/裁剪策略，例如：

- `min_face_width`
- `min_face_height`
- `prefer_larger_faces`
- `max_output_detections`
- `enable_roi`
- `roi`

---

## 3. 第一版設計

`IFaceDetector` 定義統一入口：

```cpp
class IFaceDetector {
 public:
  virtual ~IFaceDetector() = default;

  virtual bool ValidateInputFrame(const sentriface::camera::Frame& frame) const = 0;
  virtual DetectorInfo GetInfo() const = 0;
  virtual DetectionBatch Detect(const sentriface::camera::Frame& frame) = 0;
};
```

### 3.1 第一版內建 backend

目前提供：

- `SequenceFaceDetector`
- `ManifestFaceDetector`
- `ScrfdFaceDetector`
- `IScrfdRuntime`

用途：

- 單元測試
- 離線資料流模擬
- 離線 detection replay
- 正式 `SCRFD` backend 的固定整合落點
- 在沒有真實 `SCRFD-500M` backend 之前，先把 pipeline I/O 固定下來

### 3.2 Detection 型別

輸出直接沿用：

- `tracker::Detection`

這樣能減少 detector 和 tracker 之間的格式轉換成本。

### 3.3 門禁導向過濾與排序

第一版 detector 不應只做「score 過門檻就全部往下送」，而應先反映門禁場景特性：

- 過小臉框先淘汰
- 更大、更近的人臉優先
- 必要時只保留前幾個候選
- 若安裝場景已知，可只接受落在有效區域內的人臉

也就是說，detector 的責任除了偵測本身，還包含一層輕量的 access-control pre-filter：

- `min_face_width / min_face_height`
- `prefer_larger_faces = true`
- `max_output_detections`
- `ROI center-point filtering`

這樣可以減少：

- 背景小頭像干擾
- 遠處無效人臉進入 tracker
- 下游 embedding / search 的無效負擔

### 3.4 Manifest replay backend

`ManifestFaceDetector` 主要用於：

- 將外部先跑出的 detection 結果回放到主程式
- 驗證 `offline_sequence_runner -> tracker` 路徑

第一版採用 text manifest，並以 timestamp 精確對齊 frame。

對應規格文件：

- `docs/pipeline/offline_detection_replay_spec.md`

### 3.5 SCRFD backend skeleton

`ScrfdFaceDetector` 現在已經作為正式 backend skeleton 納入程式碼。

目前它先固定：

- `ScrfdDetectorConfig`
- `ScrfdRuntime`
- `ScrfdTensorView`
- `ScrfdInferenceOutputs`
- CPU / RKNN 兩種 runtime 方向
- detector config 與 model path 驗證

第一版仍是 integration stub，尚未接真實推論 runtime；但它的價值是先把：

- 系統內部型別
- 初始化責任
- runtime 分流
- backend tensor 到 head view 的映射責任
- 未來 CPU/RKNN backend 的對外介面

固定下來。

---

## 4. CPU-First 與 SCRFD 主線

雖然目前 detector 模組先提供的是 mock backend，  
但本專案主線仍然是：

- `SCRFD-500M`

因此未來實作正式 detector backend 時，應優先補：

- `ScrfdFaceDetector`

而不是重新設計整套 detector 介面。
