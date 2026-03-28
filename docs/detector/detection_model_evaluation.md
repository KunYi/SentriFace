# Detection Model Evaluation

## 1. 目的

這份文件評估本專案的人臉偵測模型選型，重點問題是：

- 是否應採用 `SCRFD-500M`
- 若不採用，常見替代方案有哪些
- 在 `RV1106 + RKNN + SC3336 + 門禁` 這個條件下，哪個選擇最合理

本文件不是追求學術最強，而是追求：

- 能部署
- 能維護
- 能與 `tracking / alignment / embedding / decision` 穩定整合

---

## 2. 本專案的選型前提

在這個專案中，偵測模型必須同時滿足以下條件：

1. 能在 `RV1106G2 / RV1106G3` 上部署
2. 盡量適合 `RKNN` 推論
3. 需要支援門禁近距離場景
4. 最好直接提供 `5 landmarks`
5. 必須與後續 `alignment` 流程自然銜接
6. 不應過度吃掉 NPU/DDR，影響 embedding 與整體延遲

也就是說，這裡的最佳模型不是「理論精度最高」，而是：

- **精度、速度、landmark、部署成本之間最平衡的模型**

---

## 3. 為什麼 detection 在門禁裡很關鍵

人臉 detection 不只是找框。
它還直接影響：

- tracking 是否穩
- alignment 是否可靠
- embedding 品質是否穩定
- 查庫頻率是否過高
- 最終門禁誤開率

如果 detection 框與 landmarks 不穩，後面所有模組都會被拖累。

---

## 4. 候選方案

這裡先評估幾個最有代表性的方向：

1. `SCRFD-500M`
2. `RetinaFace`
3. `BlazeFace`
4. `YuNet`
5. `S3FD`

---

## 5. SCRFD-500M 評估

### 5.1 基本定位

`SCRFD` 來自 InsightFace 團隊，核心目標就是提供不同算力區間下效率與精度平衡較好的 face detector。
從論文與官方頁面的描述來看，這個家族本來就是為了：

- 在不同 compute budget 下找出較佳 efficiency-accuracy trade-off

這點非常符合本專案。

### 5.2 優點

#### 優點 1：非常符合「受限算力下的人臉偵測」

`SCRFD` 不是單一超重模型，而是一個針對不同算力區間設計的家族。
這很適合 `RV1106 + RKNN`，因為我們本來就需要：

- 根據板端算力做取捨

#### 優點 2：與 InsightFace 生態整合自然

如果後續 embedding 也走 InsightFace 系列思路，`SCRFD + face recognition pipeline` 的整合路徑相對自然。

#### 優點 3：可直接支援 landmarks 流程

本專案後續 alignment 依賴 `5 landmarks`。
`SCRFD` 路線在工程上很容易和這一步串起來。

#### 優點 4：比重型 detector 更接近板端可部署現實

對門禁這種固定視角、近距離、臉數少的場景，我們不需要一個為複雜監控多尺度極限表現而付出巨大成本的模型。
`SCRFD-500M` 這類較輕量設定更符合實際。

### 5.3 缺點

#### 缺點 1：仍需要實際驗證 RKNN 轉換與板端延遲

即使模型家族方向正確，也不能直接假設：

- `SCRFD-500M` 在 `RV1106G3` 上就一定達標

還是要驗證：

- RKNN 轉換後是否穩定
- 真實 FPS
- NPU 佔用
- landmarks 品質

#### 缺點 2：若場景極簡，可能仍比超輕量方案重

如果最後門禁場景被收斂得非常單純，例如：

- 臉幾乎永遠都很大
- 只有單人近距離
- 光線非常受控

那 `SCRFD-500M` 可能不是最極致省算力的選擇。

### 5.4 對本專案的結論

`SCRFD-500M` 是目前最合理的主選方案。
原因不是它「理論最強」，而是它最符合我們現在的綜合需求：

- 有效率
- 夠成熟
- 能接 landmarks
- 適合後續 alignment
- 與門禁 pipeline 的相容性高

### 5.5 對目前實際場景的補充建議

如果目前實際門禁場景更接近：

- 臉幾乎都很大
- 通常只有單人近距離
- 但光線**不一定穩定**

那麼我仍然建議：

- **主線先維持 `SCRFD-500M`**

原因是前兩項條件確實會把選型往更輕量推，但第三項：

- 光線不穩定

會把選型重新拉回「穩定性優先」。
對門禁而言，比起極限省算力，更重要的是：

- 不要在亮暗變化時漏檢
- landmarks 不要飄太大
- 後續 alignment / embedding 要保持穩定

因此在這個實際條件下，`SCRFD-500M` 仍然是比較保守且合理的主方案。

### 5.6 如果 RKNN 實測後仍不夠

若後續在 `RV1106G3 + RKNN` 上實測發現：

- FPS 不足
- NPU 佔用過高
- 已明顯壓縮到 embedding / decision 的可用算力

那麼建議的降階順序是：

1. 先試更小的 `SCRFD` family 變體
2. 再評估 `BlazeFace`
3. 最後才考慮為了極限輕量而大幅改變整體 detection 路線

這樣做的好處是：

- 優先保留 `SCRFD` 路線的 landmarks 與整體 pipeline 相容性
- 避免過早切換到和既有設計差異過大的 detector
- 讓降階過程更可控

---

## 6. RetinaFace 評估

### 6.1 基本定位

`RetinaFace` 是非常經典、很強的人臉偵測方法，強項在：

- 人臉定位
- landmark 預測
- 對複雜場景與小臉的表現

### 6.2 優點

- 精度強
- landmarks 路線成熟
- 學術與工程社群都很熟悉

### 6.3 缺點

- 整體通常比 `SCRFD-500M` 更重
- 對 `RV1106 + RKNN` 這種板端算力環境不一定划算
- 對門禁近距離場景而言，部分能力可能超過需求

### 6.4 對本專案的結論

`RetinaFace` 比較像：

- 能當強基準
- 能當參考上限

但不太像第一版最務實的板端主方案。

---

## 7. BlazeFace 評估

### 7.1 基本定位

`BlazeFace` 主要是為 mobile GPU 極低延遲而設計，非常強調：

- 快
- 輕

### 7.2 優點

- 非常輕量
- 在極低延遲場景有吸引力
- 若只求快速檢出大臉，可能很划算

### 7.3 缺點

- 相較 `SCRFD / RetinaFace`，在門禁辨識系統中未必是最佳整合選擇
- landmarks 與後續 alignment/recognition 的整合彈性通常不是它最主要賣點
- 其設計背景偏 mobile GPU，不等於就自然適合 `RV1106 + RKNN`

### 7.4 對本專案的結論

`BlazeFace` 可以當超輕量備選，但比較像：

- 若最後發現 `SCRFD-500M` 對板端仍偏重
- 才值得作為降階方案重新評估

它不是目前最適合直接定為主線的選擇。

---

## 8. YuNet 評估

### 8.1 基本定位

`YuNet` 是 OpenCV 生態中很實用的輕量 face detector，工程整合門檻低。

### 8.2 優點

- 輕量
- 易於在 CPU 上快速驗證
- 工程入門成本低

### 8.3 缺點

- 對本專案來說，最大問題不是能不能跑，而是：
  是否能在 `RV1106 + RKNN` 上提供足夠好的整體表現與路線延續性
- 若後續主線要走 RKNN/NPU，人臉辨識全鏈路仍需考慮一致性

### 8.4 對本專案的結論

`YuNet` 比較適合：

- CPU-first 快速驗證
- 做簡易 baseline

但不是目前最理想的板端正式方案。

---

## 9. S3FD 評估

### 9.1 基本定位

`S3FD` 是較早期但很有代表性的人臉偵測方法，對小臉很強。

### 9.2 優點

- 小臉能力強
- 是重要經典方法

### 9.3 缺點

- 對今天這個門禁場景來說，整體工程優先級不高
- 與現在偏輕量、邊緣部署導向的需求不完全一致

### 9.4 對本專案的結論

比較適合作為歷史參考，不是目前主選。

---

## 10. 工程比較總結

| 方案 | 精度潛力 | 輕量性 | landmarks 整合 | RKNN/板端適配直覺 | 對門禁適配 | 建議角色 |
|---|---:|---:|---:|---:|---:|---:|
| SCRFD-500M | 高 | 高 | 高 | 高 | 高 | 主選 |
| RetinaFace | 很高 | 中低 | 高 | 中 | 中高 | 參考上限/備選 |
| BlazeFace | 中 | 很高 | 中 | 中 | 中 | 超輕量備選 |
| YuNet | 中 | 高 | 中 | 中 | 中 | CPU baseline |
| S3FD | 高 | 低 | 中 | 低 | 低中 | 歷史參考 |

上表不是學術排名，而是：

- **針對本專案條件的工程排序**

---

## 11. 我的建議

### 11.1 主方案

目前建議維持：

- `SCRFD-500M`

作為本專案主選 detection 方案。

### 11.2 備選方案

若後續實測發現 `SCRFD-500M` 在 `RV1106G3 + RKNN` 上仍不夠好，可依序考慮：

1. 更輕量的 `SCRFD` family 變體
2. `BlazeFace`
3. `YuNet` 作為 CPU-only baseline

### 11.3 不建議目前就切到重型方案

目前不建議主線直接改成：

- `RetinaFace`
- `S3FD`

因為它們比較可能讓你過早掉進：

- 模型太重
- 板端 latency 太高
- NPU/DDR 壓力過大

的問題。

---

## 12. 建議的驗證順序

### Step 1. 維持主線為 SCRFD-500M

先把整體 pipeline 做通。

### Step 2. 在 CPU-first 環境完成全鏈路驗證

確認：

- detection I/O
- landmarks
- tracking
- alignment
- embedding

串接合理。

### Step 3. 在 RV1106G3 + RKNN 做實測

重點記錄：

- 實際 FPS
- NPU 利用率
- landmarks 穩定性
- 是否壓縮到 embedding 與 decision 的可用算力

### Step 4. 若不達標，再做降階比較

此時再評估：

- 更小的 `SCRFD`
- `BlazeFace`
- 其他更輕量替代方案

---

## 13. 最終結論

對這個專案而言，`SCRFD-500M` 目前仍然是最合理的主選。
它的優勢不是單點極致，而是整體平衡最好：

- 輕量到足以認真考慮上板
- 強到足以支撐後續人臉辨識
- landmarks 路線成熟
- 與 alignment / embedding / decision 的整體相容性高

換句話說：

- 若你要一個「現在就能作為主線推進」的方案，選 `SCRFD-500M`
- 若你要一個「萬一上板壓力過大時的降階備案」，再看更小的 `SCRFD` 或 `BlazeFace`

---

## 14. 參考資料

以下主要採用原始論文或官方來源：

- SCRFD official page
  https://insightface.ai/scrfd
- SCRFD paper, *Sample and Computation Redistribution for Efficient Face Detection*
  https://arxiv.org/abs/2105.04714
- RetinaFace paper, *RetinaFace: Single-stage Dense Face Localisation in the Wild*
  https://arxiv.org/abs/1905.00641
- BlazeFace paper, *BlazeFace: Sub-millisecond Neural Face Detection on Mobile GPUs*
  https://arxiv.org/abs/1907.05047
- S3FD paper, *Single Shot Scale-invariant Face Detector*
  https://arxiv.org/abs/1708.05237

關於 YuNet，本文件主要將其作為工程輕量 baseline 類型參考，而不是本專案主線候選核心。
