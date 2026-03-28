# RV1106 Numeric Type Policy

## 1. 目的

這份文件定義 `RV1106G2 / RV1106G3` 目標平台上的數值型別使用原則。

重點不是追求理論最通用的型別，而是：

- 符合 `32-bit` 平台現實
- 避免不必要的 `64-bit` 成本
- 避免把桌面 CPU 習慣直接帶進板端主線

---

## 2. 基本原則

對 `RV1106` 板端程式碼，預設遵守：

- 能用 `int` / `std::int32_t` / `std::uint32_t`，就不要先用 `64-bit`
- 能用整數表示的量，就不要先用浮點
- 能用 `float`，就不要用 `double`

簡單說就是：

- 先 `32-bit integer`
- 再 `float`
- 最後才考慮 `64-bit`

---

## 3. 建議型別對照

建議如下：

- frame width / height / channels: `int`
- track id / person id / counters: `int`
- timestamp in milliseconds: `std::uint32_t`
- small enum / state: `std::uint8_t`
- model score / IoU / geometry / Kalman state: `float`

不建議作為預設：

- `std::int64_t`
- `std::uint64_t`
- `double`

---

## 4. 允許例外

以下情況允許保留較重型別，但應有明確理由：

1. 外部 API 或第三方 runtime 強制要求
   例如某些 runtime tensor shape API 使用 `int64`

2. 長時間單調遞增值確實可能超出 `uint32_t`
   例如超長時間桌面錄製分析工具

3. 演算法真的需要浮點
   例如：
   - bbox 幾何
   - landmarks
   - IoU
   - cosine similarity
   - Kalman filter
   - neural network tensor

---

## 5. 對目前專案的實際解讀

目前專案仍有一些 `std::uint64_t timestamp_ms`，這在桌面驗證階段可以接受，
但不應被視為板端最終最佳型別。

後續若開始明確收斂板端路徑，優先考慮：

- 將 timestamp 主線型別逐步收斂為 `std::uint32_t`
- 保留 `float` 於幾何與模型相關模組
- 避免新增 `double`
- 避免在非必要處引入新的 `64-bit` 狀態欄位

---

## 6. 與門禁系統的關係

對本專案來說，真正需要數值精度的地方主要是：

- detection geometry
- tracking / Kalman
- alignment
- embedding / search / similarity

這些地方使用 `float` 是合理的。

但像是：

- timestamp
- counters
- state
- config flags

則應盡量維持在 `32-bit` 或更小的型別上。
