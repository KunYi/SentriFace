# Embedding Product Strategy

## 1. 目的

這份文件定義本專案在產品化前提下，應如何使用 `embedding`。

核心原則不是追求學術式的每幀最佳 identity，而是：

- 在 `RV1106` 可承受的算力下運作
- 在使用者可接受的時間內完成辨識
- 讓門禁最終 decision 穩定可靠

---

## 2. 產品導向前提

本專案是門禁產品，不是研究型多目標追蹤系統。

因此 `embedding` 的定位應該是：

- 用來做身份確認
- 用來做短時間斷軌補強
- 用來做資料庫搜尋

而不是：

- 每幀都跑一次
- 企圖把 tracker 變成重型 appearance tracker

---

## 3. 延遲目標

目前產品化基線可先定為：

- 一般門禁可接受在 `2 秒內` 得到穩定結果

這代表系統不需要把每個 frame 都送去做 embedding 或 DB search。

更合理的節奏是：

- tracker 持續輕量運作
- embedding 只在關鍵時刻觸發
- decision 在短時間窗內累積穩定證據

---

## 4. 建議使用方式

### 4.1 tracker 與 embedding 分工

- `tracker` 負責持續維持畫面中有效人物的主次與存在狀態
- `embedding` 負責在必要時提供身份特徵

也就是：

- 不是要求 `track_id` 永遠不斷
- 而是要求在最終 decision 時，系統仍能在可接受時間內判斷出正確人員

### 4.2 embedding 觸發時機

第一版較合理的條件是：

- `Confirmed`
- 臉尺寸足夠大
- 品質達門檻
- 距離上次 embedding 已超過冷卻時間
- 或 track 剛穩定
- 或 track 剛重新出現，且位於最近的有效人物區域附近

### 4.3 embedding cache

系統應保留最近一次或最近幾次有效 embedding 作為 cache。

這些 cache 的主要用途是：

- 短時間 `1 ~ 2` 秒內的 re-link
- 減少重複推論
- 在同一門禁事件中避免過度頻繁查庫

較合理的產品策略是：

- `id` 先保持
- feature cache 超過一定時間、或事件到達適當階段，才真的再做 embedding / search

---

## 5. 不必強求全部 reconnect

在門禁場景中，不一定要把所有短斷軌都重新接成同一條 track。

更重要的是：

- 畫面中有效人物要有主次
- 系統知道目前誰值得繼續處理
- 最終在時間窗內得到穩定 decision

因此：

- track 有短暫切段可以接受
- 只要 decision session 仍能在 `2 秒內` 穩定收斂即可

---

## 6. 成功驗證後的 prototype 更新

這是一個值得正式保留的產品設計方向。

當系統已經成功辨識出使用者，且成功條件足夠強時，可考慮把這次新的 embedding 納入該使用者的 prototype pool。

目的在於：

- 讓資料庫逐步跟上使用者最新外觀
- 吸收髮型、年齡、鬍子、眼鏡、光線條件等慢變因素

但這件事不應直接覆蓋原始 enrollment 基準，而應採：

- 分區
- 分權重
- 漸進融合

---

## 7. 建議的 prototype zones

可將每位使用者的 prototype 分成至少三個區域：

### 7.1 enrollment baseline zone

這是最初註冊時取得的基準資料。

特性：

- 最可信
- 權重最高
- 不應輕易被覆蓋

### 7.2 verified history zone

這是後續多次成功驗證後累積的 prototype。

特性：

- 權重中高
- 允許隨時間更新
- 用來吸收使用者外觀慢變

### 7.3 recent adaptive zone

這是最近期成功驗證後加入的短期 prototype。

特性：

- 權重中低
- 容量較小
- 可滾動替換
- 用來快速適應近期外觀變化

---

## 8. 融合原則

比較合理的方式不是直接平均所有 prototype，而是：

- baseline zone 權重最高
- verified history zone 次之
- recent adaptive zone 最低

而且只有在以下條件都夠強時，才應加入新的 adaptive prototype：

- 多次 temporal smoothing 已穩定
- search top-1 穩定
- margin 足夠
- liveness 或其他安全條件通過

---

## 9. 對本專案的直接意義

這份策略確立了幾件事：

- `w600k_mbf` 這類 embedding 模型可以用，但不能無節制高頻執行
- 系統應該偏向 `event-driven embedding`
- 真正的產品目標是 `2 秒內穩定辨識`
- prototype store 未來應支援 baseline / verified / recent 三層概念

因此後續實作方向應優先是：

- embedding trigger 節流
- embedding cache
- session-level decision
- adaptive prototype policy

而不是先把 tracking 做成重型 appearance system

---

## 10. Pipeline Update Workflow

目前專案的最小產品化骨架已把這條路進一步收斂成：

1. `FacePipeline` 對 `unlock_ready` tracks 建立 update candidate
2. candidate 會整理：
   - `person_id`
   - `embedding`
   - `timestamp_ms`
   - `quality_score`
   - `decision_score`
   - `top1_top2_margin`
   - `liveness_ok`
3. `EnrollmentStoreV2` 再依 policy 判斷：
   - 是否可寫入 `recent adaptive`
   - 是否可寫入 `verified history`

這樣的好處是：

- 不需要把 adaptive update 硬塞進 decision engine
- policy 邊界清楚
- 日後接真實 liveness / GPIO / RPC 流程時也比較容易維持安全性
