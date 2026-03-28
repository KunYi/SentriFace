# FFmpeg RTSP Frame Source Spec

## 1. 目的

這份文件定義未來 `FfmpegFrameSource / RtspFrameSource` 的設計前提。

它不代表目前已完成 C++ RTSP frame source 實作，而是先把：

- 輸入來源
- 輸出 frame 型別
- artifact workflow
- 與 `ffmpeg` 驗證工具的關係

固定下來。

---

## 2. 目前狀態

目前專案已有：

- `camera::IFrameSource`
- `camera::SequenceFrameSource`
- `tools/ffmpeg_rtsp_smoke.sh`

也就是：

- 抽象介面已存在
- RTSP 驗證工具已存在
- 但正式的 `FfmpegFrameSource` 尚未實作

---

## 3. 未來 `FfmpegFrameSource` 的角色

未來的 `FfmpegFrameSource` 應負責：

1. 開啟 RTSP URL
2. 解碼輸出為統一的 `camera::Frame`
3. 提供 `timestamp_ms`
4. 回報 stream metadata

它應實作：

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

---

## 4. 設計原則

### 4.1 先與目前 `Frame` 型別對齊

不要為了 RTSP 另建一套 frame 格式。  
應直接輸出到：

- `camera::Frame`

### 4.2 CPU-first 驗證，但不偏離板端目標

即使 `ffmpeg` 實作可能先跑在桌面 CPU，設計仍應考慮：

- 未來接板端資料來源
- 與 `RV1106 + RKNN` 資料流一致

### 4.3 不把驗證工具和正式 frame source 混在一起

`tools/ffmpeg_rtsp_smoke.sh` 的定位是：

- 外部驗證與資料採集工具

未來的 `FfmpegFrameSource` 定位是：

- 程式內部的 frame source 實作

兩者相關，但不應互相取代。

---

## 5. 與 artifact workflow 的關係

目前 `tools/ffmpeg_rtsp_smoke.sh` 會輸出：

- `stream_info.txt`
- `preview.jpg`
- `preview_1fps/`
- `clip.mp4`
- `artifact_manifest.txt`

這些 artifact 的目的，是在正式 C++ RTSP source 完成前，先讓開發者/測試者可以：

- 驗證 RTSP 串流
- 保存樣本
- 建立 regression 素材

---

## 6. 建議後續實作順序

1. 先維持目前 shell-based RTSP validation
2. 再決定是否需要正式 `FfmpegFrameSource`
3. 若需要，再把它接到：
   - sample runner
   - offline/rtsp pipeline runner

這樣會比一開始就把 RTSP 直接塞進主應用更穩定。
