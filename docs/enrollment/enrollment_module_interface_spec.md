# Enrollment Module Interface Spec

## 1. 目的

這份文件定義第一版 `enrollment` 模組介面。

第一版先專注於：

- person record 管理
- 多個 prototype embedding 保存
- 匯出給 search 模組使用

暫時不綁 UI、檔案格式或遠端資料庫。

第一版也刻意保持簡單：

- 不先做 prototype zones
- 不先做 adaptive update
- 不先做權重化搜尋

這些能力會在後續產品化階段再演進。

---

## 2. 介面定位

目前模組位置：

```text
include/enroll/enrollment_store.hpp
src/enroll/enrollment_store.cpp
```

主要類型：

- `EnrollmentConfig`
- `PersonRecord`
- `EnrollmentStore`

---

## 3. 第一版設計

`EnrollmentStore` 提供：

1. 新增或更新 person
2. 為 person 新增 prototype embedding
3. 刪除 person
4. 匯出所有 prototypes 給 search

```cpp
class EnrollmentStore {
 public:
  explicit EnrollmentStore(const EnrollmentConfig& config = EnrollmentConfig {});

  bool UpsertPerson(int person_id, const std::string& label);
  bool AddPrototype(int person_id, const std::vector<float>& embedding);
  bool RemovePerson(int person_id);
  void Clear();

  std::size_t PersonCount() const;
  std::size_t PrototypeCount(int person_id) const;
  std::vector<PersonRecord> GetPersons() const;
  std::vector<sentriface::search::FacePrototype> ExportPrototypes() const;
};
```

### 3.1 Prototype 上限

第一版每位 person 預設保留有限數量 prototype。
若超過上限，先移除最舊的 prototype。

---

## 4. CPU-First 與板端邊界

第一版 enrollment 先做：

- 記憶體內 person/prototype 管理

這符合 CPU-first 驗證需求，同時保持：

- person 資料結構簡單
- prototype 數量可控
- 匯出格式可直接供 search 使用

之後可再延伸成：

- 檔案持久化
- 板端快取
- 遠端同步

目前也已經建立一條更接近產品化的前置邊界：

- `include/enroll/enrollment_artifact_import.hpp`
- `src/enroll/enrollment_artifact_import.cpp`

這條邊界的目的不是直接替代 embedding/import 主流程，而是先把 host
enrollment tool 導出的：

- `enrollment_summary.txt`
- captured `frame_image`
- captured `face_crop`

解析成穩定的 `EnrollmentArtifactPackage`，讓後續：

- alignment
- embedding
- `EnrollmentStoreV2`

可以沿用同一種 import package，而不是各自重解 artifact 文字格式。

目前這條邊界也已進一步提供：

- `BuildBaselineEnrollmentPlan(...)`
- `GenerateBaselinePrototypePackage(...)`
- `GenerateMockBaselinePrototypePackage(...)`
- `LoadBaselinePrototypePackageFromEmbeddingCsv(...)`
- `enrollment_baseline_import_runner`

也就是先把 artifact 轉成：

- 哪些樣本應優先送去做 alignment / embedding
- `face_crop` 與 `frame_image` 何者應作為 preferred input

這讓後續 baseline prototype generation 可以先依同一組規則運作，而不是
把 sample 選擇邏輯散落在不同模組。

其中：

- `GenerateBaselinePrototypePackage(...)` 應作為正式 backend dispatch 邊界
- `GenerateMockBaselinePrototypePackage(...)` 仍保留作為 bring-up backend
- `LoadBaselinePrototypePackageFromEmbeddingCsv(...)` 應作為真實 baseline embedding CSV 的正式導入邊界

對應 guide：

- `docs/enrollment/enrollment_artifact_runner_guide.md`
- `docs/enrollment/baseline_embedding_import_workflow.md`
- `docs/enrollment/enrollment_baseline_import_runner_guide.md`
- `docs/enrollment/enrollment_baseline_verify_runner_guide.md`

而 `GenerateMockBaselinePrototypePackage(...)` 的定位很明確：

- 先打通 artifact -> prototype package -> store 的資料流
- 使用 deterministic mock embedding 做 bring-up
- 不應被誤解成最終量產版 embedding runtime

---

## 5. 後續產品化演進方向

雖然目前第一版只保留簡單 prototype list，但產品化方向已經明確：

- 不應直接覆蓋原始 enrollment baseline
- 成功驗證後的新 embedding 應該分區、分權重地吸收

建議未來演進成：

- `baseline zone`
- `verified history zone`
- `recent adaptive zone`

詳細策略請參考：

- `docs/enrollment/adaptive_prototype_policy.md`

若未來要正式將 `EnrollmentStore` 演進成產品化版本，建議介面與資料模型可參考：

- `docs/enrollment/enrollment_store_v2_spec.md`
