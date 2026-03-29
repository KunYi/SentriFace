# RKNN Embedding Validation Checklist

## 1. 目的

這份 checklist 專門用來驗證 embedding 候選模型在 `RKNN + RV1106` 上是否可作為主線部署候選。

---

## 2. Conversion 檢查

每個候選都要記錄：

- model name：
- ONNX path：
- RKNN toolchain version：
- target board：
- quantization mode：

檢查項：

- [ ] ONNX load 成功
- [ ] RKNN conversion 成功
- [ ] 無 blocking unsupported op
- [ ] warning 已記錄
- [ ] 若有 graph surgery，變更內容已記錄
- [ ] `.rknn` 可成功生成

若失敗，請記錄：

- fail stage：
- error message：
- tentative cause：
- next action：

---

## 3. Runtime 檢查

- [ ] `.rknn` 可在板端 init
- [ ] inference 可成功完成
- [ ] output tensor shape 正確
- [ ] 長時間連跑無明顯 crash
- [ ] init latency 已記錄
- [ ] steady-state latency 已記錄
- [ ] memory footprint 已記錄

---

## 4. Reference 比對

同一批 aligned face crop，至少比：

- host `onnxruntime`
- board `RKNN`

檢查項：

- [ ] 每筆 sample 都有 output
- [ ] same-sample cross-backend cosine 已記錄
- [ ] top-1 identity 一致率已記錄
- [ ] top1-top2 margin 偏移已記錄
- [ ] 無明顯 identity collapse

建議額外記錄：

- max deviation sample
- representative failure case

---

## 5. Pipeline 檢查

- [ ] 可導入 `.sfbp`
- [ ] 可導入 `.sfsi`
- [ ] verify workflow 可跑
- [ ] offline replay 可跑
- [ ] Search V2 top-k 行為無明顯異常
- [ ] decision 收斂時間無明顯退化

---

## 6. Go / No-Go 判準

### Go

符合以下條件可進下一輪：

- [ ] conversion 穩定
- [ ] runtime 穩定
- [ ] consistency 合理
- [ ] latency 可接受
- [ ] integration cost 可接受

### No-Go

符合以下任一條件可直接降級或淘汰：

- [ ] conversion 長期不穩
- [ ] output 與 ONNX reference 明顯偏離
- [ ] latency / memory 明顯超標
- [ ] 必須維護大量模型專屬特例

---

## 7. 結論欄

- final decision：
  - `promote`
  - `hold`
  - `drop`

- owner note：

- follow-up：

