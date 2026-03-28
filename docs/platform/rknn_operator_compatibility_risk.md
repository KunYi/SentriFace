# RKNN Operator Compatibility Risk

## 1. 目的

這份文件記錄一個非常實際的產品風險：

- **不是所有 ONNX operator 都一定能被 RKNN 正常支援**

因此：

- 我們今天在 PC 上能跑的 ONNX model
- 不代表明天一定能順利轉成 `.rknn`
- 也不代表轉成功之後，在 `RV1106` 上就一定結果正確

這份文件的定位是：

- 風險提醒
- 模型選型原則
- 驗證 checklist

而不是已完成事項。

---

## 2. 為什麼這是主線風險

對本專案來說，模型不只是學術效果問題，而是產品可部署性問題。

只要出現以下任一情況，主線就會被卡住：

- ONNX -> RKNN 轉換失敗
- 某些 operator 不支援
- 某些 operator 只能 fallback，效能不符合需求
- 轉換成功，但板端結果和 ONNX reference 明顯不一致
- 必須手寫前後處理或 custom op 才能落地

也就是說：

- **模型精度好，不代表能量產**
- **能跑 ONNX，不代表能跑 RKNN**

---

## 3. 目前已知的合理假設

目前我們只能做以下保守假設：

- `SCRFD-500M` 與 embedding model 都必須經過 RKNN compatibility 驗證
- 模型選型不能只看：
  - accuracy
  - FLOPs
  - CPU 測試結果
- 還要看：
  - operator 支援性
  - ONNX opset
  - conversion 穩定性
  - device-side 結果一致性

---

## 4. 不能先假設的事

在沒有實測前，不應先假設：

- ONNX 模型一定能轉 RKNN
- 轉換成功就等於 device 端正確
- detector model 和 embedding model 都不需要改圖
- 現有前後處理不需要手動重寫
- 模型 zoo 裡相近模型可跑，就代表我們自己的模型也能跑

---

## 5. 目前工程結論

本專案現在應把這件事視為：

- **主線 deployment risk**

不是附帶問題。

因此後續所有模型策略都應加入這條原則：

- **先驗證 RKNN compatibility，再凍結模型**

而不是反過來。

---

## 6. 模型選型原則

模型選型時，建議優先級如下：

1. 能成功轉成 RKNN
2. device 端結果與 reference 接近
3. 效能可接受
4. 精度夠用

而不是：

1. 純理論精度最好
2. 之後再想辦法塞進 RKNN

對產品開發來說，後者風險很高。

---

## 7. 必做驗證

任何候選模型，至少要經過以下驗證：

### 7.1 Conversion 檢查

- ONNX load 是否正常
- RKNN conversion 是否成功
- 是否出現 unsupported op / warning
- 是否需要修改 opset / graph

### 7.2 Runtime 檢查

- `.rknn` 是否能在 `RV1106` 正常 init
- inference 是否成功
- output tensor shape 是否符合預期

### 7.3 Reference 比對

至少要對比：

- ONNX reference output
- RKNN output

確認：

- bbox / landmark / embedding 結果沒有明顯偏掉
- 不是「能跑但結果壞掉」

### 7.4 效能檢查

- init time
- inference latency
- memory footprint
- 長時間穩定性

---

## 8. 可能的應對策略

如果最後發現某個模型不適合 RKNN，可能的退路包括：

### 8.1 換模型

優先選：

- operator 更單純
- 已知有 Rockchip 範例或接近案例
- model zoo 中已有成功部署路徑的模型家族

### 8.2 改 ONNX graph

例如：

- 調整 opset
- 移除不必要節點
- 改寫部分 operator 組合

### 8.3 手寫前後處理

若主網路可跑，但輸出形式不完全符合現有流程，可能需要：

- 手寫 decode
- 手寫 postprocess
- 手動對齊量化與 tensor layout

### 8.4 自訂或替代實作

若某些 operator 真不支援，可能需要：

- custom op
- CPU fallback
- 改模型結構

但這類方案成本通常高，應視為後手，不是第一優先。

---

## 9. 對本專案目前的直接影響

目前至少要把這條風險套用到兩個模型：

- `SCRFD-500M`
- embedding model

換句話說：

- 我們現在的 ONNX 驗證只代表 PC reference 與資料流已較完整
- **還不代表 RKNN 路線已經安全**

所以後續應增加明確 milestone：

1. `SCRFD ONNX -> RKNN compatibility`
2. embedding ONNX -> RKNN compatibility
3. `RV1106` device-side reference comparison

---

## 10. 當前結論

現在最務實的專案結論是：

- **RKNN operator compatibility 是主線風險**
- **不應晚到整合末期才驗證**
- **模型凍結前必須先完成 compatibility 驗證**

一句話總結：

- **不是所有 ONNX model 都值得投入主線**
- **能不能被 RKNN 穩定支援，應該是模型選型的第一關之一**
