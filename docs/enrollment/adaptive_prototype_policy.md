# Adaptive Prototype Policy

## 1. 目的

這份文件定義本專案未來在 `enrollment / search / decision` 之間，如何安全地使用「成功驗證後的新 embedding」來更新 prototype。

重點不是做自動覆蓋，而是：

- 保留原始 enrollment 基準
- 在安全前提下逐步吸收新外觀
- 讓資料庫能隨時間適應使用者慢變

---

## 2. 為什麼需要這個策略

人臉不會永遠固定不變。

實務上會慢慢出現：

- 髮型變化
- 鬍子或化妝
- 眼鏡
- 年齡變化
- 季節性光線差異
- 門口不同時間段的成像條件

如果 prototype 永遠停在初始 enrollment，長期來看可能會讓：

- top-1 score 下降
- margin 變差
- 需要更保守 threshold

因此產品化方案應允許 prototype 緩慢更新，但不能犧牲安全性。

---

## 3. 基本原則

### 3.1 原始 enrollment 不覆蓋

最初註冊得到的 enrollment prototype 是最高信任基線。

因此：

- 不應直接刪除
- 不應被新的驗證樣本直接覆蓋

### 3.2 新樣本只能增量吸收

後續成功驗證得到的新 embedding，應該：

- 加入新的 zone
- 或替換 recent zone 中較舊的樣本

而不是直接改寫 baseline。

### 3.3 吸收要有安全條件

不是每次辨識成功都應直接更新。

只有在條件夠強時，才允許納入 adaptive prototype。

---

## 4. Prototype Zones

每位使用者的 prototype 建議分成三層。

### 4.1 Baseline Zone

用途：

- 保存 enrollment 原始基準

特性：

- 權重最高
- 容量小
- 幾乎不變

建議：

- `1 ~ 3` 個高品質 prototype

### 4.2 Verified History Zone

用途：

- 保存後續多次成功驗證的高信任 prototype

特性：

- 權重中高
- 更新較慢
- 用來吸收中長期外觀變化

建議：

- `3 ~ 8` 個 prototype
- 若以第一版產品化基線來看，可先收斂為 `最多 5 個`

### 4.3 Recent Adaptive Zone

用途：

- 保存近期成功驗證後的短期樣本

特性：

- 權重中低
- 容量小
- 可滾動替換

建議：

- `2 ~ 5` 個 prototype

---

## 5. 權重建議

Search / decision 不應把所有 prototype 視為同權重。

較合理的策略是：

- `baseline zone`: 最高權重
- `verified history zone`: 次高權重
- `recent adaptive zone`: 最低權重

這樣可以兼顧：

- 安全
- 穩定
- 對近期外觀變化的適應能力

---

## 6. 允許更新的條件

只有在以下條件足夠強時，才應允許把新 embedding 寫入 adaptive zones：

- `unlock_ready` 已成立
- temporal smoothing 已穩定
- `top-1` 穩定
- `top-1 vs top-2 margin` 達門檻
- face quality 達門檻
- liveness 或其他安全條件已通過

若其中任一條件不足，應只把這次結果當成一次成功通行事件，而不是 prototype 更新事件。

---

## 7. 建議更新策略

### 7.1 Recent Adaptive Zone

成功驗證後，新的 embedding 先考慮寫入 recent zone。

若 recent zone 已滿：

- 刪除最舊樣本
- 或刪除與現有樣本最相近、資訊增益最低的樣本

### 7.2 Verified History Zone

只有當某一批 recent samples 經過多次成功事件驗證，且長時間表現穩定時，才可晉升為 verified history。

這個條件應比 recent zone 更嚴格。

verified history 也必須有明確容量上限，不能無限制累積。

較務實的第一版規則是：

- 每人 verified history 最多保留 `5` 個
- 超過後採滾動更新
- 預設先淘汰最舊樣本

若未來需要更細緻，再考慮：

- 依資訊增益淘汰
- 依與 baseline / verified 群的冗餘度淘汰
- 依長期品質分數淘汰

### 7.3 Baseline Zone

baseline zone 預設不自動更新。

若未來需要調整，應該：

- 經過人工審核
- 或透過明確的 re-enrollment 流程

---

## 8. 與目前系統的關係

目前專案的 `EnrollmentStore` 仍是簡單版本：

- 不分 zone
- 不分權重
- 不做自動吸收

這是合理的第一版，因為目前重點仍是：

- 把整個 pipeline 跑通
- 建立穩定 replay / validation 基線

但後續若要產品化，這份策略應作為 enrollment store 演進的正式方向。

其中一條已確認的產品原則是：

- verified history 不能無限制增長
- 必須有固定上限與滾動淘汰機制

---

## 9. 建議的未來介面方向

後續 enrollment 模組可朝這些能力演進：

- `AddPrototype(person_id, zone, embedding, metadata)`
- `PromoteRecentToVerified(person_id, prototype_id)`
- `BuildFaceSearchV2IndexPackage(...)`
- `PruneAdaptivePrototypes(person_id)`

metadata 至少可包含：

- source type
- timestamp
- quality score
- decision confidence
- liveness result

目前專案第一版骨架已把這條路收斂成：

- `ShouldAcceptAdaptivePrototype(metadata)`
- `ShouldAcceptVerifiedHistoryPrototype(metadata)`
- `AddRecentAdaptivePrototype(...)`
- `AddVerifiedHistoryPrototype(...)`

也就是先把「是否允許吸收」和「真的寫入哪個 zone」拆開，方便後續由
`decision/pipeline` 在事件成立後再做最終決策。

其中 `ExportWeightedPrototypes()` 若仍保留，較適合作為 build / legacy compatibility helper；
runtime search 較建議直接使用 `.sfsi` / `FaceSearchV2IndexPackage`。
若 adaptive update 已實際寫入 `EnrollmentStoreV2`，則持有 search runtime 的
pipeline / runner 也應同步 refresh 內部 `Search V2` index，而不是只更新 store。
若 runtime 本來就是由 `.sfsi` 起始，則 adaptive update 所需的 store-side zone state
也應由同一份 `.sfsi` seed，而不是另走舊的 store-first 初始化。

---

## 10. 對本專案的意義

這份策略把一件重要事情定清楚：

- 本專案不是靜態 enrollment-only 系統
- 但也不是無限制自我學習系統

我們要的是：

- 保守
- 可控
- 有權重
- 能長期演進

這很符合門禁產品的安全需求。
