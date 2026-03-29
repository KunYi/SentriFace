# Search Module Interface Spec

## 1. 目的

這份文件定義第一版 `search` 模組介面。

第一版先專注於：

- prototype 管理
- top-k candidate retrieval
- cosine similarity
- top-1 / top-2 margin

暫時不綁定 ANN、vector DB 或遠端服務。

---

## 2. 介面定位

目前模組位置：

```text
include/search/face_search.hpp
src/search/face_search.cpp
```

主要類型：

- `SearchConfig`
- `FacePrototype`
- `SearchHit`
- `SearchResult`
- `FaceSearch`

---

## 3. 第一版設計

`FaceSearch` 提供：

1. 加入 prototype
2. 清空 prototype
3. 對 query embedding 做 top-k 搜尋

```cpp
class FaceSearch {
 public:
  explicit FaceSearch(const SearchConfig& config = SearchConfig {});

  bool AddPrototype(const FacePrototype& prototype);
  bool RebuildFromPrototypes(const std::vector<FacePrototype>& prototypes);
  void Clear();
  std::size_t PrototypeCount() const;
  SearchResult Query(const std::vector<float>& embedding) const;
};
```

### 3.1 SearchResult

第一版輸出：

- `hits`
- `top1_top2_margin`

這樣能直接支援後續門禁 decision。

### 3.2 512-d implementation note

對固定 `512-d` embedding，較務實的 hot path 可採：

- metadata 與 embedding storage 分離
- embedding 進 index 時先 normalize
- 以 contiguous matrix 保存
- query 時走 batch dot kernel

這不改變 `FaceSearch` 對外 API，但可讓 local exact search 更接近
`RV1106` 的記憶體與延遲邊界。

若 enrollment / import 階段已經取得穩定的 `512-d` vectors，search 也可直接
透過 bulk rebuild 邊界載入，而不是由外層逐筆 `AddPrototype(...)`。

對 `Search V2` runtime 來說，正式主線則應再往前一步，優先 direct-load
`.sfsi` / `FaceSearchV2IndexPackage`；bulk rebuild 比較適合作為 build/import
階段的 helper。

若 search index 已在 import 階段建好，也建議保留 dedicated binary package，
直接保存：

- search metadata
- normalized contiguous matrix

讓 runtime 載入時不必重做 normalize / matrix rebuild。
若已經有 `.sfsi`，runtime 應優先 direct-load package；CSV 與 store export 只保留給 build / import。

因此建議優先順序為：

- runtime：優先 direct-load `.sfsi`
- build / import：再使用 bulk rebuild

對應格式規格：

- `docs/search/search_index_package_binary_spec.md`

### 3.3 排序穩定性

當 score 相同時，search 結果建議保留 deterministic tie-break。

這有助於：

- regression 比較
- replay 可重現性
- pipeline / decision debug

---

## 4. CPU-First 與 RKNN 邊界

第一版 search 先做：

- 本地 CPU prototype search

目前較務實的 local deployment 基線可先假設：

- `200 persons` 為第一驗證目標
- `200 ~ 500 persons` 為 local-first 產品規模

但仍要遵守板端導向原則：

- top-k 保持保守
- 不預設每幀都查庫
- 不把第一版寫成大型桌面專用搜尋器

之後可再延伸成：

- ANN
- remote search service
- hybrid local/remote search

若未來規模進入 `10,000+` persons，請不要直接假設 `RV1106` 仍應承擔完整
full-corpus search 主責。較合理的產品化方向請參考：

- `docs/search/large_scale_identity_search_strategy.md`

---

## 5. 後續產品化演進方向

當 enrollment store 進入 `prototype zones` 與 `adaptive update` 階段後，search 也不應再把所有 prototype 視為同權重。

建議後續演進方向為：

- baseline prototype 權重最高
- verified history 次高
- recent adaptive 最低

第一版較建議的產品化方向是：

- 先計算 prototype-level cosine similarity
- 再套用固定 zone weight
- person-level aggregation 先採 `max(weighted_score)`

詳細策略請參考：

- `docs/search/weighted_search_spec.md`

若未來要正式把 search 介面升級成 weighted prototype + person-level aggregation，可參考：

- `docs/search/search_v2_interface_spec.md`
