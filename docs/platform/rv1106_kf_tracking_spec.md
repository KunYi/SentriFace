# RV1106 KF Tracking Spec

## 1. 文件目的

這份文件定義本專案在 `RV1106 + Luckfox + SC3336` 平台上的 `Kalman Filter face tracking` 規格，作為後續實作與驗證基準。

本文件不是通用 MOT 規格，而是針對以下前提設計：

- 開發板：`Luckfox Pico Pro (RV1106G2)` 或 `Pico Max (RV1106G3)`
- 鏡頭：`SC3336 3MP`
- 安裝方向：**直立模式**
- 偵測器：`SCRFD-500M`
- 場景：固定式近距離人臉門禁
- 資料庫：可能擴展到 `上萬人`

本文件直接建立在以下文件之上：

- [rv1106_system_hardware_baseline.md](./rv1106_system_hardware_baseline.md)
- [rv1106_face_tracking_embedding_design.md](./rv1106_face_tracking_embedding_design.md)
- [rv1106_tracking_technical_evaluation.md](./rv1106_tracking_technical_evaluation.md)

---

## 2. 設計目標

本 tracking 模組的目標不是追求最複雜的多目標追蹤能力，而是：

1. 讓同一張臉在連續幀中維持穩定 `track_id`
2. 降低 `SCRFD` bbox 抖動對後續 alignment / embedding 的影響
3. 容忍短暫漏檢
4. 控制 embedding 與 DB search 的觸發節奏
5. 在大型資料庫下降低重複查庫與 identity 跳動

---

## 3. 系統前提

### 3.1 影像座標系

本模組一律假設輸入已經轉為：

- **直立座標系**

也就是演算法層看到的座標定義為：

- `x`: 由左到右
- `y`: 由上到下

若 sensor 原始輸出不是直立，必須在 tracking 前完成旋轉或等價座標轉換。

### 3.2 Detection 前提

每一幀輸入為 `SCRFD-500M` detection 結果：

```text
Detection {
    bbox: [x1, y1, x2, y2]
    score: float
    landmarks5: [left_eye, right_eye, nose, left_mouth, right_mouth]
    timestamp_ms: uint64
}
```

tracking 模組不負責 detection，本模組只消耗 detection 輸出。

### 3.3 運動場景前提

系統假設：

- 鏡頭固定
- 人臉數量少
- 目標移動平滑
- 人臉在門口停留時間短到中等

因此狀態模型可採用：

- **constant velocity linear model**

---

## 4. 為什麼選 KF 而不是 EKF

本專案選擇 `KF` 而不是 `EKF`，原因如下：

- 追蹤狀態主要是 2D bbox
- 短時間內線性近似通常足夠
- 門禁鏡頭固定，不需複雜非線性相機模型
- KF 在 RV1106 上幾乎沒有計算壓力
- 工程風險、除錯成本與調參難度都明顯低於 EKF

因此本規格固定採用：

- `IoU + Kalman Filter`

而非：

- `IoU + EKF`

---

## 5. Track 狀態定義

### 5.1 狀態向量

每個 track 使用以下 8 維狀態：

```text
x = [cx, cy, w, h, vx, vy, vw, vh]^T
```

其中：

- `cx, cy`: bbox 中心座標
- `w, h`: bbox 寬高
- `vx, vy`: 中心速度
- `vw, vh`: 寬高變化速度

### 5.2 量測向量

由 detection 提供的量測定義為：

```text
z = [cx, cy, w, h]^T
```

由 bbox 換算：

```text
cx = (x1 + x2) / 2
cy = (y1 + y2) / 2
w  = x2 - x1
h  = y2 - y1
```

### 5.3 為什麼不用 `s, r`

有些 SORT 類方法使用：

```text
[cx, cy, s, r, vx, vy, vs]
```

本專案不採用這種表示，理由是：

- `w, h` 更容易 debug
- 門禁工程人員較容易理解
- 直接對應 crop 大小與 face-size threshold
- 與 alignment / ROI / size gating 更直觀

---

## 6. Kalman Filter 模型

### 6.1 狀態轉移模型

採用 constant velocity model。

在固定幀率近似下：

```text
cx_k = cx_{k-1} + vx_{k-1} * dt
cy_k = cy_{k-1} + vy_{k-1} * dt
w_k  = w_{k-1}  + vw_{k-1} * dt
h_k  = h_{k-1}  + vh_{k-1} * dt
vx_k = vx_{k-1}
vy_k = vy_{k-1}
vw_k = vw_{k-1}
vh_k = vh_{k-1}
```

### 6.2 狀態轉移矩陣 `F`

```text
F =
[1 0 0 0 dt  0  0  0]
[0 1 0 0  0 dt  0  0]
[0 0 1 0  0  0 dt  0]
[0 0 0 1  0  0  0 dt]
[0 0 0 0  1  0  0  0]
[0 0 0 0  0  1  0  0]
[0 0 0 0  0  0  1  0]
[0 0 0 0  0  0  0  1]
```

### 6.3 量測矩陣 `H`

```text
H =
[1 0 0 0 0 0 0 0]
[0 1 0 0 0 0 0 0]
[0 0 1 0 0 0 0 0]
[0 0 0 1 0 0 0 0]
```

### 6.4 流程噪聲 `Q`

第一版採用對角矩陣即可：

```text
Q = diag(
    q_pos, q_pos, q_size, q_size,
    q_vel, q_vel, q_vsize, q_vsize
)
```

建議初值：

```text
q_pos   = 4.0
q_size  = 4.0
q_vel   = 9.0
q_vsize = 9.0
```

若實測發現：

- prediction 太僵硬：增大 `Q`
- prediction 太飄：減小 `Q`

### 6.5 量測噪聲 `R`

同樣先用對角矩陣：

```text
R = diag(
    r_pos, r_pos, r_size, r_size
)
```

建議初值：

```text
r_pos  = 16.0
r_size = 25.0
```

如果 `SCRFD` 很穩，可以逐步減小 `R`。

### 6.6 初始協方差 `P0`

新 track 建立時建議：

```text
P0 = diag(
    25, 25, 25, 25,
    100, 100, 100, 100
)
```

意思是：

- 初始位置與尺寸相對可信
- 但初始速度很不確定

---

## 7. Detection 預處理

在進入 tracking 前，先做 detection filter。

### 7.1 基本門檻

```text
det_score_thresh = 0.55
min_face_width   = 50 px
min_face_height  = 50 px
```

### 7.2 邊界過濾

若 bbox 過度貼邊，可先標記為低品質，不建議優先拿來建立高價值 track。

### 7.3 ROI 過濾

在門禁系統中，建議只讓位於有效門禁 ROI 中的人臉進入正式 tracking 流程。
ROI 外的 detection 可：

- 丟棄
- 或降權處理

---

## 8. Prediction 規格

### 8.1 每幀流程

每一幀開始時，先對所有非 `Removed` track 執行：

```text
predict(x, P, dt)
```

### 8.2 `dt` 定義

`dt` 應由時間戳實際計算：

```text
dt = (t_k - t_{k-1}) / 1000.0
```

如果第一版系統幀率很穩，也可暫時固定為：

```text
dt = 1 / fps
```

但正式版本建議使用實際時間戳。

### 8.3 Prediction clamp

預測後建議加入合理限制：

- `w > 0`
- `h > 0`
- bbox 不可完全飛出畫面太遠

避免濾波器因異常值失控。

---

## 9. Association 規格

### 9.1 關聯總原則

本規格採用：

- **KF prediction 提供預測 bbox**
- **IoU + 幾何 gating 決定可配對集合**
- **cost-based assignment 完成匹配**

### 9.2 Gating 條件

對每組 `(track, detection)` 先檢查：

```text
IoU(pred_box, det_box) >= 0.20
center_distance <= 0.60 * diag(pred_box)
0.5 <= det_area / pred_area <= 2.0
```

若任一條件不成立，視為不可配對。

### 9.3 成本函數

```text
cost =
    0.55 * (1 - IoU)
  + 0.30 * normalized_center_distance
  + 0.15 * abs(log(det_area / pred_area))
```

其中：

```text
normalized_center_distance =
    distance(center_pred, center_det) / diag(pred_box)
```

### 9.4 Assignment 方法

第一版建議：

- `greedy matching`

流程：

1. 產生所有合法 pair 與 cost
2. 依 cost 由小到大排序
3. 依序選取尚未被使用的 track 與 detection

若後續多人情境增加，再升級：

- Hungarian assignment

### 9.5 是否使用 Mahalanobis gating

本規格第一版可不強制使用 Mahalanobis gating。
原因：

- 幾何 gating 已足夠應付門禁場景
- 程式較簡單
- 容易 debug

若後續實測需要再補上：

- Mahalanobis distance gating

---

## 10. Track Lifecycle

### 10.1 Track 狀態

每個 track 有四種狀態：

- `Tentative`
- `Confirmed`
- `Lost`
- `Removed`

### 10.2 狀態轉換

#### 新建 track

未匹配 detection 建立新 track，初始狀態：

- `Tentative`

#### Confirm 條件

若連續匹配達：

```text
min_confirm_hits = 3
```

則轉為：

- `Confirmed`

#### Lost 條件

若某幀未匹配 detection：

- `miss += 1`

且進入：

- `Lost`

#### Remove 條件

若：

```text
miss > max_miss
```

則轉為：

- `Removed`

建議初值：

```text
max_miss = 8
```

### 10.3 Tentative 的處理原則

`Tentative` track 不應直接用於：

- unlock decision
- 積極查大庫

這樣可以降低假警報對系統的污染。

---

## 11. Track 資料結構

建議每個 track 除 KF 狀態外，還維護：

```text
Track {
    track_id: int
    state: enum
    age: int
    hits: int
    miss: int
    last_timestamp_ms: uint64

    x[8]: float
    P[8x8]: float

    bbox_latest: [x1, y1, x2, y2]
    bbox_pred: [x1, y1, x2, y2]
    landmarks5_latest

    face_quality: float
    best_face_quality: float
    best_embedding[]

    identity_id
    identity_score
    decision_buffer
}
```

### 11.1 為什麼 track 要帶 identity 狀態

因為在本專案中，tracking 不是孤立模組。
它必須服務：

- embedding 節流
- DB search 節流
- temporal smoothing

所以 track 需要保留辨識層上下文。

---

## 12. Update 規格

### 12.1 匹配成功時

若 detection 與 track 成功匹配：

1. 用 detection 轉成量測 `z`
2. 執行 KF update
3. 更新 `bbox_latest`
4. 更新 `landmarks5_latest`
5. `hits += 1`
6. `miss = 0`
7. 若已達 confirm 門檻則轉為 `Confirmed`

### 12.2 未匹配時

若某 track 本幀未匹配 detection：

1. 保留 predict 後的狀態
2. `miss += 1`
3. 狀態設為 `Lost`
4. 若 `miss > max_miss` 則 `Removed`

### 12.3 新 track 初始化

使用 detection 初始化：

```text
cx, cy, w, h from bbox
vx = vy = vw = vh = 0
```

並用 `P0` 初始化協方差。

---

## 13. 與 Embedding / DB Search 的耦合規格

### 13.1 Tracking 不負責 identity 決策

tracking 模組本身不應直接判斷「這個人是誰」。
它只提供穩定的 face track 與高品質候選。

### 13.2 Embedding 觸發條件

建議只有在以下情況才觸發 embedding：

- track 狀態為 `Confirmed`
- track `miss == 0`
- detection score >= `0.75`
- face width >= `80 px`
- face height >= `80 px`
- face quality 達門檻
- 距離上次 embedding 已超過 `embed_period_frames`

在目前程式第一版中，這條規則已經開始具體化為：

- `embed_det_score_thresh`
- `embed_min_face_width`
- `embed_min_face_height`
- `embed_min_face_quality`
- `embed_interval_ms`

其中 `embed_min_face_quality` 目前由第一版 `face_quality` evaluator 提供，使用：

- detection score
- face size

作為保守的前置過濾條件。

### 13.3 大型資料庫下的節流原則

若資料庫達上萬人，tracking 層應明確支援：

- 同一條 track 不要每幀查庫
- 同一 identity 連續穩定時減少查詢頻率
- Lost 後短期恢復的 track 可沿用既有 decision context

### 13.4 重要規則

`Tentative` track 不應進行高成本全庫搜尋。
正式查庫應優先保留給：

- `Confirmed`
- 高品質
- 位於有效 ROI

的 track。

---

## 14. 推薦參數初值

### 14.1 Tracking

```text
det_score_thresh    = 0.55
min_face_width      = 50
min_face_height     = 50
iou_min             = 0.20
center_gate_ratio   = 0.60
area_ratio_min      = 0.50
area_ratio_max      = 2.00
min_confirm_hits    = 3
max_miss            = 8
max_tracks          = 5
```

### 14.1.1 `max_tracks` 的系統意義

`max_tracks` 不只是效能參數，也是門禁系統邊界定義。

第一版建議：

- 預設 `max_tracks = 5`
- 若鏡頭 FOV 較窄、操作區更受控，可改成 `3`

理由：

- 門禁場景同時真正需要處理的操作人臉通常很少
- 限制 track 數量可避免把資源浪費在背景路人
- 可降低後續 embedding 與大庫搜尋負擔
- 可減少多人同框造成的 identity noise

### 14.2 Embedding trigger

```text
embed_det_score     = 0.75
embed_min_face_w    = 80
embed_period_frames = 5
force_embed_frames  = 15
```

### 14.3 大型資料庫 decision

```text
search_topk             = 20
min_consistent_hits     = 3
top1_top2_margin_min    = 0.03 ~ 0.08
cooldown_after_unlock   = 2 sec
```

---

## 15. 偽流程

```text
for each frame:
    detections = scrfd(frame)
    detections = filter(detections)

    for each track:
        kf_predict(track, dt)

    candidate_pairs = build_pairs(predicted_tracks, detections)
    matches = greedy_assign(candidate_pairs)

    for each matched pair:
        kf_update(track, detection)
        update_track_metadata(track, detection)

        if should_run_embedding(track, detection):
            aligned = align_face(frame, detection.landmarks5)
            emb = run_embedding(aligned)
            search_result = db_search(emb)
            update_track_identity(track, search_result)

    for each unmatched_track:
        mark_lost(track)

    for each unmatched_detection:
        create_tentative_track(detection)

    remove_dead_tracks()
    run_temporal_smoothing()
```

---

## 16. 驗證重點

這份規格落地後，應優先驗證：

1. bbox 是否比純 IoU tracking 更平滑
2. 短暫漏檢時 track 是否能維持
3. 同一人停留 2 秒時是否不會頻繁重建 track
4. embedding 次數是否明顯下降
5. 大型資料庫下 identity 是否更穩定

---

## 17. 後續升級點

若之後需要升級，可沿以下方向前進：

1. 加入 Mahalanobis gating
2. assignment 升級為 Hungarian
3. Lost track 的短期 re-association 加入 embedding 輔助
4. 對不同 face size 使用自適應 `Q / R`

但這些都應建立在本文件這個第一版 KF 規格先跑穩的前提上。
