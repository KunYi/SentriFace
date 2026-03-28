# Enrollment Store V2 Spec

## 1. 目的

這份文件定義 `EnrollmentStore` 未來產品化版本的介面方向。

V2 的重點不是推翻第一版，而是在第一版可用的基礎上補上：

- prototype zones
- prototype metadata
- adaptive update policy
- weighted export

---

## 2. 設計目標

V2 應該解決的問題包括：

- 原始 enrollment prototype 不可直接覆蓋
- 成功驗證後的新 embedding 需要有安全的吸收路徑
- search 不能把所有 prototype 視為同權重
- 系統需要保留 prototype 的來源與信任等級

---

## 3. 與 V1 的關係

目前 V1：

- 每位使用者只有一個 prototype list
- prototype 無 zone
- prototype 無 metadata
- 匯出時所有 prototype 同權重

V2：

- 保留 V1 的簡單可用性
- 但在資料模型中引入 zone 與 metadata

---

## 4. 核心型別建議

### 4.1 PrototypeZone

```cpp
enum class PrototypeZone : std::uint8_t {
  kBaseline = 0,
  kVerifiedHistory = 1,
  kRecentAdaptive = 2,
};
```

### 4.2 PrototypeMetadata

```cpp
struct PrototypeMetadata {
  std::uint32_t timestamp_ms = 0;
  float quality_score = 0.0f;
  float decision_score = 0.0f;
  float top1_top2_margin = 0.0f;
  bool liveness_ok = false;
  bool manually_enrolled = false;
};
```

### 4.3 ZonedPrototype

```cpp
struct ZonedPrototype {
  int prototype_id = -1;
  PrototypeZone zone = PrototypeZone::kBaseline;
  std::vector<float> embedding;
  PrototypeMetadata metadata;
  float search_weight = 1.0f;
};
```

### 4.4 PersonRecordV2

```cpp
struct PersonRecordV2 {
  int person_id = -1;
  std::string label;
  std::vector<ZonedPrototype> prototypes;
};
```

---

## 5. Config 建議

```cpp
struct EnrollmentConfigV2 {
  int embedding_dim = 512;
  int max_baseline_prototypes = 3;
  int max_verified_history_prototypes = 5;
  int max_recent_adaptive_prototypes = 4;
  float baseline_weight = 1.00f;
  float verified_history_weight = 0.80f;
  float recent_adaptive_weight = 0.60f;
  float min_adaptive_quality_score = 0.80f;
  float min_adaptive_decision_score = 0.85f;
  float min_adaptive_margin = 0.05f;
  bool require_adaptive_liveness = true;
  float min_verified_quality_score = 0.85f;
  float min_verified_decision_score = 0.90f;
  float min_verified_margin = 0.08f;
  bool require_verified_liveness = true;
};
```

---

## 6. 核心 API 建議

```cpp
class EnrollmentStoreV2 {
 public:
  explicit EnrollmentStoreV2(
      const EnrollmentConfigV2& config = EnrollmentConfigV2 {});

  bool UpsertPerson(int person_id, const std::string& label);
  bool RemovePerson(int person_id);
  void Clear();

  bool AddPrototype(int person_id,
                    PrototypeZone zone,
                    const std::vector<float>& embedding,
                    const PrototypeMetadata& metadata);
  bool AddRecentAdaptivePrototype(int person_id,
                                  const std::vector<float>& embedding,
                                  const PrototypeMetadata& metadata);
  bool AddVerifiedHistoryPrototype(int person_id,
                                   const std::vector<float>& embedding,
                                   const PrototypeMetadata& metadata);

  bool ShouldAcceptAdaptivePrototype(const PrototypeMetadata& metadata) const;
  bool ShouldAcceptVerifiedHistoryPrototype(const PrototypeMetadata& metadata) const;

  bool PromotePrototype(int person_id, int prototype_id, PrototypeZone new_zone);
  bool RemovePrototype(int person_id, int prototype_id);

  std::size_t PersonCount() const;
  std::size_t PrototypeCount(int person_id) const;
  std::size_t PrototypeCount(int person_id, PrototypeZone zone) const;

  std::vector<PersonRecordV2> GetPersons() const;
  std::vector<sentriface::search::FacePrototype> ExportWeightedPrototypes() const;
};
```

---

## 7. Zone 寫入規則

### 7.1 Baseline

只允許：

- enrollment
- re-enrollment
- 明確人工操作

不允許：

- 一般通行成功後自動寫入

### 7.2 Verified History

只允許來自：

- 已多次成功驗證
- 長期表現穩定
- 通過較嚴格安全條件的樣本

而且必須受固定容量限制保護。

第一版建議：

- `max_verified_history_prototypes = 5`
- 超過後以滾動方式淘汰最舊樣本

### 7.3 Recent Adaptive

允許來自：

- 本次成功驗證事件
- 且已滿足 update policy 的安全條件

目前第一版骨架已將這些安全條件明確化為：

- `quality_score`
- `decision_score`
- `top1_top2_margin`
- `liveness_ok`

也就是說，adaptive / verified 寫入不再只是單純 `AddPrototype(zone=...)`，
而是應優先透過：

- `AddRecentAdaptivePrototype(...)`
- `AddVerifiedHistoryPrototype(...)`

來走受控入口。

---

## 8. Capacity Policy

不同 zone 應各自有限額。

若超過限額：

- baseline zone: 預設拒絕寫入，不自動淘汰
- verified history zone: 第一版先刪除最舊樣本，後續才考慮刪除最冗餘樣本
- recent adaptive zone: 刪除最舊樣本，或刪除資訊增益最低樣本

---

## 9. Export 給 Search 的語意

V2 匯出時，search 不應只拿到：

- `person_id`
- `label`
- `embedding`

而應能保留 zone 與 weight 概念。

若第一版 search 介面暫時不想改太大，可採兩步：

1. enrollment 先把 zone weight 折疊成 `FacePrototype` 的 label / metadata 擴充欄位
2. search V2 再正式支援 weighted prototypes

較理想的未來方向是：

```cpp
struct FacePrototypeV2 {
  int person_id = -1;
  std::string label;
  std::vector<float> embedding;
  float prototype_weight = 1.0f;
  int prototype_id = -1;
  int zone = 0;
};
```

---

## 10. 與 Decision / Pipeline 的關係

V2 不應由 enrollment 模組自行判斷何時吸收新 prototype。

較合理的責任分工是：

- `decision/pipeline` 判斷這次事件是否足夠可信
- `enrollment` 只負責保存與管理 prototype

也就是說，未來應存在類似這種資料流：

1. decision 判斷 `unlock_ready`
2. decision / pipeline 檢查 margin、quality、liveness
3. 若符合 policy，才呼叫 enrollment 寫入 `recent adaptive zone`

目前最小骨架已支援：

- `ShouldAcceptAdaptivePrototype(...)`
- `ShouldAcceptVerifiedHistoryPrototype(...)`

因此後續 `pipeline` 若要真正接上 adaptive update，只需要在事件成立後把
metadata 整理好，再呼叫對應 API。

---

## 11. 對 RV1106 的實務意義

這個設計的重點不是把板端資料結構做得很豪華，而是：

- 讓 search 更穩
- 讓資料庫長期可演進
- 保持安全邊界

在 `RV1106` 上，第一版實作仍應保持簡單：

- zone 數量固定
- metadata 精簡
- 不做複雜背景同步邏輯

另外，host enrollment bring-up 目前已經會輸出：

- captured full frame
- captured face crop

而且可透過：

- `LoadEnrollmentArtifactPackage(...)`
- `BuildBaselineEnrollmentPlan(...)`
- `GenerateBaselinePrototypePackage(...)`
- `GenerateMockBaselinePrototypePackage(...)`
- `LoadBaselinePrototypePackageFromEmbeddingCsv(...)`

先載回為 import package，再由後續 alignment / embedding 流程決定如何生成
baseline prototypes 並寫入 `EnrollmentStoreV2`。

---

## 12. 建議實作順序

若未來真的要落實 V2，建議順序如下：

1. 新增 zone 與 metadata 結構
2. 讓 enrollment store 內部先能分 zone 保存
3. 新增 `PrototypeCount(person_id, zone)`
4. 新增 `ExportWeightedPrototypes()`
5. 再決定 search 是否升級成 weighted search

先不要一開始就把：

- zone
- adaptive update
- weighted search
- persistent storage

全部一起改。

weighted search 的細節建議可參考：

- `docs/search/weighted_search_spec.md`
