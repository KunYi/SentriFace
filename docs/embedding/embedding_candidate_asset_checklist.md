# Embedding Candidate Asset Checklist

## 1. 目的

這份 checklist 用來確認一個 embedding 候選模型，是否值得進入正式驗證流程。

使用時機：

- 新增候選模型
- 替換 `w600k_mbf`
- 準備做 `ONNX -> RKNN` 轉換前

---

## 2. 基本資訊

請先記錄：

- candidate name：
- model family：
- source repo / paper：
- model file：
- license：
- trained with：
- input size：
- output dim：
- ONNX opset：

---

## 3. 必要條件

以下項目都應先確認：

- [ ] 模型來源可信
- [ ] 可追溯到 paper / repo / release
- [ ] 權重檔可重現取得
- [ ] license 可接受
- [ ] input 為 `112x112`
- [ ] output 可對應 `512-d`
- [ ] 沒有依賴不明 custom op
- [ ] ONNX 檔能在 host 正常 load

只要上述任一項失敗，先不要放入第一批主線驗證。

---

## 4. 工程相容性

請確認：

- [ ] 可對接目前 `FaceAligner` 的 `112x112` 對齊輸入
- [ ] 可對接目前 `FaceEmbedder` 的 preprocess 邊界
- [ ] output 可直接進 `.sfbp / .sfsi`
- [ ] 不需要改 `FaceSearchV2` 的 `512-d` 主線
- [ ] 不需要為單一模型重寫大量 package 格式

---

## 5. RKNN 風險初篩

在正式轉換前，先做保守判斷：

- [ ] graph 主要由常見 CNN operator 組成
- [ ] 沒有明顯 transformer / dynamic-shape 依賴
- [ ] 沒有明顯高風險 operator 組合
- [ ] 量化路線看起來可行
- [ ] 預期不需要大幅 graph surgery

若這一段大多數都答不出來或偏否定，優先度應下降。

---

## 6. Host Reference 基礎檢查

至少要確認：

- [ ] `onnxruntime` 可正常 init
- [ ] 單張 image inference 可完成
- [ ] output shape 正確
- [ ] L2 normalize 後沒有異常 `NaN / Inf`
- [ ] self-match cosine 接近合理高值

---

## 7. 結論欄

- shortlist decision：
  - `keep`
  - `hold`
  - `drop`

- reason：

- next step：

