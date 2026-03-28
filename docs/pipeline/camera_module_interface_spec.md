# Camera Module Interface Spec

## 1. 目的

這份文件定義第一版 `camera` 模組介面。

第一版先專注於：

- 統一 frame 資料格式
- 定義 frame source 抽象
- 支援未來接：
  - RTSP
  - 離線影片
  - 單張影像
  - 板端 camera

暫時不直接綁定 `ffmpeg` 或板端 SDK。

---

## 2. 介面定位

目前模組位置：

```text
include/camera/frame_source.hpp
src/camera/frame_source.cpp
```

主要類型：

- `PixelFormat`
- `Frame`
- `FrameSourceInfo`
- `IFrameSource`
- `SequenceFrameSource`

---

## 3. 第一版設計

`Frame` 先固定以下最基本欄位：

- width
- height
- channels
- pixel_format
- timestamp_ms
- raw data buffer

`IFrameSource` 定義統一介面：

```cpp
class IFrameSource {
 public:
  virtual ~IFrameSource() = default;

  virtual bool Open() = 0;
  virtual void Close() = 0;
  virtual bool IsOpen() const = 0;
  virtual FrameSourceInfo GetInfo() const = 0;
  virtual bool Read(Frame& frame) = 0;
};
```

### 3.1 第一版內建來源

第一版提供：

- `SequenceFrameSource`

用途：

- 單元測試
- 離線資料流模擬
- 不依賴 RTSP/ffmpeg 先驗證 pipeline glue

---

## 4. RTSP 與板端遷移

這個抽象層的目的就是讓後面可以新增：

- `RtspFrameSource`
- `FfmpegFrameSource`
- `Rv1106CameraSource`

而不需要重寫：

- pipeline glue
- tracker integration
- 測試入口

因此第一版雖然很輕，但它是後續 RTSP milestone 與板端整合的重要接口。

### 4.1 後續 RTSP/ffmpeg 方向

未來若要補正式 RTSP source，建議方向是：

- `FfmpegFrameSource`
- 或 `RtspFrameSource`

其詳細前提可參考：

- `docs/pipeline/ffmpeg_rtsp_frame_source_spec.md`
