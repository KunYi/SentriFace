# Search V2 Interface Spec

## 1. 目的

這份文件定義 `Search V2` 的介面方向。

V2 的重點是把目前 search 從：

- 全 prototype 同權重
- 直接對 prototype 排序

演進成：

- 支援 weighted prototypes
- 先做 prototype-level scoring
- 再做 person-level aggregation

---

## 2. 設計目標

若想先理解本文件背後的概念脈絡，例如：

- normalized embedding search 的幾何直覺
- cosine similarity 與 Euclidean distance 的關係
- `top-1 / top-2 margin`、open-set rejection、false accept / false reject 的意義
- 為什麼 search ranking 與最終 authorization 不應混為一談

可先參考：

- `docs/search/embedding_search_intuition.md`

Search V2 應滿足：

- 與 `.sfsi` / `FaceSearchV2IndexPackage` 相容
- 與 `adaptive prototype policy` 相容
- 保持 `RV1106` 可承受的計算量
- 保持 decision 仍可使用 `top-1 / top-2 margin`
- 保留足夠 debug 能力

runtime / replay 主線應優先直接載入 `.sfsi` 或
`FaceSearchV2IndexPackage`；`AddPrototype(...)` / `RebuildFromPrototypes(...)`
較適合作為 build、import、compatibility helper。

---

## 3. V1 與 V2 差異

### 3.1 V1

- `FacePrototype` 只有 `person_id / label / embedding`
- `SearchHit` 只保留 person-level基本資訊
- query 後直接輸出 prototype 排序結果

### 3.2 V2

- prototype 帶 `prototype_id / zone / weight`
- query 時先算 prototype-level raw/weighted score
- 再聚合成 person-level top-k
- 同時保留 prototype-level debug 視圖

---

## 4. 建議型別

### 4.1 FacePrototypeV2

```cpp
struct FacePrototypeV2 {
  int person_id = -1;
  int prototype_id = -1;
  int zone = 0;
  std::string label;
  std::vector<float> embedding;
  float prototype_weight = 1.0f;
};
```

### 4.2 PrototypeHit

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

### 4.3 SearchHitV2

```cpp
struct SearchHitV2 {
  int person_id = -1;
  std::string label;
  float score = 0.0f;
  int best_prototype_id = -1;
  int best_zone = 0;
};
```

### 4.4 SearchResultV2

```cpp
struct SearchResultV2 {
  bool ok = false;
  std::vector<SearchHitV2> hits;
  std::vector<PrototypeHit> prototype_hits;
  float top1_top2_margin = 0.0f;
};
```

---

## 5. Query 流程

Search V2 的建議流程如下：

1. 對每個 prototype 計算 `raw_score = cosine_similarity`
2. 套用 `prototype_weight`
3. 得到 `weighted_score`
4. 產生 prototype-level ranking
5. 依 `person_id` 做 person-level aggregation
6. 對 person-level 結果排序並裁成 `top_k`
7. 產出 `top1_top2_margin`

在實作上，person-level `top_k` 可優先採 partial sort，而不是每次都對
全部 person hits 做 full sort。

當 score 相同時，也建議保留 deterministic tie-break，避免 replay /
regression 輸出因等分數而抖動。

---

## 6. 第一版 aggregation 建議

V2 第一版建議採：

- `person_score = max(weighted_score among prototypes of the same person)`

對產品來說，這是很合理的第一步，因為它：

- 計算簡單
- 行為直觀
- 對板端友善
- 不會因 prototype 數量較多而天然得利

---

## 7. 建議 API

```cpp
class FaceSearchV2 {
 public:
  explicit FaceSearchV2(const SearchConfig& config = SearchConfig {});

  bool LoadFromIndexPackage(const FaceSearchV2IndexPackage& package);
  bool LoadFromIndexPackagePath(const std::string& input_path);
  bool ExportIndexPackage(FaceSearchV2IndexPackage* out_package) const;
  bool SaveIndexPackageBinary(const std::string& output_path) const;
  bool AddPrototype(const FacePrototypeV2& prototype);
  bool RebuildFromPrototypes(const std::vector<FacePrototypeV2>& prototypes);
  void Clear();
  std::size_t PrototypeCount() const;
  SearchResultV2 Query(const std::vector<float>& embedding) const;
};
```

主線優先順序應是：

1. runtime / replay 優先直接 `LoadFromIndexPackagePath(...)`
2. import / build 階段使用 `RebuildFromPrototypes(...)`
3. `AddPrototype(...)` 保留給測試、工具或增量 builder

對固定 `512-d` hot path，建議再補一個 search-ready binary package 邊界：

- 存 person / prototype metadata
- 存已 normalize 的 contiguous matrix
- runtime 直接載入，不必再經過 store export 或重新 normalize

這種 package 比較適合作為板端 direct-load 產物，而不是把 CSV 視為正式 runtime 格式。
`AddPrototype(...)` / `RebuildFromPrototypes(...)` 比較像 build / import helper；runtime 主線應優先走 `LoadFromIndexPackagePath(...)`。
若是 build/test tooling 要一次完成 package 落盤，則可使用
`BuildAndSaveFaceSearchV2IndexPackageBinary(...)`。

在 V2 路徑上，建議優先順序為：

- runtime / replay 載入：`LoadFromIndexPackagePath(...)`
- build / import 階段：`RebuildFromPrototypes(...)`

格式細節請參考：

- `docs/search/search_index_package_binary_spec.md`

---

## 8. 與 Decision 的關係

Decision 不需要立即大改。

較務實的做法是：

- 讓 `Search V2` 的 person-level `hits` 仍維持與 V1 類似的使用語意
- `Decision` 先繼續看：
  - top-1
  - top-2 margin
  - temporal smoothing

同時在 debug / validation 需要時，再額外查看：

- `prototype_hits`
- `best_prototype_id`
- `best_zone`

---

## 9. 對產品驗證的好處

Search V2 做完後，後續會比較容易回答這些問題：

- 現在 top-1 是被哪個 zone 撐起來的
- recent adaptive 是否過度主導
- baseline 是否仍足夠穩
- 某次成功驗證是否值得納入 verified history

也就是說，它不只改善 search，也改善可解釋性。

---

## 10. 建議實作順序

若未來真的要落地 Search V2，建議順序如下：

1. 新增 V2 型別，不先破壞 V1 API
2. 補 prototype-level weighted score 測試
3. 補 person-level aggregation 測試
4. 再考慮是否要讓 pipeline 切到 V2

先不要直接把：

- `FaceSearch`
- `Decision`
- `Pipeline`

一起重構。
