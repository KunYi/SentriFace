# Embedding Model Evaluation

## 1. 目的

這份文件評估本專案第一版 `embedding` 模型選型，重點問題是：

- 是否應持續以 `w600k_mbf.onnx` 作為主線基準
- 若不只看 `w600k_mbf`，還有哪些輕量候選值得評估
- 在 `RV1106 + RKNN + 112x112 + local-first access control` 條件下，哪類模型最合理

本文件不是追求純學術排行榜，而是追求：

- 可部署
- 可維護
- 可與 `SCRFD + 5 landmarks + Search V2` 穩定整合

---

## 2. 先釐清名詞

### 2.1 `ArcFace` 不是 backbone 名稱

在工程語境裡，大家常說「用 ArcFace 模型」，但更準確地說：

- `ArcFace` 主要是 face recognition 的 margin-based training / classification 方法
- 真正部署時，仍要搭配一個 backbone

所以：

- `ArcFace + MobileFaceNet`
- `ArcFace + PocketNet`
- `ArcFace + MixFaceNet`

都可能成立。

### 2.2 `w600k_mbf` 的實際含義

目前 repo 採用的 `w600k_mbf.onnx`，實際上是：

- `MBF`
- 通常可理解為 `MobileFaceNet`
- 訓練資料來自 `WebFace600K`

InsightFace 的 model zoo 也把 `buffalo_s` / `buffalo_sc` 的 recognition backbone 標為
`MBF@WebFace600K`。

---

## 3. 本專案的選型前提

在這個專案裡，embedding 模型不能只看驗證精度，還必須同時滿足：

1. 能在 `RV1106G2 / RV1106G3` 的資源邊界內運作
2. 優先考慮 `RKNN` operator / conversion 風險
3. 輸入維持 `112x112`
4. output 維度以 `512-d` 為主
5. 適合 event-driven，而不是每幀都跑
6. 能和既有 `.sfbp / .sfsi / FaceSearchV2` 主線無痛整合

也就是：

- 最佳模型不是「榜單最高」
- 而是「在 `RV1106 + RKNN` 上最有機會穩定落地的輕量模型」

---

## 4. 目前主線基準

目前主線基準是：

- `w600k_mbf.onnx`

它的優勢是：

- 已經在 InsightFace 生態裡被大量使用
- `112x112 -> 512-d` 路徑成熟
- 與現有 baseline workflow、C++ `onnxruntime` backend、`.sfbp / .sfsi` 主線已接通
- `MobileFaceNet` 本身就是針對 mobile / embedded face verification 提出的輕量 backbone

但它目前仍不是「已凍結的板端最終答案」，原因是：

- 還沒完成 `ONNX -> RKNN` compatibility 驗證
- 也還沒完成 `RV1106` device-side reference comparison

因此現在比較合理的定位是：

- `current baseline`
- 不是 `final frozen board model`

---

## 5. 候選模型清單

這裡只列對本專案真正有實務意義的候選，不追求把所有論文都掃過。

### 5.1 `MobileFaceNet / MBF`

定位：

- 最成熟的輕量 baseline

優點：

- 原始設計就面向 mobile / embedded face verification
- 參數量小，論文描述少於 `1M` parameters
- 與現有 `w600k_mbf` 路徑最接近
- 社群、InsightFace、生態與現成 ONNX 資源最成熟
- 對 `112x112`、`512-d`、ArcFace 訓練路線都很自然

缺點：

- 不是最新一代的 compact FR 上限
- 在同等級輕量模型中，近年已有方法宣稱能在近似算量下超越它

對本專案的結論：

- 最穩妥的主線 baseline
- 也是最合理的第一個 `RKNN` compatibility 驗證對象

### 5.2 `PocketNet`

定位：

- 極輕量 face recognition family

優點：

- 論文明確以 embedded / low-end devices 為目標
- smallest variant 約 `0.92M` parameters
- 論文宣稱在 compactness 相近條件下，相對多個 compact FR baseline 有優勢
- 純 CNN 路線，理論上比 hybrid / transformer 類更接近 `RKNN` 友善

缺點：

- 生態成熟度不如 InsightFace `MBF`
- 現成 ONNX / deployment assets 與社群使用量通常較少
- 實際可轉 `RKNN` 的風險仍需另驗

對本專案的結論：

- 值得列為 `第二優先候選`
- 若 `w600k_mbf` 在 RKNN 轉換或效能上不理想，PocketNet 很值得接著驗

### 5.3 `MixFaceNets`

定位：

- 對 `low-compute` 條件很有吸引力的高效率 CNN 家族

優點：

- 論文明確主打 extremely efficient face recognition
- 在 `< 500M FLOPs` 條件下，論文結果優於 MobileFaceNet
- 仍偏 CNN/Depthwise 路線，對 edge deployment 比較友善

缺點：

- 雖然比 transformer 類穩妥，但整體工程成熟度通常仍不如 InsightFace `MBF`
- 不一定有現成、穩定、廣泛使用的 ONNX release
- 若要自己找 checkpoint / export / calibrate，工程成本會上升

對本專案的結論：

- 很適合作為 `性能升級候選`
- 如果想找「比 MBF 更有機會維持輕量、又比 MBF 更新」的路線，這是很合理的一個

### 5.4 `EdgeFace`

定位：

- 新一代 edge-oriented compact FR

優點：

- 論文就是為 edge devices 而寫
- 參數量不大，論文摘要提到 `1.77M` parameters
- 在 paper 指標上很漂亮

缺點：

- 屬於 hybrid architecture
- 往往比純 CNN 更容易帶來 `RKNN` operator / graph compatibility 風險
- 雖然學術表現吸引人，但對 `RV1106 + RKNN` 來說部署不確定性更高

對本專案的結論：

- 可以列入研究名單
- 但不應作為第一批 board deployment 候選

### 5.5 `GhostFaceNets`

定位：

- 也是常被提到的 lightweight FR 路線

優點：

- 模型小
- paper / code 可得性不差

缺點：

- Ghost-family 結構未必比純 MobileFaceNet / PocketNet 更 `RKNN` 友善
- 現成量產案例與成熟生態相對沒有 `MBF` 明確
- 目前 repo 主線也沒有直接依附這個家族

對本專案的結論：

- 可作為備查候選
- 但不建議排在 `MBF / PocketNet / MixFaceNet` 前面

### 5.6 大 backbone：`ResNet50 / ResNet100`

定位：

- 強精度基準，不是第一版板端主線

優點：

- 指標通常更高
- 可作 reference upper bound

缺點：

- 對 `RV1106` 不夠友善
- memory / latency / power 壓力大
- 容易吃掉 detector、search、decision、liveness 的整體預算

對本專案的結論：

- 可以當 host-side reference
- 不應當第一版板端主線

---

## 6. 評估維度

模型是否適合本專案，建議按以下順序看：

1. `RKNN` compatibility
2. `RV1106` latency / memory
3. ONNX reference 與板端結果一致性
4. accuracy / TAR / verification quality
5. 社群成熟度與維護成本

這個順序很重要。

因為對本專案來說：

- 不能部署的高精度模型沒有主線價值

---

## 7. 各候選對本專案的實務排序

### Tier A：應優先實測

1. `w600k_mbf`
2. `PocketNet`
3. `MixFaceNet`

原因：

- 都屬於 compact FR 的合理方向
- 前兩者特別偏向純 CNN / mobile route
- `w600k_mbf` 又已經與現有 repo 主線整合

### Tier B：可研究，但不先投主線

1. `EdgeFace`
2. `GhostFaceNets`

原因：

- 有潛力
- 但部署不確定性、graph complexity、現成量產路線成熟度，都比 Tier A 高

### Tier C：只當 reference，不當第一版板端主線

1. `ResNet50 / ResNet100` 類大型 backbone

---

## 8. 當前工程建議

### 8.1 不要只押 `w600k_mbf`

結論不是「只有 `w600k_mbf` 可以用」。

更準確的說法是：

- `w600k_mbf` 是目前最合理、最成熟、最低風險的 baseline
- 但它不應是唯一被考慮的候選

### 8.2 最合理的 shortlist

目前建議 shortlist 是：

1. `w600k_mbf`
2. `PocketNet`
3. `MixFaceNet`

若這三者都不理想，再看：

4. `EdgeFace`
5. `GhostFaceNets`

### 8.3 第一批驗證順序

建議順序：

1. `w600k_mbf.onnx -> RKNN`
2. `PocketNet ONNX -> RKNN`
3. `MixFaceNet ONNX -> RKNN`
4. host / board reference comparison
5. 再依結果凍結板端主線

---

## 9. 對目前 repo 的直接意義

目前 repo 已經做好的事情：

- `112x112 -> 512-d` embedding 邊界已固定
- `FaceEmbedder` 已有 CPU / C++ `onnxruntime` runtime 邊界
- `.sfbp / .sfsi / Search V2` 已不綁死特定 backbone

這代表後續若替換 embedding backbone：

- 不需要重寫整個 enrollment / search / pipeline
- 主要影響的是：
  - model asset
  - preprocess / output compatibility
  - reference / board validation

這是目前架構上很重要的好處。

---

## 10. 當前結論

目前最務實的工程結論是：

- 不是只有 `w600k_mbf` 一個可選模型
- 但它仍是目前最合理的 baseline
- 真正值得納入短名單的替代方案，首選是：
  - `PocketNet`
  - `MixFaceNet`
- `EdgeFace` 與 `GhostFaceNets` 可以研究，但不應先放到主線最前排
- 在 `RV1106` 專案裡，模型凍結不能只看 paper accuracy，必須先過：
  - `ONNX -> RKNN`
  - board-side latency
  - result consistency

一句話總結：

- `w600k_mbf` 不是唯一答案，但它是目前最合理的第一順位 baseline；下一步應做的是把 `PocketNet / MixFaceNet` 納入正式實測，而不是只憑印象換模型。

---

## 11. 參考來源

- InsightFace model zoo / package table:
  - https://deepwiki.com/deepinsight/insightface/5.2-face-detection-%28scrfd%29
- MobileFaceNets paper:
  - https://arxiv.org/abs/1804.07573
- PocketNet paper:
  - https://arxiv.org/abs/2108.10710
- MixFaceNets paper:
  - https://arxiv.org/abs/2107.13046
- EdgeFace paper:
  - https://arxiv.org/abs/2307.01838
- RKNN 風險與本專案部署邊界：
  - [docs/platform/rknn_operator_compatibility_risk.md](../platform/rknn_operator_compatibility_risk.md)

