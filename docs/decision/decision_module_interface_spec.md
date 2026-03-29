# Decision Module Interface Spec

## 1. 目的

這份文件定義第一版 `decision` 模組介面。

第一版先專注於：

- track 為單位的 temporal smoothing
- top-1 / top-2 margin 使用
- unlock-ready 判定

暫時不直接控制 GPIO。

---

## 2. 介面定位

目前模組位置：

```text
include/decision/face_decision.hpp
src/decision/face_decision.cpp
```

主要類型：

- `DecisionConfig`
- `DecisionVote`
- `DecisionState`
- `FaceDecisionEngine`

---

## 3. 第一版設計

`FaceDecisionEngine` 以 `track_id + SearchResult` 為輸入，累積每條 track 的歷史，輸出：

- 穩定 person id
- consistent hits
- average score
- average margin
- unlock_ready

```cpp
class FaceDecisionEngine {
 public:
  explicit FaceDecisionEngine(const DecisionConfig& config = DecisionConfig {});

  void Reset();
  DecisionState Update(int track_id, const sentriface::search::SearchResult& result);
  bool RemoveTrack(int track_id);
};
```

### 3.1 第一版 unlock-ready 規則

第一版使用：

- `min_consistent_hits`
- `unlock_threshold`
- `avg_threshold`
- `min_margin`

作為保守決策條件。

---

## 4. CPU-First 與板端邊界

decision 雖然先在 CPU 上驗證，但設計目標仍是：

- 給 `RV1106 + RKNN` 的門禁使用

因此第一版只做：

- 輕量歷史累積
- 簡單票數與平均分數
- 不引入重型時序模型

---

## 5. 短時間斷軌的處理原則

對門禁來說，實務上常見的情況不是多人長時間交錯，而是：

- 同一個人在鏡頭前微轉頭
- 短暫前後移動
- tracker 因姿態變化切出新的 `track_id`

因此目前設計方向是：

- 不急著把 tracker 升級成重型外觀式追蹤
- 先在 `decision / pipeline` 層做短時間斷軌合併

也就是說，若：

- 舊 track 剛消失
- 新 track 很快出現
- 新舊 track 的穩定 person id 相同

則系統可以把舊 track 的 decision history 合併到新 track，讓 unlock 判定更貼近
「同一個門禁使用者的一次通行事件」，而不是死綁單一 `track_id`。

---

## 6. 與 Search V2 的相容方向

未來若 search 升級到 weighted prototype + person-level aggregation，decision 第一版不需要大幅改寫。

較合理的方向是：

- search 仍輸出 person-level top-k
- decision 繼續看 `top-1 / top-2 margin`
- 只在 debug / validation 時額外使用：
  - `best_prototype_id`
  - `best_zone`
  - prototype-level hits

這樣可以讓系統逐步演進，而不是一次重構 search 與 decision。

---

## 7. 與 Adaptive Update Workflow 的關係

`decision` 本身不應直接寫入 enrollment store。

較合理的責任分工是：

- `decision` 負責產生 `unlock_ready`
- `pipeline` 負責整合：
  - `snapshot.face_quality`
  - `average_score`
  - `average_margin`
  - `liveness_ok`
  - 最新可用 embedding
- `enrollment store v2` 只負責依 policy 判斷是否接受並寫入對應 zone

目前專案的最小骨架已朝這個方向落實成：

- `FacePipeline::GetPrototypeUpdateCandidates(...)`
- `FacePipeline::ApplyAdaptivePrototypeUpdates(...)`

因此後續若要把成功驗證事件寫回 `recent adaptive / verified history`，
應透過 pipeline workflow 完成，而不是由 decision engine 直接操作資料庫。

若後續接入真實 `liveness / anti-spoof`，其 threat model 與 feature 邊界應以：

- [docs/access/rgb_liveness_anti_spoof_spec.md](../access/rgb_liveness_anti_spoof_spec.md)

為準；`decision` 層只消費 `liveness_ok` 與其 gating 結果。
