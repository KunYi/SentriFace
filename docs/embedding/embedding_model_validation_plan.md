# Embedding Model Validation Plan

## 1. 目的

這份文件定義 embedding 候選模型的正式驗證順序與判準。

它回答的不是：

- 哪篇 paper 看起來最強

而是：

- 哪個模型最適合 `RV1106 + RKNN + access control` 主線

---

## 2. 驗證目標

第一版要完成四件事：

1. 找出可轉 `RKNN` 的候選
2. 確認 `ONNX` 與 `RKNN` 結果接近
3. 確認 `RV1106` latency / memory 可接受
4. 在實際 pipeline 上確認辨識品質不退化

---

## 3. 候選清單

第一批正式候選：

1. `w600k_mbf`
2. `PocketNet`
3. `MixFaceNet`

第二批研究候選：

1. `EdgeFace`
2. `GhostFaceNets`

說明：

- 第一批偏向更保守、較接近 `RKNN` 友善的 compact CNN 路線
- 第二批可研究，但不先卡主線

另外保留一條 deployment-first fallback：

- `Luckfox RetinaFace + MobileFaceNet`

這條不是一般 paper 候選，而是：

- 若現有主線模型在 `RKNN` 轉換有問題
- 優先檢查的 Luckfox / RKNN 已示範組合

---

## 4. 驗證順序

驗證順序固定如下：

1. asset / graph 檢查
2. host `onnxruntime` reference
3. host workflow integration
4. `ONNX -> RKNN` conversion 檢查
5. `RV1106` device-side runtime 檢查
6. `ONNX vs RKNN` embedding consistency
7. access-control pipeline integration comparison

不要顛倒順序。

特別是：

- 沒過 conversion 的模型，不要先花很多時間做 pipeline 微調
- 沒過 consistency 的模型，不要先宣稱板端可用

---

## 5. 每一階段的檢查項

### 5.1 Asset / Graph 檢查

每個候選至少要確認：

- model source 清楚
- input 是 `112x112`
- output 維度可對應 `512-d`
- ONNX opset 可被現有工具鏈接受
- graph 沒有明顯怪異 custom op

若這一步就出現：

- 模型來源不清
- export 不穩
- graph 很難維護

就不應進第一批主線驗證。

### 5.2 Host `onnxruntime` Reference

要確認：

- C++ `FaceEmbedder` 可正確 init
- `PrepareInputTensor(...)` 與 model input 對得上
- `Run(...)` 可穩定產生 `512-d`
- self-match / same-person / different-person 的相似度分布合理

最少要做：

- 同圖 self-match
- 同人不同圖
- 不同人不同圖

### 5.3 Host Workflow Integration

要確認這條能打通：

1. artifact summary
2. baseline generation
3. `.sfbp`
4. `.sfsi`
5. verify / replay

若模型需要特例前處理，應在這一步暴露，不要等板端才發現。

### 5.4 `ONNX -> RKNN` Conversion

每個候選都要記錄：

- conversion 成功或失敗
- warning / unsupported op
- 是否需要改 opset
- 是否需要 graph surgery
- 是否需要 CPU fallback

只要出現以下任一項，就要降級優先度：

- conversion 失敗
- 需重寫大量 graph
- 依賴不穩定 fallback

### 5.5 `RV1106` Device Runtime

至少要記錄：

- init latency
- steady-state inference latency
- peak memory / DDR 壓力
- 長時間穩定性

對本專案更重要的是：

- embedding 不應明顯擠壓 detector / tracking / decision 的整體 budget

### 5.6 Embedding Consistency

用同一批 aligned face crop，比對：

- `onnxruntime` output
- `RKNN` output

至少看：

- cosine similarity of same sample between backends
- query-top1 identity 是否一致
- top1-top2 margin 是否明顯崩壞

這一步比單純 tensor L2 距離更重要，因為最終我們關心的是 search / decision 行為。

### 5.7 Pipeline Integration

用同一批真實或 replay 資料，比較：

- verify top-1 hit stability
- false accept / false reject 傾向
- decision 收斂時間
- adaptive prototype 之後是否仍穩定

---

## 6. 比較矩陣

正式記錄時，建議每個候選都填以下欄位：

| Candidate | ONNX Ready | RKNN Convert | RKNN Runtime | ONNX/RKNN Consistent | Latency Acceptable | Integration Cost | Keep / Drop |
| --- | --- | --- | --- | --- | --- | --- | --- |
| w600k_mbf |  |  |  |  |  |  |  |
| PocketNet |  |  |  |  |  |  |  |
| MixFaceNet |  |  |  |  |  |  |  |
| EdgeFace |  |  |  |  |  |  |  |
| GhostFaceNets |  |  |  |  |  |  |  |

其中：

- `Integration Cost` 建議用 `low / medium / high`
- `Keep / Drop` 應附一句原因

---

## 7. 淘汰規則

若模型符合以下任一條件，可直接淘汰：

1. `ONNX -> RKNN` 無法穩定轉換
2. 板端輸出與 ONNX reference 明顯不一致
3. latency / memory 超出 access-control 產品可接受範圍
4. 需為該模型維護太多特殊前後處理
5. 對 `.sfbp / .sfsi / Search V2` 主線造成不必要耦合

也就是：

- 不是所有 paper 表現好的模型都值得保留到最後

---

## 8. 當前建議執行順序

### Phase A

1. `w600k_mbf`
2. 補完整 host reference / replay baseline
3. 做第一輪 `ONNX -> RKNN` compatibility

若這一步出現明顯 conversion / operator 阻塞，優先插入：

4. `Luckfox RetinaFace + MobileFaceNet` compatibility check

### Phase B

1. `PocketNet`
2. `MixFaceNet`

條件是：

- `w600k_mbf` 已有第一輪 conversion / runtime 結果

### Phase C

只在以下情況再啟動：

- `w600k_mbf` conversion 不穩
- `w600k_mbf` latency 不理想
- 或 `PocketNet / MixFaceNet` 顯示明顯優勢

此時才研究：

- `EdgeFace`
- `GhostFaceNets`

但若阻塞主因是 `RKNN compatibility`，不是純 accuracy / latency 問題，
則應優先於 `Phase C` 之前先檢查：

- `Luckfox RetinaFace + MobileFaceNet`

---

## 9. 與目前 repo 的關係

目前 repo 已具備：

- `FaceEmbedder` runtime abstraction
- C++ `onnxruntime` backend
- `.sfbp / .sfsi` package-first 主線
- baseline generation / verify / replay workflow

因此接下來模型驗證的重點不應是重寫架構，而是：

- 換 model asset
- 驗證前處理相容性
- 做 backend consistency 與 board benchmark

---

## 10. 當前結論

最合理的工程結論是：

- 先把 `w600k_mbf` 當 baseline 做完整 `RKNN` 驗證
- 不要只押單一模型
- 若現有主線候選在 `RKNN` 轉換有問題，優先嘗試 Luckfox 已提供的
  `RetinaFace + MobileFaceNet` 組合
- 把 `PocketNet` 和 `MixFaceNet` 當正式 backup candidates
- 等第一輪 conversion / board 結果出來，再決定是否需要更激進地研究 `EdgeFace` 或 `GhostFaceNets`

一句話總結：

- 下一步不是直接換模型，而是把 embedding 候選納入固定驗證矩陣，讓模型凍結建立在 `RV1106 + RKNN` 的實測證據上。
