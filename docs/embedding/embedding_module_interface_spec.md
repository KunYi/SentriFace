# Embedding Module Interface Spec

## 1. 目的

這份文件定義第一版 `embedding` 模組介面。

第一版先專注於：

- embedding 輸入輸出規格
- 向量後處理
- L2 normalize
- cosine similarity
- 為後續 event-driven trigger 與 embedding cache 打基礎

暫時不綁定任何特定模型推論後端。

---

## 2. 介面定位

目前模組位置：

```text
include/embedding/face_embedder.hpp
src/embedding/face_embedder.cpp
```

主要類型：

- `EmbeddingConfig`
- `EmbeddingRuntimeConfig`
- `EmbeddingInputTensor`
- `EmbeddingResult`
- `FaceEmbedder`

---

## 3. 第一版設計

`FaceEmbedder` 目前提供四個穩定能力：

1. 驗證輸入尺寸是否符合模型要求
2. 把 image/frame preprocess 成 model input tensor
3. 將 raw output 轉成 embedding 向量
4. 計算 cosine similarity

```cpp
class FaceEmbedder {
 public:
  explicit FaceEmbedder(const EmbeddingConfig& config = EmbeddingConfig {});

  bool ValidateInputShape(int width, int height, int channels) const;
  EmbeddingInputTensor PrepareInputTensor(
      const sentriface::camera::Frame& image) const;
  EmbeddingResult Postprocess(const std::vector<float>& raw_output) const;
  bool InitializeRuntime(
      const EmbeddingRuntimeConfig& runtime_config = EmbeddingRuntimeConfig {});
  EmbeddingResult Run(const sentriface::camera::Frame& image) const;
  float CosineSimilarity(const std::vector<float>& a,
                         const std::vector<float>& b) const;
};
```

### 3.1 預設配置

第一版預設：

- `112 x 112 x 3`
- `embedding_dim = 512`
- `l2_normalize = true`

---

### 3.2 Runtime 邊界

目前 runtime backend 分成：

- `stub_deterministic`
- `onnxruntime`

其中：

- `PrepareInputTensor(...)`
- `Postprocess(...)`

是穩定的 backend-agnostic core；
runtime 只負責真正的 model inference。

## 4. 為什麼先做 backend-agnostic core

這樣做的好處：

- 不把專案綁死在特定 RKNN/ONNX/SDK
- 可先把向量與搜尋規則穩定下來
- 後續接不同推論後端時，不需要重寫 search / decision 層

---

## 5. CPU-First 開發策略

本專案目前採用：

- 先在 `Intel CPU` 上完成演算法驗證
- 再遷移到 `RKNN` 或其他板端推論後端

這代表第一版 `embedding` 模組的核心目標是：

- 先固定輸入輸出規格
- 先固定向量後處理方式
- 先固定 similarity 計算

等 CPU 端資料流與測試穩定後，再接：

- C++ `onnxruntime`
- RKNN runtime
- NPU backend
- 或其他 vendor-specific inference path

### 5.1 但設計邊界仍以 RKNN 為主

雖然目前先在 CPU 上驗證，`embedding` 模組的設計仍必須遵守板端限制：

- 輸入尺寸優先維持 `112 x 112`
- embedding 維度不宜無限制放大
- 後處理與 similarity 計算應保持輕量
- 不假設可以頻繁地對每一條 track 每一幀都做 embedding

也就是：

- `CPU-first` 是驗證策略
- `RKNN-first` 是設計邊界

---

## 6. 與 Short-Gap Re-link 的關係

本專案目前也將 embedding 視為一種事件層訊號，而不只是查庫輸入。

典型用途是：

- 同一個人在很短時間內斷軌
- 新舊 track 空間位置接近
- 已經有至少一次可用 embedding

此時可利用 embedding similarity，輔助判斷：

- 新 track 是否應該接回舊的 decision session

也就是說，embedding 不一定要每幀都跑，但已經算出的結果仍然很有價值。

---

## 7. 產品化策略補充

對本專案來說，更重要的不是「embedding 是否每次都能即時 reconnect 所有 track」，而是：

- 是否能在可接受延遲內完成穩定辨識
- 是否能避免無意義重複推論
- 是否能逐步吸收使用者外觀慢變

因此後續 embedding 模組與 pipeline 的實際策略應偏向：

- `event-driven`
- `cache-first`
- `session-level`

也就是：

- 不是每幀都跑 embedding
- 已有 cache 時，優先利用 cache 支援 decision / re-link
- 超過一定時間或達到適當事件階段時，再觸發新的 embedding / search

產品化設計細節可參考：

- `docs/embedding/embedding_product_strategy.md`
