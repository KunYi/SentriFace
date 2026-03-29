# Weighted Search Spec

## 1. 目的

這份文件定義 search 模組未來如何支援 `weighted prototype`。

這是 `adaptive prototype policy` 與 `EnrollmentStore V2` 的自然延伸，目的在於：

- 不把所有 prototype 視為同權重
- 讓 baseline prototype 維持最高信任
- 讓 recent adaptive prototype 能參與搜尋，但不主導結果

---

## 2. 為什麼需要 weighted search

若想先建立本文件用到的 search 與 decision 直覺，例如：

- normalized similarity score 的幾何意義
- 為什麼 score 與 margin 要分開看
- 為什麼 top-1 不等於足以 unlock
- 為什麼 false accept / false reject 是 operating point tradeoff

可先參考：

- `docs/search/embedding_search_intuition.md`

如果 search 仍把所有 prototype 當作同權重，會出現幾個風險：

- recent adaptive zone 的短期樣本可能過度影響 top-1
- baseline zone 的高信任 prototype 優勢被稀釋
- search 分數在長期演進後變得難以解讀

因此只在 enrollment 內部分 zone 還不夠，search 也要知道這些 zone 的信任差異。

---

## 3. 設計目標

weighted search 的目標不是把 search 做成很複雜的研究型 ensemble，而是：

- 仍保持 CPU-first 可驗證
- 對 `RV1106` 友善
- 行為可解釋
- 與目前 decision 的 `top-1 / margin` 邏輯相容

---

## 4. 分層策略

### 4.1 Prototype-level score

每個 prototype 先計算：

- `cosine_similarity(query, prototype)`

### 4.2 Prototype weight

每個 prototype 再乘上對應權重：

- `weighted_score = raw_score * prototype_weight`

其中 `prototype_weight` 可由 zone 決定，例如：

- baseline: `1.00`
- verified history: `0.80`
- recent adaptive: `0.60`

### 4.3 Person-level aggregation

同一個 `person_id` 若有多個 prototypes，不應只把它們視為多個獨立候選。

較合理的產品化做法是：

- 先算 prototype-level scores
- 再做 person-level aggregation

---

## 5. 建議的 person-level aggregation

第一版較建議採：

- `person_score = max(weighted_score across prototypes)`

理由：

- 計算簡單
- 可解釋
- 對板端友善
- 不會因為 prototype 數量較多就天然占優勢

不建議第一版就直接採：

- 全平均
- 複雜投票
- learned fusion

---

## 6. 輸出建議

未來 search V2 的輸出可分兩層。

### 6.1 Prototype-level hit

```cpp
struct PrototypeHit {
  int person_id = -1;
  int prototype_id = -1;
  int zone = 0;
  std::string label;
  float raw_score = 0.0f;
  float weighted_score = 0.0f;
};
```

### 6.2 Person-level hit

```cpp
struct SearchHitV2 {
  int person_id = -1;
  std::string label;
  float score = 0.0f;
  int best_prototype_id = -1;
  int best_zone = 0;
};
```

這樣的好處是：

- decision 仍可使用 person-level top-k
- debug / validation 時仍可追溯是哪個 prototype 命中

---

## 7. 與目前 decision 的關係

目前 decision 使用：

- top-1
- top-2 margin
- temporal smoothing

weighted search 應維持這條邏輯不變，只是讓：

- `top-1`
- `top-2`
- `margin`

的來源更可信。

也就是說，weighted search 的角色是：

- 改善候選排序品質
- 而不是重寫 decision engine

---

## 8. 與 adaptive prototype 的配合

weighted search 的設計，必須與 prototype zones 一起看。

### 8.1 Baseline zone

- 權重最高
- 作為長期穩定基準

### 8.2 Verified history zone

- 權重次高
- 補足長期外觀變化
- 但數量必須受控，避免單一 person 的 history 無限制膨脹並稀釋 search 解釋性

### 8.3 Recent adaptive zone

- 權重最低
- 幫助近期適應
- 不應單獨主導最終 identity

---

## 9. 對 RV1106 的實務建議

在 `RV1106` 上，weighted search 第一版應保持簡單：

- prototype_weight 為固定常數
- person-level aggregation 採 `max`
- top-k 保持小
- 不做大型 ANN / vector DB

這樣就能在不大改整體系統的情況下，把 search 從「全同權」提升到「可控權重」。

---

## 10. 建議實作順序

若未來要落地 weighted search，建議順序如下：

1. 擴充 `FacePrototype`，加入 `prototype_weight / prototype_id / zone`
2. 新增 prototype-level debug 輸出
3. 將 search 排序依據改成 `weighted_score`
4. 再加入 person-level aggregation
5. 最後才調整 decision 使用的輸出格式

先不要一開始就同時改：

- enrollment
- search
- decision
- storage

全部模組。
