# RV1106 + Luckfox 人臉門禁系統設計說明

## 1. 文件目的

這份文件專門回答兩個問題：

1. `tracking` 和 `embedding` 到底在做什麼，為什麼人臉門禁一定會需要它們。
2. 在 `RV1106G2 / RV1106G3 + Luckfox + camera` 上，若我們已經用 `SCRFD-500M` 拿到每一幀的人臉偵測結果，接下來該怎麼設計一個實際可落地的 tracking 演算法。

本文假設系統 pipeline 如下：

- Phase 0 camera deterministic pipeline
- Phase 1 detection
- Phase 2 tracking
- Phase 3 alignment
- Phase 4 embedding
- Phase 5 DB search
- Phase 6 temporal smoothing
- Phase 7 enrollment pipeline
- Phase 8 threshold tuning
- Phase 9 GPIO unlock integration
- Phase 10 liveness detection

另外加入一個新的系統假設：

- 已註冊人臉資料庫規模可能達到 `上萬人`

---

## 2. 先講結論：tracking 和 embedding 各自的任務

### 2.1 Tracking 是「這一張臉，在連續影格裡是不是同一個人」

偵測器 `SCRFD-500M` 每一幀只會告訴你：

- 這一幀有哪些人臉
- 每張臉的 bounding box
- confidence
- 如果模型版本支援，還有 5 點 landmarks

但它**不會告訴你**：

- 第 20 幀左邊那張臉，是否就是第 19 幀左邊那張臉
- 這個人剛剛被遮到 3 幀後重新出現，是不是同一個人
- 這一秒鐘內的 15 次辨識結果，哪些屬於同一個訪客

Tracking 的任務就是幫每張臉建立一個短期身份：

- `track_id = 7` 代表目前畫面中的某一個人
- 這個人從進入畫面到離開畫面，都盡量維持同一個 `track_id`

所以 tracking 處理的是：

- **影格內到影格間的連續性**
- **短時間同一張臉的穩定追蹤**
- **把 detection 結果串成一段 track**

它不是最終身份辨識，它只是先回答「是不是同一個畫面中的人」。

### 2.2 Embedding 是「這張臉在特徵空間裡長得像誰」

Embedding model 會把一張已對齊的人臉裁圖，轉成一個固定長度向量，例如：

- 128 維
- 256 維
- 512 維

這個向量不是姓名，但它會有一個很重要的性質：

- 同一個人的 embedding 距離比較近
- 不同人的 embedding 距離比較遠

所以 embedding 處理的是：

- **長期身份特徵**
- **拿來跟資料庫做 1:N 比對**
- **決定這張臉是不是資料庫中的某個已註冊人員**

簡單說：

- `tracking` 解決「這幾幀是不是同一張臉」
- `embedding` 解決「這張臉是誰」

---

## 3. 為什麼不能只有 detection + DB search

很多人第一眼會想：

1. 每一幀先做人臉偵測
2. 每一幀直接裁臉
3. 每一幀直接做 embedding
4. 每一幀直接查 DB
5. 分數超過門檻就開門

理論上可行，但工程上會很不穩。

### 3.1 沒有 tracking，結果會抖動

因為每一幀的人臉框都會有微小漂移：

- 框大小會變
- 中心點會跳
- 偵測置信度會波動
- 有時一幀抓到，下一幀漏掉

如果你每幀都獨立辨識，就會出現：

- 第 1 幀像 A
- 第 2 幀像 B
- 第 3 幀又像 A
- 第 4 幀因為角度不好變 unknown

門禁系統就會變得很不可信。

### 3.2 沒有 tracking，算力會浪費

在 RV1106 這種資源受限平台上，每幀都做 embedding + DB search 很浪費。

其實同一個人連續站在門口 1 秒鐘，畫面裡是同一張臉。
這時候比較合理的策略是：

- 偵測每幀都做，確保不漏
- tracking 每幀更新，維持 face track
- embedding 不一定每幀都做，只在「品質好」或「定期更新」時做

這樣可以省很多 NPU/CPU 時間。

### 3.3 沒有 alignment + tracking，embedding 品質不穩

人臉 embedding 很怕：

- 臉歪掉
- 框切太多背景
- 眼睛位置飄
- 模糊
- 臉太小

tracking 可以幫你先累積幾幀，再挑：

- 比較大的臉
- 比較正的臉
- 比較清楚的臉
- landmarks 比較穩的幀

再送去 alignment 和 embedding，辨識率會高很多。

---

## 4. 你的 pipeline 裡，tracking 和 embedding 在哪裡接起來

### 4.1 推薦資料流

```text
Camera
  -> deterministic capture
  -> SCRFD-500M detection
  -> tracker association / track update
  -> face alignment (using landmarks)
  -> embedding extraction
  -> DB search
  -> temporal smoothing / decision
  -> GPIO unlock
```

### 4.2 每個階段真正做的事

#### Phase 1 detection

輸出每幀的人臉候選：

- bbox: `(x1, y1, x2, y2)`
- score
- landmarks5: `[(lx, ly), ...]`

#### Phase 2 tracking

把本幀 detections 指派給既有 tracks，或建立新 tracks：

- detection 3 -> track 7
- detection 4 -> track 9
- detection 5 -> new track 11

#### Phase 3 alignment

根據 landmarks 做相似變換，把臉轉成固定姿態。

#### Phase 4 embedding

將對齊後的人臉轉成特徵向量。

#### Phase 5 DB search

拿特徵向量與已註冊人員特徵庫比較：

- cosine similarity
- 或 L2 distance

#### Phase 6 temporal smoothing

不要因為單幀結果就開門。
應該用 track 為單位累積多幀證據。

---

## 5. 在 RV1106 平台上，設計原則要先改成「輕量穩定」而不是「學術最強」

`RV1106` 是視覺 SoC，適合做 camera + ISP + NPU 的邊緣 AI 任務，但資源還是有限。

根據 Luckfox 與 Rockchip 公開資料：

- `RV1106G2` 為 `128MB DDR3L`
- `RV1106G3` 為 `256MB DDR3L`
- `RV1106G2` NPU 約 `0.5 TOPS`
- `RV1106G3` NPU 約 `1 TOPS`
- 具有單核 `Cortex-A7`
- 內建 ISP 與 MIPI CSI 視覺輸入能力

這代表我們不應該一開始就上很重的 tracking 架構，例如：

- DeepSORT 全量外觀模型每幀跑
- ByteTrack 但再加大型 re-id 模型
- 偵測、辨識、活體都每幀全開

比較實際的第一版是：

1. `SCRFD-500M` 負責人臉偵測
2. `tracking` 採用輕量級多目標追蹤
3. `alignment` 利用 SCRFD landmarks，不再額外跑人臉關鍵點模型
4. `embedding` 只在條件合適時跑
5. `DB search` 只對穩定 track 做
6. `temporal smoothing` 決定是否 unlock

如果資料庫規模是上萬人，這個原則更重要，因為系統負擔不只在 detection / embedding，也會明顯落在：

- 大量向量比對
- 候選人排序
- 門檻誤判控制
- 多幀決策整合

---

## 6. 建議的 tracking 演算法：Face-SORT-Lite

這裡我建議不要直接套通用 MOT，而是做一個針對門禁人臉場景的簡化版：

- 場景固定
- 鏡頭固定
- 人臉數量通常少
- 人臉尺寸變化相對可控
- 目標是穩定辨識，不是學術 benchmark

我把它稱為 `Face-SORT-Lite`，核心概念是：

1. 用 `SCRFD` 的 bbox + score + landmarks 當 observation
2. 每個 track 維護位置、速度、品質、辨識結果
3. 用 IoU + 中心距離 + 尺寸比例做 detection-to-track 關聯
4. 短時間 miss 容忍
5. embedding 不參與每幀追蹤，但可用於短期 re-association 或辨識穩定化

### 6.1 為什麼先不用「embedding-based tracking」

因為你的主目標是門禁，不是 crowded multi-face re-identification。
在門口場景，多數時間同時只會有 1 到 2 張臉。

所以第一版 tracking 最划算的方法是：

- 先靠幾何資訊追蹤
- 再把 embedding 用於身份辨識

而不是一開始就把 embedding 當 tracking 外觀特徵，原因有三個：

1. 算力比較貴
2. 臉部姿態變化大時，embedding 每幀也會抖
3. 如果幾何追蹤已經足夠，沒必要把系統複雜化

---

## 7. SCRFD-500M detection 輸出後，tracking 怎麼接

### 7.1 每幀 detection 輸入格式

```text
Detection {
    bbox: [x1, y1, x2, y2]
    score: float
    landmarks5: [left_eye, right_eye, nose, left_mouth, right_mouth]
}
```

### 7.2 每個 track 需要維護的狀態

```text
Track {
    track_id: int
    state: Tentative | Confirmed | Lost | Removed
    bbox: [x1, y1, x2, y2]
    velocity: [vx, vy, vw, vh]
    landmarks5: latest landmarks
    age: total frame count since born
    hits: matched frame count
    miss: consecutive unmatched frame count
    quality_score: latest face quality
    best_crop: best aligned face crop snapshot
    best_embedding: best embedding so far
    identity_id: current matched person id or unknown
    identity_score: best similarity score
    decision_buffer: recent recognition history
    last_update_ts: timestamp
}
```

### 7.3 Track 狀態機

#### Tentative

新建立的 track。
如果只出現 1 幀，很可能是假警報，不立刻拿去開門。

#### Confirmed

連續被匹配到足夠幀數後，才當作有效人臉 track。

#### Lost

短暫沒匹配到 detection，但先保留幾幀，等待同一張臉再出現。

#### Removed

超過保留時間，正式刪除。

### 7.4 推薦門檻

第一版可以先這樣設：

- `min_confirm_hits = 3`
- `max_miss = 8`
- `det_score_thresh = 0.55`
- `high_quality_embed_interval = 5 frames`
- `force_embed_interval = 15 frames`

如果鏡頭 15 FPS：

- 連續 3 幀確認，大概 `0.2 秒`
- 最多容忍 8 幀 miss，大概 `0.53 秒`

這對門禁場景通常合理。

---

## 8. Detection-to-Track 關聯規則

### 8.1 先做 gating，過濾不可能的配對

對每個 `track` 和 `detection`，先檢查：

- `IoU >= iou_min`
- 中心點距離不要太大
- 面積比例變化不要太離譜

例如：

- `iou_min = 0.2`
- `center_dist < 0.6 * track_box_diag`
- `0.5 < det_area / track_area < 2.0`

只要其中一項不合理，就不要配。

### 8.2 成本函數

對剩下可配對的組合計算 cost：

```text
cost =
    w_iou * (1 - IoU)
  + w_center * normalized_center_distance
  + w_scale * scale_change
```

推薦初值：

```text
w_iou    = 0.55
w_center = 0.30
w_scale  = 0.15
```

其中：

- `normalized_center_distance = distance(center_t, center_d) / diag(track_bbox)`
- `scale_change = abs(log(det_area / track_area))`

### 8.3 指派方法

有兩種方式：

1. Hungarian assignment
2. 依成本由小到大 greedily match

在門禁臉數通常很少時，兩者都可行。
如果你要實作簡單、好 debug，我建議第一版直接用：

- **greedy matching**

流程：

1. 列出所有合法 `(track, detection, cost)`
2. 按 `cost` 從小到大排序
3. 依序挑選尚未被用過的 track/detection

因為畫面裡通常只有少量人臉，效果通常已足夠。

---

## 9. 每一幀 tracking 更新流程

### 9.1 演算法步驟

```text
Input:
    detections_t from SCRFD
    existing tracks

Step 1:
    filter detections by score and face size

Step 2:
    predict each track's next bbox
    - simple constant velocity prediction
    - or no prediction in first version

Step 3:
    build track-detection candidate pairs
    - IoU gating
    - center distance gating
    - area ratio gating

Step 4:
    associate detections to tracks

Step 5:
    update matched tracks
    - bbox
    - landmarks
    - velocity
    - hits++
    - miss = 0

Step 6:
    mark unmatched tracks
    - miss++
    - if miss > max_miss => Removed
    - else => Lost

Step 7:
    create new tentative tracks from unmatched detections

Step 8:
    promote tentative -> confirmed if hits >= min_confirm_hits
```

### 9.2 第一版是否一定要 Kalman Filter

不一定。

在固定門禁鏡頭、低目標數量場景，第一版其實可以先用：

- `上一幀 bbox`
- `速度 = 本幀中心 - 上幀中心`
- 用簡單 constant velocity 做預測

也就是：

```text
cx_pred = cx + vx
cy_pred = cy + vy
w_pred  = w
h_pred  = h
```

如果之後遇到：

- 快速移動
- 遮擋
- 鏡頭視角變化

再升級到 Kalman Filter 會比較合理。

所以在 RV1106 第一版，我建議：

- **先不上 Kalman**
- 先用輕量 velocity model
- 把算力留給 detection / embedding / liveness

---

## 10. 為什麼 tracking 後還要 alignment

因為 detection 框只告訴你臉在哪裡，不代表眼睛鼻子嘴巴已經對齊。

人臉 embedding 對幾何位置很敏感。
如果同一個人：

- 一次頭往左歪
- 一次鏡頭框偏上
- 一次嘴巴被切掉

embedding 會不穩。

### 10.1 SCRFD 的 5 點 landmarks 剛好可用

如果你的 `SCRFD-500M` 版本輸出 5 點 landmarks，那就非常適合做人臉 alignment：

- left eye
- right eye
- nose
- left mouth corner
- right mouth corner

### 10.2 Alignment 做法

將 5 個 landmarks 對應到一組標準模板點，計算 similarity transform：

- 旋轉
- 縮放
- 平移

把臉 warp 到固定輸出，例如：

- `112 x 112`
- 或 `128 x 128`

這一步能讓 embedding 的可比性大幅提升。

---

## 11. Embedding 在門禁系統中的真正角色

### 11.1 Embedding 不是分類器，而是特徵描述子

門禁系統通常不是把模型訓成「張三 / 李四 / 王五」固定分類。
而是：

1. 每個已註冊人臉先建立 embedding
2. 現場新進來的臉也算一個 embedding
3. 兩者比較距離

這樣好處是：

- 新增人員比較容易
- 不需要整個模型重訓
- 更適合小型資料庫門禁

### 11.2 為什麼 embedding 不能直接對整張 bbox 做

因為 bbox 常常包含：

- 額外背景
- 頭髮
- 耳朵
- 部分肩膀

所以標準流程應該是：

1. `bbox + landmarks`
2. `alignment`
3. `aligned face crop`
4. `embedding`

### 11.3 在 track 裡不必每幀都跑 embedding

推薦策略：

- track 剛 Confirmed 時跑一次
- 如果這一幀 face quality 很好，再更新一次
- 每隔固定幀數補跑一次，防止初始品質太差

這樣比每幀跑更合理。

### 11.4 當資料庫是上萬人時，embedding 會變成系統核心瓶頸之一

當資料庫只有幾十或幾百人時，很多設計都可以比較隨意。
但如果資料庫規模到 `上萬人`，事情會明顯不同：

- 更容易出現相似臉候選
- top-1 與 top-2 分數差距可能變小
- false accept risk 會上升
- 每次查詢的比對成本會變高

所以在大型資料庫場景中，embedding 不只是「算出一個向量」，還要考慮：

- 向量品質是否穩定
- 資料庫索引是否有效率
- 決策是否保守
- 是否需要 coarse-to-fine search

這也是為什麼 tracking 很重要。
因為 tracking 能幫你避免把每一幀的低品質臉都拿去對上萬人搜尋。

---

## 12. Face quality gate：只對值得的幀做 embedding

這一步在嵌入式系統非常重要。

### 12.1 建議品質條件

只有滿足以下條件才跑 embedding：

- face width >= `80 px`
- detection score >= `0.75`
- 人臉不太斜
- landmarks 合理
- blur 分數高於門檻
- bbox 沒貼近畫面邊界

### 12.2 可計算的 quality score

```text
quality =
    a1 * det_score
  + a2 * face_size_score
  + a3 * frontal_score
  + a4 * sharpness_score
  - a5 * edge_penalty
```

其中：

- `face_size_score`: 臉越大越好
- `frontal_score`: 左右眼水平差、鼻尖偏移等估計姿態
- `sharpness_score`: Laplacian variance
- `edge_penalty`: 太靠近邊界扣分

### 12.3 Track 中維護 best shot

對每個 track 記錄目前最佳臉圖：

- `best_crop`
- `best_quality_score`
- `best_embedding`

當新的 quality 更高時再覆蓋。

這樣就算前幾幀比較糊，後面也能提升辨識結果。

---

## 13. DB search 怎麼跟 tracking 配合

### 13.1 建議以 track 為單位辨識，而不是以 frame 為單位辨識

錯誤做法：

- 每幀都產生一個 identity

推薦做法：

- 每個 `track` 維護一個 identity 狀態

例如：

```text
track 7:
    frame 31 -> alice, 0.61
    frame 36 -> alice, 0.72
    frame 41 -> alice, 0.78
```

最後由 temporal smoothing 決定：

- `track 7 == Alice`

### 13.2 資料庫比對方式

常見做法：

- cosine similarity
- L2 distance

若 embedding 模型輸出前已做 L2 normalize，通常 cosine similarity 會較直觀。

### 13.3 每個人資料庫不只存一個 embedding

建議 enrollment 時，每個人至少存：

- 正面
- 微左轉
- 微右轉
- 不同光線

並且每個人保留多個 prototype embedding。

比對時：

- query embedding 與該人所有 prototypes 比
- 取最大相似度

這樣會比每人只存一張穩很多。

### 13.4 上萬人資料庫時，不建議每次都做完全暴力比對

如果每個人存多個 prototype embedding，例如：

- `10,000` 人
- 每人 `5` 個 prototype

那資料庫就會有 `50,000` 個向量。

若每個 confirmed track 都高頻率做全庫比對：

- CPU 會有壓力
- latency 會上升
- unlock 體驗可能變差

因此更建議使用兩階段查詢：

#### 第一階段：粗搜尋

先用較快的方法取出 top-K 候選，例如：

- 向量索引
- ANN search
- 或簡化版 coarse quantization

#### 第二階段：精搜尋

只對 top-K 候選做精確 cosine similarity 排序。

例如：

1. 先從 `50,000` 個 prototype 找 top-20
2. 再對 top-20 做精排
3. 再交給 temporal smoothing 決定是否 accept

### 13.5 大型資料庫時，track 的價值會更高

當資料庫很大時，tracking 的一個額外價值是：

- **減少搜尋次數**

例如同一個人站在門口 2 秒，如果每秒 15 幀：

- 沒 tracking：可能做 30 次查庫
- 有 tracking：可能只做 4 到 6 次高品質查庫

這個差異在上萬人資料庫下非常重要。

### 13.6 大型資料庫下，decision 不應該只看 top-1 分數

更建議同時看：

- top-1 similarity
- top-2 similarity
- top-1 與 top-2 的 margin
- 同一 identity 在多幀中是否一致

例如：

- top-1 = 0.79
- top-2 = 0.78

這種情況即使 top-1 過門檻，也不應輕易開門。
因為候選之間分不開，代表辨識不夠有把握。

---

## 14. Temporal smoothing：真正決定是否開門的關鍵

tracking 和 embedding 做完後，還不能立刻開門。

### 14.1 為什麼不能單幀開門

單幀可能因為：

- 角度剛好像
- 一幀誤判
- 遮擋造成錯判

就觸發誤開門。

### 14.2 建議規則

以 `track` 為單位累積最近 N 次 embedding search 結果：

```text
decision_buffer = [
    (alice, 0.72),
    (alice, 0.75),
    (alice, 0.81),
    (unknown, 0.48),
]
```

開門條件可設為：

1. 最近 `5` 次中至少 `3` 次是同一人
2. 最高分 >= `unlock_threshold`
3. 平均分 >= `avg_threshold`
4. track 狀態為 `Confirmed`
5. 活體檢測通過

例如：

- `unlock_threshold = 0.78`
- `avg_threshold = 0.72`

### 14.3 上萬人資料庫時，temporal smoothing 要更保守

在大型資料庫下，單幀高分更可能是偶然碰撞。
因此建議 decision rule 加上以下條件：

1. 最近多次查詢中，必須是同一個 identity 重複勝出
2. top-1 與 top-2 的平均 margin 必須超過門檻
3. 高品質幀的結果權重要高於低品質幀
4. 若結果在多個身份之間跳動，直接維持 unknown

這能有效降低大庫誤識別造成的誤開門。

---

## 15. 給 RV1106 的實際部署建議

### 15.1 推薦運算分工

#### NPU

- `SCRFD-500M`
- face embedding model
- 如果後續需要，也可放輕量 liveness model

#### CPU

- tracker association
- quality scoring
- alignment 幾何計算
- DB similarity search
- temporal smoothing
- GPIO 控制

這樣分工合理，因為：

- tracking 本身不是重網路，CPU 夠做
- NPU 要保留給 CNN inference

### 15.2 記憶體策略

RV1106G2 只有 `128MB`，要很克制：

- 不要保存太多全尺寸 frame
- track 只保留必要資訊
- aligned face crop 只保留少量最佳圖
- embedding buffer 用固定長度環形佇列

第一版建議：

- 最多同時追蹤 `max_tracks = 8`
- 每個 track 最多保存最近 `5` 次 search result
- 每個人資料庫 prototype 不要一開始就放太多

如果資料庫規模是上萬人，還要額外注意：

- 不一定能把完整索引都舒適地放在極小記憶體配置內
- `RV1106G2 128MB` 版本尤其要小心
- 資料庫很可能需要更緊湊的向量表示，或把查庫放到邊緣伺服器

### 15.3 上萬人資料庫的部署策略

這裡有兩種可行路線。

#### 路線 A：板上全本地查庫

適合：

- prototype 數不算太多
- embedding 維度不高
- 已做量化或壓縮
- 可接受較保守的更新頻率

優點：

- 離線可用
- 不依賴網路

缺點：

- 記憶體與 latency 壓力最大
- 門檻調校更辛苦

#### 路線 B：板上做 detection/tracking/alignment/embedding，查庫放遠端

流程：

1. RV1106 本地完成 face track 與高品質 embedding
2. 只在必要時將 embedding 傳給後端搜尋
3. 後端回傳 top-K 候選與相似度
4. 板端再做 temporal smoothing 與 GPIO decision

優點：

- 更適合上萬人甚至更大資料庫
- 可以用更成熟的 ANN / vector DB
- 更容易持續更新資料庫

缺點：

- 依賴網路
- 要處理延遲與安全傳輸

### 15.4 我對大型資料庫的實務建議

若目標真的是穩定支援 `上萬人`，我更傾向：

- `RV1106` 做前端感知與節流
- 資料庫搜尋交由上位機或伺服器

也就是：

- 本地做 `SCRFD + tracking + alignment + embedding + smoothing`
- 遠端做 `large-scale search`

這樣最符合平台能力邊界。

### 15.5 影像解析度建議

門禁第一版不必直接跑高解析全畫面辨識。

較合理策略：

- camera capture: `640x480` 或 `720p`
- detection input: 視模型需求 resize
- embedding crop: `112x112`

如果門口距離固定、視野小，`640x480` 常常就夠了。

---

## 16. 推薦第一版 tracking 設計參數

### 16.1 Detection filtering

```text
det_score_thresh = 0.55
min_face_size    = 50 px
max_face_size    = image_width * 0.8
```

### 16.2 Track lifecycle

```text
min_confirm_hits = 3
max_miss         = 8
max_tracks       = 8
```

### 16.3 Association

```text
iou_min          = 0.20
center_gate      = 0.60 * diag(track_box)
area_ratio_min   = 0.5
area_ratio_max   = 2.0
match_cost_max   = 0.85
```

### 16.4 Embedding trigger

```text
embed_score_thresh     = 0.75
embed_min_face_width   = 80 px
embed_period_frames    = 5
force_embed_frames     = 15
```

### 16.5 Unlock

```text
same_id_votes_needed   = 3 / 5
unlock_threshold       = 0.78
avg_threshold          = 0.72
cooldown_after_unlock  = 2 sec
```

以上都只是第一版起始值，後面一定要靠實測調整。

### 16.6 大型資料庫下額外 decision 參數

若資料庫達上萬人，建議再加入：

```text
top1_top2_margin_min     = 0.03 ~ 0.08
min_consistent_hits      = 3
search_topk              = 20
high_quality_vote_weight = 1.5
low_quality_vote_weight  = 0.5
```

這些參數的作用是避免：

- top-1 只是勉強贏
- 同一 track 中 identity 一直跳
- 低品質幀把高品質決策沖掉

---

## 17. 建議的偽程式

```text
for each frame:
    frame = capture()

    detections = scrfd_detect(frame)
    detections = filter_by_score_and_size(detections)

    predict_tracks(tracks)

    matches, unmatched_tracks, unmatched_dets = associate(tracks, detections)

    for (track, det) in matches:
        update_track(track, det)

        if should_run_embedding(track, det):
            aligned = align_face(frame, det.bbox, det.landmarks5)
            quality = eval_face_quality(aligned, det)

            if quality >= quality_thresh:
                emb = run_embedding(aligned)
                person_id, sim = search_db(emb)
                update_identity(track, person_id, sim, emb, quality)

    for track in unmatched_tracks:
        mark_missed(track)

    for det in unmatched_dets:
        create_tentative_track(det)

    remove_dead_tracks(tracks)

    for track in confirmed_tracks:
        if liveness_ok(track) and unlock_rule_passed(track):
            gpio_unlock()
            set_unlock_cooldown()
```

---

## 18. 這個設計比「每幀都 embedding」好在哪裡

### 18.1 好處

- 算力比較省
- identity 比較穩
- 可以容忍短暫漏檢
- 更容易做 temporal smoothing
- 比較符合門禁「決策要保守」的需求
- 在上萬人資料庫下可以大幅減少查庫壓力

### 18.2 代價

- 系統複雜度會比純 detection 高
- 要維護 track 狀態
- 要調參
- 要處理 track birth / death / reappear

但這個代價是值得的，因為門禁系統最怕：

- 誤開門
- 明明是本人卻一直辨識失敗
- 畫面稍微晃動就身份跳來跳去

tracking 正是拿來解決這些工程問題。

---

## 19. 如果我來做第一版，我會這樣拆任務

### Step A. 先打通最小閉環

1. camera capture
2. SCRFD detection
3. 繪框與 landmarks 顯示
4. 建立單人追蹤版 tracker
5. alignment
6. embedding
7. DB search
8. console 印出辨識結果

### Step B. 擴成可用版

1. 多 track 管理
2. quality gate
3. temporal smoothing
4. GPIO unlock
5. cooldown
6. enrollment tool

### Step C. 最後再補安全性

1. threshold tuning
2. anti-spoof / liveness
3. edge cases
4. log 與故障回報

這個順序很重要。
不要一開始就把所有功能一起上，否則很難 debug。

---

## 20. 你目前最該抓住的核心觀念

### 20.1 detection 只回答「哪裡有臉」

不是誰，也不是是不是同一個人。

### 20.2 tracking 先把「同一個畫面中的同一張臉」串起來

這會讓後面的 alignment、embedding、DB search 都穩定很多。

### 20.3 embedding 才是「這個人是誰」的特徵表示

它是拿來跟資料庫比，不是拿來單獨做整個門禁決策。

### 20.4 開門決策應該以 track 為單位，不應該以單幀為單位

真正安全的門禁一定要做 temporal smoothing。

### 20.5 當資料庫上萬人時，tracking 的價值不只是穩定，還包括節省搜尋與降低誤識別

大型資料庫會把錯誤匹配機率放大。
因此 tracking 在這裡不只是輔助模組，而是整個系統可擴展性的關鍵元件。

---

## 21. 建議的第一版實作選型

如果要在 `RV1106G2 / RV1106G3` 上快速做出能跑的版本，我建議：

- Detection: `SCRFD-500M`
- Tracking: 自製 `Face-SORT-Lite`
- Alignment: 直接使用 `SCRFD 5 landmarks`
- Embedding: 輕量 face recognition model
- Search: cosine similarity on CPU
- Decision: majority vote + threshold
- Unlock: GPIO relay with cooldown

這條路線的好處是：

- 結構清楚
- 模組容易分開驗證
- 很符合嵌入式平台資源限制
- 後續也容易再加 liveness

---

## 22. 參考資料

以下資料用來確認 RV1106 / Luckfox 平台能力與官方建置方式：

- Luckfox Pico RV1106 Wiki
  https://wiki.luckfox.com/Luckfox-Pico-RV1106/
- Rockchip RV1106 Datasheet Rev 1.3
  https://files.luckfox.com/wiki/Luckfox-Pico/PDF/Rockchip%20RV1106%20Datasheet%20V1.3-20230522.pdf
- Luckfox RKNN Wiki
  https://wiki.luckfox.com/Luckfox-Pico-Pro-Max/RKNN/
- Luckfox Core1106 product page
  https://www.luckfox.com/Luckfox-Pico-86-Panel

---

## 23. 下一步建議

下一份文件最值得寫的是二選一：

1. `tracker 資料結構 + C/C++ 模組介面設計`
2. `SCRFD -> alignment -> embedding -> DB search` 的實作流程圖與偽碼

如果你要，我下一步可以直接幫你把這份文件再往下變成：

- 系統架構圖
- C++ 類別設計
- 或可直接開發的 task breakdown
