# Feature-Only Cloud Adaptation Spec

## 1. 目的

這份文件定義一種可能的中長期產品方向：

- 不上傳原始照片
- 不上傳可直接回辨識到自然人的本地映射
- 只上傳 tenant-scoped、去識別化的 feature telemetry
- 用於雲端分析、校準與策略優化

這份文件不是立即交付項，而是：

- 架構預研
- 風險邊界定義
- 後續 SaaS 化的規格草案

---

## 2. 核心目標

這條路線希望同時滿足：

1. 比 image upload 方案更保守的隱私風險
2. 保留跨裝置、跨客戶的品質分析與模型改進可能性
3. 避免把 identity mapping 暴露到雲端
4. 讓本地設備仍維持 local-first 控制權

---

## 3. 基本原則

### 3.1 本地保留身份綁定

下列資料只應存在客戶本地設備或客戶自有系統：

- `person_id -> 真實自然人`
- 使用者姓名
- 原始 face crop / frame
- enrollment 人像原始資料

雲端不應直接持有上述資料。

### 3.2 雲端只收 feature telemetry

雲端可考慮接收：

- embedding / feature vector
- search score
- top-k result score distribution
- quality score
- liveness result
- detector / alignment quality metadata
- timestamp bucket
- camera/environment class
- tenant-scoped pseudonymous UUID

### 3.3 tenant-scoped pseudonymization

雲端側應只看得到：

- tenant identifier
- pseudonymous subject UUID

但不應直接知道：

- 這個 UUID 對應哪個真實人

UUID 與本地 person record 的映射應只保存在客戶本地。

---

## 4. 不可上傳資料

預設不應上傳：

- 原始 frame
- 原始 face crop
- 可逆的身份映射表
- 使用者姓名、工號、門禁真實身份資料
- 任何可直接在雲端重新識別自然人的資料

若未來真的要上傳影像，必須視為另一種更高風險產品模式，不可混寫。

---

## 5. 可上傳資料欄位

### 5.1 最小可行集合

可先考慮：

- `tenant_id`
- `subject_uuid`
- `device_id`
- `event_id`
- `embedding`
- `quality_score`
- `liveness_score` 或 `liveness_ok`
- `top1_score`
- `top2_score`
- `margin`
- `decision_outcome`
- `environment_tag`
- `timestamp_bucket`

### 5.2 設計要求

所有上傳欄位都應符合：

- 最小必要性
- 不可直接回推出自然人身份
- 可支援 tenant-isolated 分析

---

## 6. 可做的雲端學習

這條架構比較適合做：

- score calibration
- threshold recommendation
- false accept / false reject risk analysis
- quality-to-confidence mapping
- liveness / quality / search fusion policy
- adaptive prototype update policy tuning
- cross-device drift detection

也就是：

- 在 feature-space / score-space 做學習

---

## 7. 不建議直接做的事

這條架構不適合直接當成：

- image-based backbone retraining
- detector retraining
- alignment retraining
- 需要原始影像 supervision 的大模型微調

因為只靠 embedding / score telemetry，通常不足以：

- 改善 detector 漏檢
- 修正 alignment 壞掉
- 重建 image-domain 問題

---

## 8. 與合規的關係

這種做法的風險低於 image upload，但不代表：

- 完全匿名
- 完全沒有法規問題
- 完全不需要告知與同意

比較準確的描述應是：

- `tenant-scoped pseudonymized biometric telemetry`

而不是：

- `privacy-free`

---

## 9. 產品價值

這條路的價值主要是：

- 不碰原始照片也能做大量分析
- 比 image upload 更容易推進企業接受度
- 資料量較小
- 比較適合作為 SaaS 增值服務

---

## 10. 主要風險

仍需注意：

- feature vector 仍可能被視為 biometric template
- tenant + time + device + repeated UUID pattern 可能構成可再關聯風險
- 若上傳欄位設計過多，仍可能破壞去識別化目標
- 自動回寫策略若缺乏驗證，可能引入 decision drift

---

## 11. 建議 rollout 順序

### Phase 1

- 只做 feature telemetry upload
- 只做 dashboard / analytics
- 不自動回寫模型

### Phase 2

- 做 threshold / policy recommendation
- 由人工審核後再套用

### Phase 3

- 再評估是否做 tenant-level policy auto-tuning

不建議一開始就做：

- 自動微調 backbone
- 自動重新部署新模型

---

## 12. 當前結論

一句話總結：

- 若未來要做 SaaS，`feature-only cloud adaptation` 是比 `image-based automatic retraining` 更保守、也更可能產品化的方向，但仍必須強調合規、同意、刪除與不得挪用原則。

