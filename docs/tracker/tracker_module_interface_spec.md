# Tracker Module Interface Spec

## 1. 目的

這份文件定義第一版 `tracker` 模組的程式介面，對應：

- 硬體基線：`docs/platform/rv1106_system_hardware_baseline.md`
- Tracking 規格：`docs/platform/rv1106_kf_tracking_spec.md`

本模組定位為：

- 以 `C++17` 實作
- 提供清楚、可測試、可擴充的 `tracker` 介面
- 專注於 face track lifecycle 與 KF-based association

---

## 2. 目錄規劃

```text
include/tracker/
  types.hpp
  detection.hpp
  face_quality.hpp
  track.hpp
  track_snapshot.hpp
  tracker_config.hpp
  kalman_filter.hpp
  association.hpp
  tracker.hpp

src/tracker/
  kalman_filter.cpp
  association.cpp
  tracker.cpp
```

---

## 3. 模組責任

### `types.hpp`

共用基礎型別：

- `Point2f`
- `RectF`
- `Landmark5`
- `TrackState`

### `detection.hpp`

定義輸入 detection：

- bbox
- score
- landmarks
- timestamp

### `track.hpp`

定義 `Track` runtime 狀態：

- KF 狀態
- lifecycle
- metadata
- identity cache

### `face_quality.hpp`

定義第一版 face quality evaluator。

第一版先不依賴整張影像，只使用 tracker 已有的訊號：

- detection score
- bbox width
- bbox height

### `track_snapshot.hpp`

定義提供給下游模組使用的唯讀 snapshot：

- track id
- state
- bbox / landmarks
- 基本 lifecycle 狀態
- detection / quality metadata
- 最近一次 embedding 觸發時間

### `tracker_config.hpp`

定義所有 tracking 參數。

其中 `max_tracks` 屬於系統邊界參數，不只是效能調校值。  
對本專案第一版，建議預設為 `5`。

### `kalman_filter.hpp`

提供固定 8-state / 4-measurement 的 KF 操作。

### `association.hpp`

提供 gating 與 cost-based assignment。

### `tracker.hpp`

提供上層唯一主要入口：

- `Tracker::Step`
- `Tracker::Reset`
- `Tracker::GetTracks`
- `Tracker::GetTrackSnapshots`
- `Tracker::GetConfirmedTracks`
- `Tracker::GetEmbeddingCandidates`
- `Tracker::GetEmbeddingTriggerCandidates`
- `Tracker::MarkEmbeddingTriggered`

---

## 4. API 原則

### 4.1 上層只直接接 `Tracker`

上層模組不應直接操作：

- `KalmanFilter`
- `Association`

這些應視為 tracker 內部實作細節。

### 4.2 Detection 由外部提供

tracker 不做 detection，也不擁有 frame buffer。

### 4.3 Track 對 embedding/decision 友善

Track 結構應保留：

- `best_face_quality`
- `identity_id`
- `identity_score`
- `last_detection_score`
- `face_quality`
- `best_face_quality`

但 tracker 本身不做身份判定。

---

## 5. 第一版主要介面

```cpp
class Tracker {
 public:
  explicit Tracker(const TrackerConfig& config);

  void Reset();
  void Step(const std::vector<Detection>& detections, uint64_t timestamp_ms);

  const std::vector<Track>& GetTracks() const;
  std::vector<TrackSnapshot> GetTrackSnapshots() const;
  std::vector<Track> GetConfirmedTracks() const;
  std::vector<Track> GetEmbeddingCandidates() const;
  std::vector<TrackSnapshot> GetEmbeddingTriggerCandidates(
      uint64_t timestamp_ms) const;
  bool MarkEmbeddingTriggered(int track_id, uint64_t timestamp_ms);
};
```

### 5.1 `GetEmbeddingCandidates()` 第一版規則

第一版先採保守規則，只有以下條件同時成立才視為可送 embedding：

- `track.state == Confirmed`
- `track.miss == 0`
- `track.last_detection_score >= embed_det_score_thresh`
- `track.bbox_latest.w >= embed_min_face_width`
- `track.bbox_latest.h >= embed_min_face_height`
- `track.face_quality >= embed_min_face_quality`

這樣做的目的不是最終版 face quality，而是先把「可做 embedding 的條件」從 tracker 對外明確化。

### 5.2 `GetEmbeddingTriggerCandidates()` 第一版規則

在 `GetEmbeddingCandidates()` 的基礎上，再加上：

- 與上次 embedding 的時間差必須大於等於 `embed_interval_ms`

目的：

- 避免同一條 confirmed track 每幀都觸發 embedding
- 降低大庫搜尋壓力
- 讓 tracker 能直接支援節流

### 5.3 第一版 `face_quality` 規則

目前程式中的第一版估分是保守版，主要用兩個訊號：

- detection score normalized
- face size normalized

再做加權組合得到 `0.0 ~ 1.0` 的分數。  
這不是最終版品質模型，但已足以作為 embedding gate 的第一個版本。

---

## 6. 下一步

在這份介面規格之後，應優先完成：

1. `include/tracker/` 標頭
2. `src/tracker/` 骨架
3. 基本單元測試

目前第一版已經完成上述三項，下一步可往下延伸：

1. 加入 face quality 與 embeddable rule
2. 定義 tracker 對 alignment / embedding 的輸出格式
3. 補更多多人與邊界情境測試

---

## 7. CPU-First 驗證策略

tracker 模組目前優先在 `Intel CPU` 上進行：

- 演算法驗證
- 多情境測試
- lifecycle 與節流邏輯驗證

之後遷移到 `RV1106 + RKNN` 時，應保持：

- detection 輸出格式一致
- timestamp 與 bbox 座標定義一致
- gating / lifecycle / trigger 規則不因後端切換而改寫

### 7.1 但 tracker 的節流策略要以 RKNN 為主

tracker 雖然在 CPU 上驗證，但以下參數本質上是在保護板端算力：

- `max_tracks`
- `embed_interval_ms`
- `GetEmbeddingCandidates()` gate
- `GetEmbeddingTriggerCandidates()` gate

這些規則不只是演算法偏好，而是為了讓 `RV1106 + RKNN` 真正跑得動。
