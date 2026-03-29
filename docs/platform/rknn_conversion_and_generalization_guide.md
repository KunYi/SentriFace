# RKNN Conversion And Generalization Guide

## 1. 目的

這份文件定義本專案在 `ONNX -> RKNN` 轉換時的正式工作流程與注意事項。

重點不是只有：

- 能不能成功轉成 `.rknn`

還包括：

- 轉完後是否仍保有泛化能力
- 是否會變成依賴特定場域校準資料
- 是否適合量產部署

---

## 2. 先釐清：訓練資料 vs 轉換資料

`RKNN` 轉換時常會要求提供一小批圖片。

這批圖片的角色是：

- calibration dataset
- quantization reference dataset
- conversion reference dataset

它們的用途是：

- 幫工具估計 tensor 分布
- 幫助量化
- 讓 `int8` / quantized model 比較穩定

它們不是：

- 拿來重新訓練模型權重
- 拿來讓模型記住某些使用者
- 拿來做 closed-set identity learning

所以：

- training dataset 會改變模型語意能力
- conversion dataset 不應改變模型本質任務，只影響量化與部署表現

---

## 3. 正式轉換步驟

### 3.1 準備 ONNX

先確認：

- 模型已是穩定 `ONNX`
- input / output 介面已凍結
- preprocess / postprocess 已在 host reference 路徑驗證過

至少要記錄：

- model name
- input shape
- output shape
- opset
- source commit / release

### 3.2 先做 Host Reference

在轉 `RKNN` 前，先用 host 端 reference 固定行為：

- detector：bbox / landmarks 是否正確
- embedding：向量輸出、similarity、top-k 是否合理

這一步的目的是：

- 先知道原始 ONNX 參考答案

否則之後板端結果壞掉時，會不知道是模型本身、前處理、還是 RKNN 轉換出問題。

### 3.3 準備 Conversion Dataset

這一步只需要：

- 少量
- 但有代表性的圖片

不要誤用：

- 訓練集整包
- 大量低品質異常圖
- 某單一場域的偏狹樣本

### 3.4 執行 RKNN Conversion

轉換時要記錄：

- RKNN-Toolkit2 版本
- Python 版本
- target platform
- quantization mode
- dataset path
- conversion log

至少確認：

- ONNX load 成功
- conversion 成功
- 沒有 blocking unsupported op
- warning 有保存

### 3.5 做板端 Runtime 驗證

要確認：

- `.rknn` 可初始化
- inference 可成功
- output shape 正確
- 長時間連跑沒有不穩定問題

### 3.6 做 ONNX vs RKNN 比對

同一批輸入至少要比：

- detector：
  - bbox
  - landmark
- embedding：
  - same-sample cosine
  - top-1 identity
  - top1-top2 margin

這一步是必要的。

因為：

- 轉成功不代表結果正確

---

## 4. Conversion Dataset 怎麼選

### 4.1 核心原則

conversion dataset 應該代表：

- 模型在真實部署時最常看到的輸入分布

而不是：

- 某一次 demo 場景
- 某個案場的特定光線
- 某個使用者群體

### 4.2 Detector 與 Embedding 要分開

不要用同一批圖一股腦同時拿來校準 detector 與 embedding。

較合理做法：

- detector calibration set：
  - 真實 frame
  - 含背景、光線、距離、尺寸變化
- embedding calibration set：
  - alignment 後的 face crop
  - 聚焦臉部像素分布與光照條件

### 4.3 最低覆蓋面

校準資料至少應覆蓋：

- day / bright indoor
- night / low light
- backlight / uneven light
- 暖光 / 冷光
- 不同膚色
- 輕微角度差

但不要過度放大極端異常樣本比例，例如：

- 嚴重模糊
- 幾乎看不到臉
- 嚴重遮擋

### 4.4 資料量

第一版不追求超大。

較實務的起點可先是：

- detector：`30 ~ 100` 張代表性 frame
- embedding：`30 ~ 100` 張 alignment 後 face crop

重點是代表性，不是數量神話。

---

## 5. 怎樣避免「場域特化」

這是最重要的原則。

### 5.1 不接受 per-site recalibration 當主策略

如果一個模型只有：

- 每換一個安裝場域
- 就要重新換一批 calibration images 再轉一次

才能維持正常表現，那它就不是好的第一版產品主線候選。

對量產來說，我們要的是：

- global reference calibration set

不是：

- per-site calibration set

### 5.2 建立通用 calibration set

較合理的產品化做法是：

- 建一份跨場景通用 calibration set
- 用同一份資料轉出標準 `.rknn`
- 再拿這個 `.rknn` 去不同場域驗證

若大多數場域都穩，代表：

- 模型與量化策略具有產品價值

若每到新場域都要重做 conversion，代表：

- 模型或量化策略太脆弱

### 5.3 Day / Night 的處理方式

不是只做白天，也不是每個場域分開做一套。

較合理方式：

- calibration set 裡本來就包含 day / night
- 比例盡量接近產品預期分布

例如若真實部署大概是：

- day 60%
- night 30%
- difficult lighting 10%

那 calibration set 也應大致接近這種比例。

### 5.4 不要把 calibration 當 threshold tuning

conversion dataset 的目的是：

- 讓量化穩定

不是：

- 用來彌補模型本身泛化不足

若必須靠反覆換 calibration set 才能撐住不同場景，問題通常不只是 dataset，
而是：

- 模型家族
- graph/quantization 策略
- 或前處理本身

---

## 6. 本專案的建議資料來源

對目前 `SentriFace`，較合理的 conversion dataset 來源是：

- host enrollment artifact 的 accepted frame / face crop
- offline replay artifact
- RTSP artifact
- 最後再補 RV1106 實拍資料

原則是：

- 優先使用與真實產品資料流一致的輸入

不要優先依賴：

- 網路隨便抓的人臉圖
- 與實際 camera/ISP 差異很大的資料

---

## 7. 若現有模型轉換有問題，優先怎麼做

若目前主線候選在 `RKNN` 轉換上出現：

- unsupported op
- conversion 不穩
- board-side inconsistency

建議優先順序是：

1. 先查是否為 graph / opset / preprocess 問題
2. 再查是否為 calibration set 過度偏狹
3. 若仍卡住，優先檢查：
   - Luckfox 已示範的 `RetinaFace + MobileFaceNet`
4. 不要直接跳去更複雜、更新但部署風險更高的模型

---

## 8. 判斷是否適合量產主線

一個模型/轉換策略若要當量產主線，至少應符合：

- conversion 穩定
- board runtime 穩定
- ONNX / RKNN 結果接近
- 用通用 calibration set 就能跨多場域維持合理表現
- 不需 per-site 重轉

若做不到最後一點，就算 demo 可跑，也不應輕易凍結為產品主線。

---

## 9. 當前結論

一句話總結：

- `RKNN` 轉換不是只看能不能生出 `.rknn`，而是要看量化後是否仍保有跨場域泛化能力。

對本專案來說，更好的策略是：

- 建立通用 calibration set
- 驗證多場域泛化
- 拒絕把 per-site recalibration 當成第一版量產主線前提

