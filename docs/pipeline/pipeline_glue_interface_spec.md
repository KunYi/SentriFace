# Pipeline Glue Interface Spec

## 1. 目的

這份文件定義第一版 `pipeline glue` 模組介面。

它的角色不是重新實作各子模組，而是把以下模組串起來：

- tracker
- enrollment
- search
- decision

後續也會成為：

- RTSP 驗證線
- 真實 detector / embedder backend

的整合入口之一。

---

## 2. 介面定位

目前模組位置：

```text
include/app/face_pipeline.hpp
src/app/face_pipeline.cpp
```

主要類型：

- `PipelineConfig`
- `PipelineTrackState`
- `FacePipeline`

---

## 3. 第一版設計

`FacePipeline` 第一版提供：

1. 同步 tracker snapshots
2. 載入 enrollment / search runtime artifacts 到 search
3. 對 embedding 做 search
4. 把 search result 更新到 decision

```cpp
class FacePipeline {
 public:
  explicit FacePipeline(const PipelineConfig& config = PipelineConfig {});

  void Reset();
  void SyncTracks(const std::vector<sentriface::tracker::TrackSnapshot>& snapshots);
  bool LoadEnrollment(const sentriface::search::FaceSearchV2IndexPackage& package);
  bool LoadEnrollment(const std::string& search_index_package_path);
  bool LoadEnrollmentBaselinePackage(const std::string& input_path,
                                     int person_id,
                                     float baseline_weight = 1.0f);
  bool LoadEnrollment(const sentriface::enroll::EnrollmentStore& store);
  bool LoadEnrollment(const sentriface::enroll::EnrollmentStoreV2& store);
  bool ExportEnrollmentV2IndexPackage(
      sentriface::search::FaceSearchV2IndexPackage* out_package) const;
  bool SaveEnrollmentV2IndexPackageBinary(const std::string& output_path) const;

  sentriface::search::SearchResult SearchEmbedding(
      const std::vector<float>& embedding) const;
  sentriface::search::SearchResultV2 SearchEmbeddingV2(
      const std::vector<float>& embedding) const;
  sentriface::decision::DecisionState UpdateTrackSearch(
      int track_id, const sentriface::search::SearchResult& result);
};
```

對 `Search V2` 來說，`FacePipeline` 的主要載入邊界應是 `.sfsi`
search-ready binary package 或 `FaceSearchV2IndexPackage`。
若目前只持有 `.sfbp`，也可由 `LoadEnrollmentBaselinePackage(...)` 透過
正式 `.sfbp -> .sfsi` helper 載入。
對 sample / offline runner 而言，`.sfbp` 載入時使用的 `person_id`
也應可由環境變數明確指定，而不是硬編碼在 runner 內部。
`EnrollmentStoreV2` 只保留作為 build / fallback / adaptive state 相容入口。
若 runtime 已持有 `.sfsi`，則不應再回退成 `EnrollmentStoreV2 -> export`
後重建 search。

這樣 runtime / replay runner 可以：

- 直接吃 search-ready index
- 避免每次重做 store export
- 避免冷啟動時重新 normalize / rebuild matrix

---

## 4. 第一版定位

這個模組目前是：

- CPU-first
- backend-agnostic
- 偏 orchestration 的協調層

它不直接做：

- detection
- alignment warp
- embedding inference
- GPIO control

但它提供一條足夠清楚的主資料流骨架，方便後續整合。

---

## 5. 門禁事件層補充

目前 `FacePipeline` 除了串接 search / decision 外，也開始承擔一個門禁很實際的責任：

- 將短時間內斷掉又重新出現的 track 視為同一次通行事件

也就是說，這一層不是只做「把模組黏起來」，還負責：

- 把 tracker 的短暫切段
- 轉成較穩定的 decision session

這對門禁特別重要，因為系統通常只需要：

- 最後穩定身份
- 最後 unlock-ready 判斷

而不是要求單一 `track_id` 從頭到尾絕對不變。

目前 short-gap merge 的判斷不只看時間窗，還會額外檢查：

- 新舊 track 的 `stable_person_id` 是否一致
- 中心點位移是否仍在合理範圍
- bbox 面積比例是否仍在合理範圍

若後續有可用 embedding cache，這一層也可進一步使用：

- `strong embedding similarity`

作為短時間 re-link 的輔助訊號。

這樣可以降低錯誤合併的風險，讓規則更接近真實門禁事件，而不是單純的時間拼接。
