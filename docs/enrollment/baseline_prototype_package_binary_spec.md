# Baseline Prototype Package Binary Spec

## 1. 目的

這份文件定義 `BaselinePrototypePackage` 的正式 binary format。

這個格式的定位是：

- 保存 host enrollment / baseline embedding 已產生的 prototype package
- 作為 `.sfsi` 生成前的正式 prototype package 邊界
- 作為 import / verify / package rebuild 的正式 direct-load 邊界
- 避免 runtime 每次都回到 CSV 重新 parse

目前副檔名固定為：

- `.sfbp`

---

## 2. 使用時機

`.sfbp` 適合用在：

- host enrollment 已完成 sample selection
- alignment / embedding 已產出穩定 `512-d` vectors
- import / verify runner 要反覆重跑
- 需要保存 metadata 與 embedding 的正式 package

CSV 仍可保留作為：

- 互通格式
- bring-up / debug 格式
- 外部 embedding workflow 的橋接格式

但 runtime 不應把 CSV 視為正式 direct-load 邊界；正式主線應優先走 `.sfbp -> .sfsi`。

---

## 3. 資料模型

對應邏輯型別：

```cpp
struct BaselinePrototypeRecord {
  int sample_index;
  std::string slot;
  std::string source_image_path;
  std::string source_image_digest;
  std::vector<float> embedding;
  PrototypeMetadata metadata;
};

struct BaselinePrototypePackage {
  std::string user_id;
  std::string user_name;
  std::vector<BaselinePrototypeRecord> prototypes;
};
```

每個 record 至少需要保存：

- sample identity
- source image traceability
- prototype metadata
- fixed-dim `float32` embedding

---

## 4. Binary Layout

### 4.1 Endianness

第一版固定採：

- little-endian

### 4.2 Scalar encoding

- `u32`: unsigned 32-bit integer
- `f32`: IEEE-754 single precision float
- `bool`: 1 byte，`0` 或 `1`
- `string`: `u32 length` + raw bytes，不含 trailing `NUL`

### 4.3 File header

```text
magic[4]           = "SFBP"
version:u32        = 1
embedding_dim:u32
prototype_count:u32
user_id:string
user_name:string
```

### 4.4 Record layout

每個 prototype record 依序寫入：

```text
sample_index:u32
slot:string
source_image_path:string
source_image_digest:string
metadata.timestamp_ms:u32
metadata.quality_score:f32
metadata.decision_score:f32
metadata.top1_top2_margin:f32
metadata.sample_weight:f32
metadata.liveness_ok:bool
metadata.manually_enrolled:bool
embedding[embedding_dim]:f32[]
```

---

## 5. Validity Rules

載入端至少應驗證：

- magic 必須為 `SFBP`
- version 目前只接受 `1`
- `embedding_dim > 0`
- `prototype_count > 0`
- `user_id` 不可為空
- `user_name` 不可為空
- 每個 record 都必須有完整 `embedding_dim` 個 `f32`

若任一條件不成立，應回傳 load failure，而不是接受部分資料。

---

## 6. 設計取捨

第一版不做：

- 壓縮
- checksum
- 多版本 record variant
- mmap-oriented alignment padding

原因是目前目標優先順序為：

- 邊界清楚
- 容易 debug
- 容易在 host / board 兩端穩定落地

若後續需要：

- 更嚴格完整性驗證
- 更大 corpus
- 更快冷啟動

再考慮加入 checksum、block layout 或壓縮。

---

## 7. 與 `.sfsi` 的分工

`.sfbp` 保存的是：

- prototype package
- source metadata
- import 前後都可使用的中間正式產物

`.sfsi` 保存的是：

- search-ready normalized index
- runtime 可直接載入的 search 資料

也就是說：

- `.sfbp` 偏 enrollment / import 邊界
- `.sfsi` 偏 search runtime 邊界

---

## 8. 對應 API

目前對應 API：

- `SaveBaselinePrototypePackageBinary(...)`
- `LoadBaselinePrototypePackageBinary(...)`
- `LoadBaselinePrototypePackage(...)`
- `LoadBaselinePrototypePackageIntoStoreV2(...)`

相關文件：

- [docs/enrollment/enrollment_module_interface_spec.md](enrollment_module_interface_spec.md)
- [docs/enrollment/baseline_embedding_import_workflow.md](baseline_embedding_import_workflow.md)
- [docs/search/search_index_package_binary_spec.md](../search/search_index_package_binary_spec.md)
