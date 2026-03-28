# RV1106 + Luckfox 人臉門禁 Tracking 技術評估

## 1. 文件目的

這份文件專門評估人臉門禁系統中的 `tracking` 技術選型，重點不是學術 benchmark，而是：

- 在 `RV1106G2 / RV1106G3 + Luckfox + camera` 上能不能跑
- 對門禁場景是否足夠穩定
- 和後續 `alignment / embedding / DB search / unlock` 的整合成本
- 是否值得引入 `Kalman Filter` 或 `EKF`

本文假設：

- 偵測器已固定為 `SCRFD-500M`
- 鏡頭多半固定
- 門口同時出現的人數通常很少
- 已註冊人臉資料庫可能達到 `上萬人`
- 主要目標是「穩定辨識和保守開門」，不是做複雜多目標追蹤競賽

---

## 2. 先說結論

如果是第一版可落地實作，我建議 tracking 採用：

- `IoU + center-distance gating + simple velocity model`
- 必要時再升級成 `IoU + Kalman Filter`
- **先不要上 EKF**

原因很直接：

1. 門禁是固定場景，運動模型通常很簡單
2. 人臉數量少，資料關聯問題沒有一般 MOT 那麼難
3. 真正決定辨識品質的是 `alignment + embedding quality + temporal smoothing`
4. 大型資料庫時，tracking 更重要，因為它能減少重複查庫與 identity 跳動
5. EKF 的非線性建模成本高，但在這個場景下通常回報不大

所以如果你問：

- `IoU only` 可不可以？可以，但遮擋和抖動容忍度較差
- `IoU + KF` 值不值得？值得，是最平衡的升級
- `IoU + EKF` 值不值得？大多數門禁案子不值得

---

## 3. Tracking 在門禁系統中的真正工作

很多人會把 tracking 想成「很高級的 AI」。
但在人臉門禁裡，它其實主要做三件事：

### 3.1 維持同一張臉的短期連續性

把連續幾幀的同一個人串成一條 track：

- frame 101 的 bbox
- frame 102 的 bbox
- frame 103 的 bbox

都知道是同一位訪客。

### 3.2 降低 detection 抖動對辨識的影響

即使 `SCRFD` 已經很穩，bbox 還是會有：

- 小幅跳動
- 偶發漏檢
- 框大小變化

tracking 可以把這些抖動吸收掉，讓後面的 face crop 與 identity 決策更穩。

### 3.3 控制 embedding 觸發節奏

如果沒有 tracking，你很容易每一幀都做 embedding。
有 tracking 之後，你就可以：

- 只對 confirmed track 做 embedding
- 只在高品質幀做 embedding
- 只對同一個 track 累積決策

所以 tracking 的價值不只是「跟住框」，也是整個辨識決策的節流器。

### 3.4 在上萬人資料庫場景下，tracking 還負責控制搜尋成本

如果資料庫規模到 `上萬人`，每一次 embedding 查詢的系統代價都會變高。
這時 tracking 額外提供三個重要價值：

- 避免每幀都查庫
- 讓同一條 track 共用多次搜尋結果
- 讓 temporal smoothing 建立在同一個穩定 track 上

換句話說，大型資料庫下 tracking 不只是幫你追框，而是在保護：

- latency
- CPU / NPU 使用率
- false accept risk

---

## 4. RV1106 平台的設計限制

在這個平台選 tracking，不應該只看精度，也要看整體資源。

### 4.1 已知平台條件

根據 Luckfox / Rockchip 公開資料：

- `RV1106G2`: 約 `0.5 TOPS`, `128MB DDR3L`
- `RV1106G3`: 約 `1 TOPS`, `256MB DDR3L`
- 單核 `Cortex-A7`
- 內建 ISP
- 適合做 camera + NPU inference + 輕量 CPU 邏輯

### 4.2 對 tracking 的直接影響

這表示 tracking 最好具備以下特性：

- CPU 成本低
- 記憶體佔用小
- 狀態量簡單
- 容易除錯
- 不依賴大型外觀特徵網路

所以一開始不建議：

- DeepSORT 那種每幀都跑額外 re-id 模型
- 複雜 particle filter
- 需要長歷史序列的 heavy temporal model

在這個板子上，tracking 最好像「控制邏輯」而不是另一個大模型。

若再疊加「上萬人資料庫」這個條件，這個結論只會更強。
因為系統已經有其他重點負擔來自：

- 向量搜尋
- 候選排序
- 門檻判定

所以 tracking 本身更應該保持輕量與可控。

---

## 5. 可選 tracking 路線總覽

這裡把可能的設計從輕到重排開。

### 5.1 Option A: 純 IoU matching

每一幀：

1. 將本幀 detections 與上一幀 tracks 比較 IoU
2. IoU 最大者配對
3. 配不到就建立新 track 或標記 miss

### 5.2 Option B: IoU + 幾何 gating + velocity model

除了 IoU，還加：

- 中心距離限制
- 面積比例限制
- 用上一幀速度做簡單預測

這是比純 IoU 更穩、仍非常輕量的方案。

### 5.3 Option C: IoU + Kalman Filter

每條 track 維護狀態：

- 中心位置
- 寬高或尺度
- 速度

用 Kalman Filter 做：

- prediction
- update

再用 IoU 或 Mahalanobis distance 做關聯。

### 5.4 Option D: IoU + EKF

如果你想把狀態或量測模型設成非線性，例如：

- 尺度變化用非線性表示
- 用角度、深度或投影模型

可以考慮 EKF。

### 5.5 Option E: Kalman + 外觀特徵追蹤

除了幾何，還引入 embedding 或 face descriptor 幫助 association。

這通常更穩，但也更貴。

---

## 6. 純 IoU matching 的深入評估

### 6.1 優點

- 實作最簡單
- CPU 幾乎沒負擔
- 易於 debug
- 對固定鏡頭、低人數場景可用

### 6.2 缺點

- 對 bbox 抖動敏感
- 若本幀漏檢，下一幀容易換 ID
- 目標移動稍快時 IoU 可能掉太多
- 兩張臉靠近時容易配錯

### 6.3 什麼情況下純 IoU 就夠

如果同時滿足以下條件，純 IoU 可能夠用：

- 通常一次只會有 1 張臉
- 人員移動速度不快
- 鏡頭和站位固定
- 偵測器很穩
- 你只求最小可用版本

### 6.4 風險

在門禁情境中，純 IoU 最大風險不是框飄，而是：

- track ID 不穩
- 導致同一個人被當成多個 track
- temporal smoothing 被切斷
- embedding 歷史被重置

這會讓辨識和開門決策變差。

### 6.5 結論

`IoU only` 可以作為 bring-up 版本，但不建議停在這裡。

---

## 7. IoU + 幾何 gating + velocity model 的深入評估

這是我最推薦的第一版。

### 7.1 核心想法

不是只看「重疊多少」，還看：

- 這個 detection 是否在合理位置
- 尺寸變化是否合理
- 是否符合上一幀移動趨勢

### 7.2 成本函數

可用以下形式：

```text
cost =
    w_iou * (1 - IoU)
  + w_center * normalized_center_distance
  + w_scale * abs(log(area_det / area_track))
```

搭配 gating：

- `IoU >= 0.2`
- `center_distance <= 0.6 * diag(track_bbox)`
- `0.5 <= area_ratio <= 2.0`

### 7.3 優點

- 比純 IoU 穩很多
- 幾乎不增加計算負擔
- 好實作、好調參
- 對 RV1106 很友善

### 7.4 缺點

- 預測能力有限
- 遮擋稍長時仍可能斷 track
- 若目標運動不規則，效果不如 KF

### 7.5 適用性結論

如果你想要：

- 先做出穩定可用系統
- 保持系統簡單
- 後續再逐步升級

那這是最好的起點。

---

## 8. IoU + Kalman Filter 的深入評估

這就是你提到的那條路線，也是很常見、而且非常合理的選擇。

### 8.1 為什麼 Kalman Filter 合適

門禁鏡頭通常固定，臉在畫面中的移動是平滑的，短時間內可近似為線性動態：

- 位置逐步變化
- 尺寸逐步變化
- 不會瞬間亂跳

Kalman Filter 特別適合這種：

- 狀態近似線性
- 量測有雜訊
- 每幀都會收到新 observation

的場景。

### 8.2 KF 能解決什麼

#### 平滑 bbox 抖動

偵測器輸出的框帶噪聲，KF 可讓估計位置更平滑。

#### 漏檢容忍

某幀沒 detection 時，KF 仍可用 prediction 繼續估計短期位置。

#### 改善 association

比起拿上一幀原始 bbox 去匹配，本幀用預測 bbox 做匹配更穩。

### 8.3 常見狀態設計

可以使用簡化 SORT 風格狀態：

```text
x = [cx, cy, s, r, vx, vy, vs]
```

其中：

- `cx, cy`: 中心
- `s`: 面積或尺度
- `r`: 長寬比
- `vx, vy, vs`: 速度

但對人臉門禁，我更建議直接用比較直觀的版本：

```text
x = [cx, cy, w, h, vx, vy, vw, vh]
```

原因：

- 人臉框寬高較容易直接理解
- debug 時更直觀
- 實務調參方便

### 8.4 量測向量

由 `SCRFD` 直接提供：

```text
z = [cx, cy, w, h]
```

### 8.5 系統模型

可用 constant velocity model：

```text
cx_k = cx_{k-1} + vx_{k-1}
cy_k = cy_{k-1} + vy_{k-1}
w_k  = w_{k-1}  + vw_{k-1}
h_k  = h_{k-1}  + vh_{k-1}
```

### 8.6 關聯方式

KF 常見兩種搭法：

1. 用預測 bbox 與 detection bbox 算 IoU
2. 用 Mahalanobis distance 做 gating，再用 IoU 輔助

對第一版門禁，我建議：

- 先用 `predicted bbox + IoU + center gating`

因為：

- 直觀
- 簡單
- 已足夠

### 8.7 優點

- 比 velocity model 更系統化
- 遮擋與漏檢容忍較好
- 追蹤框更平滑
- 工程上成熟，資料多，容易落地

### 8.8 缺點

- 比純幾何版稍複雜
- 要調 `Q` 和 `R`
- 若偵測更新頻率不穩，參數要注意

### 8.9 在 RV1106 上的成本

很低。

Kalman Filter 的矩陣維度很小，CPU 成本相對整個系統幾乎可忽略。
真正的成本不在運算量，而在：

- 程式碼複雜度
- 調參
- 邊界情況驗證

### 8.10 結論

`IoU + KF` 是非常值得認真考慮的主力方案。
如果你希望第一版就有較好的穩定度，這會比純 velocity model 更安心。

### 8.11 為什麼大型資料庫條件下更推薦 KF

當資料庫是上萬人時，track 斷裂的代價會更高，因為：

- 同一個人被拆成多條 track
- 會觸發更多次 embedding
- 會觸發更多次 DB search
- 同一個人在不同幀可能命中不同候選身份

因此在大型資料庫場景中，`KF` 的價值不只是框更平滑，而是：

- 延長 track 穩定性
- 降低重複查庫
- 提升多幀 identity 決策的一致性

---

## 9. IoU + EKF 的深入評估

### 9.1 EKF 什麼時候才需要

EKF 適合：

- 狀態轉移是非線性
- 量測模型是非線性
- 線性近似不夠用

例如：

- 雷達量測是極座標
- 相機投影模型需要非線性映射
- 你要估深度、姿態、角度等非線性狀態

### 9.2 門禁人臉框追蹤有必要嗎

大多數情況下沒有。

因為你目前追的是：

- 2D bbox
- 2D 中心位置
- 寬高變化

這些在短時間內用線性模型通常就足夠。

### 9.3 EKF 的理論優勢

如果你真的想追：

- 頭部姿態角
- 透視縮放
- 由臉框推估距離

那 EKF 可以比普通 KF 更自然。

### 9.4 EKF 的實務缺點

- 實作更複雜
- 要自己推 Jacobian
- 除錯困難
- 模型錯了反而更差
- 在資料量不大時不一定比 KF 好

### 9.5 RV1106 上值不值得

從算力角度看，不是完全不能跑。
但從整體工程投資報酬率來看，通常不值得。

因為你的問題通常不在：

- 線性模型不夠好

而在：

- detection quality
- face alignment quality
- embedding threshold
- temporal smoothing 規則
- 活體判定

### 9.6 結論

`EKF` 可以做，但不該是這個專案的優先項。
在門禁第一版與第二版，我都不建議把時間花在這裡。

---

## 10. 是否需要把 embedding 納入 tracking association

這是一個很常被問到的問題。

### 10.1 納入 embedding 的好處

- 兩張臉靠很近時比較不容易換 ID
- 短暫遮擋後 re-association 較強

### 10.2 壞處

- 要額外算 embedding
- quality 差時外觀特徵不穩
- 系統變複雜
- 會把 tracking 和 recognition 耦合得太緊

### 10.3 在門禁第一版的建議

不要把 embedding 當每幀 association 主特徵。
比較好的做法是：

- tracking 主體仍用幾何方法
- embedding 用於 identity 決策
- 只有在少數需要時，才用 embedding 幫助 re-association

例如：

- 某條 track Lost 了 3 幀後重現
- 同時幾何資訊不夠明確
- 才用最近一次高品質 embedding 當輔助

這樣能兼顧穩定與簡潔。

### 10.4 大型資料庫下，更要避免把 tracking 和查庫硬綁在一起

如果資料庫上萬人，一個很危險的設計是：

- 為了幫 tracking association
- 幾乎每幀都跑 embedding
- 再對大庫做搜尋

這樣會同時造成：

- NPU 負擔上升
- 查庫 latency 上升
- decision 噪聲上升

更合理的策略仍然是：

- tracking 先用幾何方法完成
- embedding 有條件觸發
- 大庫搜尋以 track 為單位節流

---

## 11. 多種方案的工程比較

### 11.1 比較表

| 方案 | CPU 成本 | 實作難度 | 抖動抑制 | 漏檢容忍 | 適合 RV1106 | 推薦程度 |
|---|---:|---:|---:|---:|---:|---:|
| IoU only | 很低 | 很低 | 低 | 低 | 高 | 可做 bring-up |
| IoU + velocity | 很低 | 低 | 中 | 中 | 很高 | 很推薦 |
| IoU + KF | 低 | 中 | 高 | 高 | 很高 | 最推薦 |
| IoU + EKF | 中 | 高 | 高 | 高 | 中 | 不建議先做 |
| KF + appearance | 中到高 | 高 | 高 | 很高 | 中 | 第二階段再考慮 |

### 11.2 讀表方式

如果你的優先順序是：

- 趕快做出能跑版本

選：

- `IoU + velocity`

如果你的優先順序是：

- 第一版就希望 ID 更穩、漏檢容忍更好

選：

- `IoU + KF`

如果你的優先順序是：

- 想研究更漂亮的狀態估計

才考慮：

- `EKF`

---

## 12. 我對 IoU + KF 的具體建議

如果你想採用 `IoU + Kalman Filter`，我建議如下。

### 12.1 Track state

```text
state x = [cx, cy, w, h, vx, vy, vw, vh]
measurement z = [cx, cy, w, h]
```

### 12.2 Prediction

使用 constant velocity model。

### 12.3 Association

先 gating，再做 assignment：

1. Mahalanobis distance gating 或 center-distance gating
2. IoU gating
3. 綜合 cost 後做 greedy / Hungarian

第一版你甚至可以先不用 Mahalanobis，只用：

- KF 預測 bbox
- IoU + 中心距離

### 12.4 Track lifecycle

- `Tentative`
- `Confirmed`
- `Lost`
- `Removed`

建議：

- `min_confirm_hits = 3`
- `max_miss = 8`

### 12.5 什麼時候 update KF

每當 detection 成功匹配時，用 detection update。
若某幀沒有匹配：

- 只做 predict
- `miss += 1`

### 12.6 不要讓 KF 過度支配

有一個常見錯誤是太相信 prediction。
在人臉門禁裡，tracking 的主觀測仍然是偵測器。

所以建議：

- `SCRFD detection` 為主
- `KF` 是平滑與短期預測工具
- 不要在長時間無量測下持續相信 KF

超過 `max_miss` 就移除。

---

## 13. 如果真的考慮 EKF，哪些情況才合理

### 13.1 你要把 head pose 納入狀態

例如你想追：

- yaw
- pitch
- roll

並用它預測下一幀臉框投影。

### 13.2 你要估計人臉距離或深度

若你想由臉框尺寸和透視關係估計距離，模型可能更非線性。

### 13.3 你要和 PTZ / 移動鏡頭結合

如果攝影機本身也在移動，狀態模型會複雜很多。

但目前你的條件是 Luckfox 門禁板 + 固定鏡頭，通常不屬於這類需求。

---

## 14. 追蹤評估時應該看哪些技術指標

不要只看 MOT 指標，門禁有自己更重要的指標。

### 14.1 Tracking 層指標

- `ID switch count`
- `track fragmentation`
- `average confirmed track length`
- `recovery after short miss`
- `false track creation count`

### 14.2 Recognition 層影響

tracking 的好壞最後會反映到：

- 單次進場的平均 embedding 次數
- 同一人 identity 是否穩定
- unknown 是否過多
- false accept / false reject 是否增加

### 14.3 系統層指標

- 平均 unlock latency
- NPU 利用率
- CPU 利用率
- 平均每秒 embedding 次數
- GPIO 誤觸發次數

### 14.4 上萬人資料庫下必加的指標

- 每條 confirmed track 的平均查庫次數
- 每次查庫 latency
- top-1 / top-2 margin 分布
- 同一 track 中 identity 一致率
- 大型資料庫下的 false accept rate

### 14.5 門禁應設定同時追蹤邊界

對固定單入口門禁，我建議一開始就定義：

- 同時有效追蹤上限約 `3 ~ 5`

其中較合理的預設值是：

- `5`

如果你已經確定：

- 鏡頭視野窄
- 使用者會主動靠近設備操作
- 不希望背景路人進入流程

那麼：

- `3`

會更保守，也更像真正的門禁設備。

如果同時追蹤超過 `5` 常常發生，通常表示真正該優先調整的是：

- ROI
- 鏡頭視角
- 安裝角度
- 操作距離

而不是一味把 `max_tracks` 拉高。

對門禁來說，tracking 的最佳評估方式不是只有「框有沒有跟到」，而是：

- 它有沒有讓最終開門決策更穩

---

## 15. 建議測試情境

你可以用這些情境來驗證不同 tracking 方案。

### 15.1 單人正面接近

最基本情況，確認 track 是否平穩。

### 15.2 單人停留 2 到 3 秒

觀察是否有重複建 track。

### 15.3 單人快速經過

測試 IoU only 是否容易斷 track。

### 15.4 短暫遮擋

例如手遮臉、轉頭、被門框擋到。

### 15.5 兩人交錯

測試是否容易 ID switch。

### 15.6 臉靠近畫面邊界

測試 track 進出畫面的穩定性。

### 15.7 光線變差

看 detection 抖動時 KF 是否比純 IoU 穩。

---

## 16. 我的技術建議

### 16.1 第一階段

直接實作：

- `IoU + center-distance + area-ratio + simple velocity`

目的：

- 先把整個 tracking 架構與 lifecycle 跑起來
- 快速看到系統行為
- 容易 debug

### 16.2 第二階段

若發現以下問題：

- bbox 太抖
- 短暫漏檢時 track 常斷
- 進出畫面時 ID 不穩

就升級到：

- `IoU + Kalman Filter`

如果你已經知道系統目標是支援 `上萬人資料庫`，那我會更傾向直接把 `IoU + KF` 當成第一個正式版本。

### 16.3 第三階段

只有在以下情況才考慮更複雜方案：

- 同時多人很多
- 遮擋頻繁
- 頻繁交錯
- 要做更長距離 re-association

這時才考慮：

- `KF + appearance`

### 16.4 不建議的路線

在目前這個專案階段，不建議：

- 一開始就上 EKF
- 一開始就上重型 multi-object tracker
- 一開始就把 tracking 與 recognition 深度綁死

---

## 17. 最終建議

如果今天要我幫這個專案拍板 tracking 技術，我會這樣選：

### 推薦主方案

- `IoU + Kalman Filter`
- 搭配 `center-distance gating`
- 搭配 `area-ratio gating`
- track lifecycle 用 `Tentative / Confirmed / Lost / Removed`

### 推薦原因

- 對 RV1106 仍然夠輕
- 比純 IoU 更能抗抖動與短暫漏檢
- 比 EKF 簡單很多
- 與後續 embedding / smoothing 非常好整合
- 在上萬人資料庫下更能減少重複查庫與身份跳動

### 若你想更快起步

可以先做：

- `IoU + simple velocity`

等系統通了，再換成 KF。
這會是一條非常健康的工程路徑。

---

## 18. 下一步建議

最適合接下來做的文件有兩種：

1. `IoU + Kalman Filter` 的數學模型與參數設計文件
2. `tracking 模組 C/C++ 介面 + 資料結構設計`

如果你要，我下一步可以直接幫你生成：

- 一份專門的 `KF tracking spec`
- 或直接寫成 `C++ tracker skeleton`
