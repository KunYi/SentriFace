# Search Index Package Binary Spec

## 1. 目的

這份文件定義 `FaceSearchV2IndexPackage` 的正式 binary format。

這個格式的定位是：

- 保存 search-ready index
- 直接提供 `FaceSearchV2` runtime 載入
- 避免每次啟動時重做 store export、normalize、matrix rebuild

目前副檔名固定為：

- `.sfsi`

---

## 2. 使用時機

`.sfsi` 適合用在：

- host/import 階段已完成 prototype collection
- search index 已可確定
- runtime 冷啟動時要快速 direct-load
- replay / validation 要重複載入同一份 search index

若需要保存更完整的 enrollment metadata 與 source traceability，應保留：

- `.sfbp`

`.sfsi` 不取代 `.sfbp`，而是 runtime 導向的衍生產物。

---

## 3. 資料模型

對應邏輯型別：

```cpp
struct FaceSearchV2IndexRecord {
  int person_id;
  int prototype_id;
  int zone;
  std::string label;
  float prototype_weight;
};

struct FaceSearchV2IndexPackage {
  int embedding_dim;
  std::vector<FaceSearchV2IndexRecord> entries;
  std::vector<float> normalized_matrix;
};
```

其中：

- `entries.size()` 代表 prototype row count
- `normalized_matrix.size()` 必須等於 `entries.size() * embedding_dim`
- matrix row order 必須與 `entries` 順序完全一致

---

## 4. Binary Layout

### 4.1 Endianness

第一版固定採：

- little-endian

### 4.2 Scalar encoding

- `u32`: unsigned 32-bit integer
- `f32`: IEEE-754 single precision float
- `string`: `u32 length` + raw bytes，不含 trailing `NUL`

### 4.3 File header

```text
magic[4]           = "SFSI"
version:u32        = 1
embedding_dim:u32
entry_count:u32
```

### 4.4 Entry layout

每個 entry 依序寫入：

```text
person_id:u32
prototype_id:u32
zone:u32
label:string
prototype_weight:f32
```

### 4.5 Matrix layout

entry metadata 全部寫完之後，再寫：

```text
normalized_matrix[entry_count * embedding_dim]:f32[]
```

這是 row-major contiguous layout：

- row `i` 對應 `entries[i]`
- row 內部按 embedding dimension 依序排列

---

## 5. Validity Rules

載入端至少應驗證：

- magic 必須為 `SFSI`
- version 目前只接受 `1`
- `embedding_dim > 0`
- `entry_count > 0`
- `normalized_matrix.size() == entry_count * embedding_dim`
- `person_id >= 0`
- `prototype_id >= 0`
- `zone` 必須落在已知 enum 範圍內
- `prototype_weight >= 0`
- 同一個 `person_id` 若出現多筆 entry，`label` 應保持一致
- `(person_id, prototype_id)` 組合不應重複

若格式合法但 embedding dim 不是 `512`，仍可接受。

實作層建議：

- `512-d` 走 dot kernel path
- 非 `512-d` 仍可直接使用 normalized matrix，但查詢走 scalar path

也就是說，`.sfsi` 是 search-ready format，不是 512-only format。

---

## 6. 設計取捨

第一版直接保存 normalized matrix，而不是原始 embedding。

這樣做的理由是：

- runtime 冷啟動更簡單
- 避免重做 normalize
- 避免 search import 時再多做一次 matrix build
- 使 `.sfsi` 真正成為 direct-load runtime package

代價是：

- `.sfsi` 不適合回推成完整 enrollment package
- 若要保留 source traceability，仍需保留 `.sfbp`

---

## 7. 與 `.sfbp` 的分工

`.sfbp` 保存：

- user / source metadata
- prototype package
- import / verify 邊界

`.sfsi` 保存：

- search metadata
- normalized matrix
- runtime direct-load 邊界

建議資料流：

1. host enrollment / embedding 先產生 `.sfbp`
2. import 階段再導出 `.sfsi`
3. runtime 優先載入 `.sfsi`

---

## 8. 對應 API

目前對應 API：

- `FaceSearchV2::ExportIndexPackage(...)`
- `FaceSearchV2::LoadFromIndexPackage(...)`
- `FaceSearchV2::LoadFromIndexPackagePath(...)`
- `FaceSearchV2::SaveIndexPackageBinary(...)`
- `BuildAndSaveFaceSearchV2IndexPackageBinary(...)`
- `SaveFaceSearchV2IndexPackageBinary(...)`
- `LoadFaceSearchV2IndexPackageBinary(...)`

相關文件：

- [docs/search/search_v2_interface_spec.md](search_v2_interface_spec.md)
- [docs/search/search_module_interface_spec.md](search_module_interface_spec.md)
- [docs/enrollment/baseline_prototype_package_binary_spec.md](../enrollment/baseline_prototype_package_binary_spec.md)
