# RKNN Multi-Model Strategy

## 1. 目的

這份文件定義目前對 `RV1106 + RKNN` 多模型載入與切換的**暫定策略**。

重點不是先假設板端一定能輕鬆同時跑多個 model，  
而是先把工程上的合理方向、風險與驗證項目寫清楚。

這份文件的定位是：

- `working assumption`
- `implementation direction`
- `must validate on target`

而不是最終已證實結論。

---

## 2. 目前問題背景

本專案目前至少有兩個主要模型需求：

- `SCRFD-500M`：做人臉 detection
- face embedding model：做人臉特徵比對 / search / re-link

因此自然會遇到這個問題：

- 板端能不能同時保留兩個 model？
- 如果要切換 model，成本高不高？
- 需不需要每次重載？

目前答案是：

- **有合理理由假設可以做兩模型常駐**
- 但**絕對不能先當成已驗證事實**

---

## 3. 暫定產品策略

在沒有板端實測證據之前，第一版建議先採以下策略：

### 3.1 假設方向

優先假設：

- detector model 常駐
- embedding model 也盡量常駐
- 透過調度控制推論頻率
- 不採每次切換就重新載入 model 的設計

原因很簡單：

- 若每次切換都 reload / init
- latency 會更不可控
- 記憶體碎片與初始化成本也可能更糟

所以就工程直覺來看，較合理的產品方向通常是：

- **常駐 + 節流 + 排程**

而不是：

- **頻繁 unload / reload**

### 3.2 調度方向

第一版預設優先級應為：

1. detection 優先
2. tracking 持續
3. embedding 僅事件觸發

也就是：

- detection 高頻
- embedding 低頻
- search / decision 跟著 embedding event 走

這與本專案既有原則一致：

- embedding 不是每幀跑
- 2 秒內得到穩定門禁結果即可

---

## 4. 目前不能假設的事

在沒有板端量測前，以下事情都**不能當成已知事實**：

- `RV1106` 能穩定同時保留兩個 RKNN context
- 兩個 model 同時常駐時記憶體壓力可接受
- model 切換幾乎沒有成本
- detection 與 embedding 交錯執行時 latency 仍能滿足門禁需求
- `rknn_init` / `release` 的成本可以忽略

所以目前任何「一定可以」或「一定不行」的說法都不夠嚴謹。

---

## 5. 需要驗證的核心問題

這部分才是之後真正要量測的重點。

### 5.1 初始化成本

需要量測：

- detector model init time
- embedding model init time
- 兩模型都 init 時的總時間

要回答的問題：

- 開機時能不能一次初始化兩個 model？
- 是否需要 lazy init？

### 5.2 常駐記憶體成本

需要量測：

- 單獨載入 detector 時的 memory footprint
- 單獨載入 embedding 時的 memory footprint
- 同時載入兩者時的總 memory footprint

要回答的問題：

- `RV1106` 記憶體是否足夠？
- 是否會壓縮到其他 pipeline buffer？

### 5.3 切換成本

需要量測：

- detector inference latency
- embedding inference latency
- 兩者交錯執行時的平均與 worst-case latency

要回答的問題：

- detection 跑動時插入 embedding 會不會讓畫面掉太多？
- 是否需要限制 embedding 觸發頻率？

### 5.4 系統穩定性

需要量測：

- 長時間運行是否出現 memory 增長
- 連續切換/交錯推論是否有 crash / timeout / 隨機錯誤

要回答的問題：

- 常駐雙模型是否穩定
- 是否需要 fallback 到單模型常駐策略

---

## 6. 驗證前的工程假設

在還沒實測前，主線程式設計應採取以下假設：

- API / module interface 應允許 detector 與 embedding 各自持有常駐 backend/context
- runtime 管理層不要寫成「每次呼叫都重新載入」
- scheduling 層要預留節流與優先級控制
- 任何與板端效能有關的決策都應標記為 `must validate`

也就是：

- **介面先朝常駐多模型設計**
- **產品結論必須靠板端驗證**

---

## 7. 若實測不如預期的退路

若之後 `RV1106` 實測發現雙模型常駐不理想，可能的退路包括：

### 7.1 Detector 常駐，embedding 延後

- detection 常駐
- embedding 僅在必要事件觸發
- 若當下算力忙碌，可延後一小段時間再做 embedding

### 7.2 Detector 板端，embedding 上位機

- detection / tracking / alignment 在板端
- embedding / search 在上位機或伺服器

### 7.3 更輕量 embedding 模型

- 若 embedding model 過重
- 優先換輕量模型
- 不先破壞 detection 主線

---

## 8. 當前結論

目前較合理的產品工程結論是：

- **兩模型常駐是合理方向**
- **但現在尚未驗證**
- **不應寫死成已確認能力**

所以本專案現階段應採：

- implementation direction：
  常駐 + 調度 + 節流
- product conclusion：
  **must validate on RV1106 target**

換句話說：

- 可以先朝這個方向設計
- 但所有涉及 `RKNN multi-model` 的結論，都必須等板端實測之後再凍結
