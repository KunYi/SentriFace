# Biometric Cloud Consent And Retention Spec

## 1. 目的

這份文件定義若未來採用雲端 feature telemetry / cloud adaptation 路線時，
應遵守的：

- 法律合規注意事項
- 使用者告知與同意原則
- 資料保留與刪除原則
- 不得挪用原則

這份文件不是法律意見。

所有正式產品落地前，仍必須由：

- 客戶法務
- 隱私/資安負責人
- 適用法域的專業顧問

共同審核。

---

## 2. 核心原則

### 2.1 合規優先

任何雲端生物特徵資料方案，都必須先問：

- 在目標法域是否合法
- 是否需要明示同意
- 是否允許跨境
- 是否允許作訓練/分析用途

不得以工程方便為由跳過。

### 2.2 明示告知

若未來啟用這條功能，至少應向客戶與最終使用者清楚說明：

- 會收集什麼資料
- 不會收集什麼資料
- 資料用途是什麼
- 資料保留多久
- 誰可以存取
- 是否會用於模型/策略優化
- 訓練或分析完成後會如何刪除

### 2.3 明確同意

若法域或客戶政策要求，應採：

- 明確同意
- 可追溯記錄
- 可撤回

不應依賴模糊、預設勾選或不可理解的條款。

---

## 3. 不得宣稱的事

文件與對外說明不應宣稱：

- 完全匿名
- 完全沒有合規風險
- 一定不構成個資 / 生物特徵資料

較安全的說法是：

- 風險較 image upload 低
- 採去識別化與 tenant-scoped pseudonymization
- 仍需依適用法規與客戶政策進行審查

---

## 4. 同意與啟用條件

建議至少包含：

- 客戶租戶層級的功能啟用開關
- 客戶管理者的明確啟用確認
- 若需要，終端使用者的明示同意
- 可審計的 consent record

未完成上述條件時，預設應：

- 不上傳任何 feature telemetry 到雲端

---

## 5. 資料最小化

只應收集達成目的所必需的資料。

原則：

- 能不收就不收
- 能聚合就不逐筆
- 能不帶時間精度就降精度
- 能不帶可再關聯欄位就去掉

---

## 6. 資料保留與刪除

### 6.1 預設原則

應明確寫入：

- feature telemetry 只用於既定分析/優化目的
- 訓練或分析完成後應依 retention policy 刪除
- 不得無限期保留

### 6.2 建議做法

至少要定義：

- raw telemetry retention
- aggregated telemetry retention
- model training intermediate artifact retention
- backup retention
- deletion propagation latency

### 6.3 使用者或客戶要求刪除

若客戶或法規要求刪除，應能處理：

- 特定 tenant
- 特定 subject UUID
- 特定時間窗
- 相關衍生中間資料

---

## 7. 不得挪用

這條原則必須明寫：

- 收集的 feature telemetry 不得挪作未告知用途
- 不得默默轉成 image reconstruction 研究
- 不得作未經同意的 cross-tenant 身份分析
- 不得挪作廣告、行銷、無關產品分析

若未來要新增用途，應：

- 重新做合規審查
- 重新做告知
- 視需要重新取得同意

---

## 8. 安全控制

至少應要求：

- 傳輸加密
- 靜態加密
- tenant 隔離
- 金鑰管理
- 權限最小化
- 審計日誌

若本地設備保存 re-identification mapping，也應明確要求：

- mapping 不得上傳
- mapping 應本地加密保存

---

## 9. 模型/策略輸出邊界

若雲端基於這些資料產生：

- threshold recommendation
- policy pack
- score calibration model

則回推到設備前應有：

- 驗證
- 版本化
- 回滾能力
- tenant scope 控制

不應採：

- 未驗證直接全量自動覆蓋

---

## 10. 當前結論

一句話總結：

- 若未來要做 feature-only cloud adaptation，必須同時設計合規、明示同意、資料最小化、訓練後刪除與不得挪用原則；這些不是附加文件，而是產品設計的一部分。

