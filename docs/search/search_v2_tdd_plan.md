# Search V2 TDD Plan

## 1. 目的

這份文件定義 `Search V2` 的測試驅動開發順序。

目標不是一次把所有 weighted search 能力都寫完，而是：

- 先把 prototype-level 計分寫對
- 再把 person-level aggregation 寫對
- 最後再考慮與 pipeline / decision 的整合

---

## 2. 測試原則

`Search V2` 的測試應遵守：

- 先驗證數學規則
- 再驗證排序規則
- 再驗證 person-level 聚合
- 最後才驗證與外部模組的整合

也就是：

- 先小範圍單元測試
- 不先做大型 end-to-end 測試

---

## 3. Phase 1: Prototype-Level Score

第一批測試應先固定：

- `raw_score = cosine_similarity`
- `weighted_score = raw_score * prototype_weight`

### 3.1 建議測試案例

- 相同向量得到最高分
- 正交向量得到低分
- 相同 raw score 下，較高 `prototype_weight` 應排更前
- embedding dimension 不符時應失敗

### 3.2 完成條件

- `PrototypeHit.raw_score` 正確
- `PrototypeHit.weighted_score` 正確
- prototype-level 排序正確

---

## 4. Phase 2: Person-Level Aggregation

第二批測試再加入：

- 同一個 `person_id` 有多個 prototypes
- 使用 `max(weighted_score)` 聚合

### 4.1 建議測試案例

- 同一 person 的較佳 prototype 應成為 person score
- prototype 數量較多的人，不應因數量而天然占優勢
- 不同 zone 的 prototypes 對同一 person 應能正確競爭

### 4.2 完成條件

- `SearchHitV2.score` 正確
- `best_prototype_id` 正確
- `best_zone` 正確
- person-level top-k 正確

---

## 5. Phase 3: Margin 與 Top-K

第三批測試固定：

- `top_k`
- `top1_top2_margin`

### 5.1 建議測試案例

- person-level 排序後只保留 `top_k`
- top-1 與 top-2 margin 正確
- 只有一個 person hit 時，margin 應退化成 top-1 score

### 5.2 完成條件

- `SearchResultV2.top1_top2_margin` 行為穩定
- 與目前 decision 需求相容

---

## 6. Phase 4: Debug 視圖

這一階段補的是可解釋性，不是搜尋主邏輯。

### 6.1 建議測試案例

- `prototype_hits` 應保留 prototype-level排序資訊
- person-level top-1 的 `best_prototype_id` 應能回對到 `prototype_hits`
- `best_zone` 應與該 prototype 的 zone 一致

### 6.2 完成條件

- debug 視圖足以支援：
  - prototype 命中追查
  - zone 影響分析
  - regression report

---

## 7. Phase 5: 與 Enrollment 的相容測試

這階段不要求先改 `EnrollmentStore V1`，而是先模擬 V2 prototype 輸入，
再驗證 search-ready index package 邊界。

### 7.1 建議測試案例

- baseline / verified / recent 的固定權重輸入
- `FaceSearchV2IndexPackage` / `LoadFromIndexPackagePath(...)` 能被 search 接受
- `FaceSearchV2IndexPackage` 能作為 search-ready 邊界被 pipeline / search 接受
- 權重設計不會讓 recent adaptive 輕易壓過 baseline

### 7.2 完成條件

- `Search V2` 可以穩定吃 V2 prototype 格式
- `Search V2` 可以穩定吃 search-ready index package
- pipeline / integration 測試預設應先走 `FaceSearchV2IndexPackage` / `.sfsi`
- `EnrollmentStoreV2` 僅保留作 package builder / compatibility coverage
- weighted score 行為符合 `weighted_search_spec.md`

---

## 8. Phase 6: 與 Decision 的相容測試

這不是一開始就要做，但在 search v2 接進 pipeline 前應先驗證。

### 8.1 建議測試案例

- `SearchHitV2` 轉回 decision 所需 person-level資料後，`FaceDecisionEngine` 行為不破
- `top1_top2_margin` 仍可直接用於 existing decision rules

### 8.2 完成條件

- 不需要大改 decision，即可使用 person-level search v2 結果

---

## 9. 建議測試檔方向

後續實作時，建議至少新增或拆分：

- `tests/face_search_v2_prototype_test.cpp`
- `tests/face_search_v2_person_aggregation_test.cpp`
- `tests/face_search_v2_margin_test.cpp`

若還沒進入實作階段，也可先從單一檔案開始：

- `tests/face_search_v2_test.cpp`

---

## 10. 建議實作順序

最務實的順序是：

1. 先新增 V2 型別
2. 先讓 prototype-level weighted score 測試通過
3. 再補 person-level aggregation
4. 再補 margin / top-k
5. 最後再接 pipeline

這樣可以避免：

- search 還沒穩
- decision 已經被迫一起改
- regression 很難判讀

---

## 11. 與本專案目前狀態的關係

目前專案還在：

- pipeline 已打通
- search v1 穩定
- enrollment v2 與 weighted search 僅完成規格化

因此現在最合理的下一步不是直接重構主線，而是：

- 先補 `Search V2` 的測試骨架與規格
- 等條件成熟後，再做最小可用實作
