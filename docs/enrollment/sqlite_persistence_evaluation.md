# SQLite Persistence Evaluation

## 1. 結論

目前不建議把 SQLite3 作為 `SentriFace` 主線 runtime 的唯一正式資料格式。

較合理的定位是：

- 短中期：
  - 繼續以 `.sfbp` 作為 prototype package 邊界
  - 繼續以 `.sfsi` 作為 search-ready runtime 邊界
- 中後期：
  - 可引入 SQLite3 作為 host-side / management-side 的 metadata catalog
  - 不取代 `.sfsi` 的 direct-load runtime 角色

也就是說：

- SQLite3 `值得保留`
- 但 `現在不應該取代` `.sfbp / .sfsi`

---

## 2. 為什麼現在不要直接切 SQLite3

目前主線目標是：

- host enrollment artifact
- baseline package
- search-ready index
- replay / runtime direct-load

在這個階段，`.sfbp / .sfsi` 已經有幾個很明確的優點：

- 檔案格式固定
- cold start 載入直接
- 不需要 query planner / schema migration
- 對 `RV1106` cross-build 與 bring-up 較單純
- 對 batch replay / validation 比較透明

若現在直接把主線切成 SQLite3，會增加：

- 第三方依賴與 cross-build 風險
- schema versioning / migration 成本
- transaction / WAL / corruption recovery 議題
- runtime 載入路徑複雜度

而目前最需要的能力其實不是 SQL 查詢，而是：

- 穩定的 package 邊界
- 可重跑的 direct-load
- search-ready 的 fixed-format 載入

這些用 `.sfbp / .sfsi` 已經更直接。

---

## 3. SQLite3 真正有價值的地方

SQLite3 比較適合處理：

- user / person registry
- artifact inventory
- sample / crop / frame 路徑索引
- metadata 查詢
- enrollment history
- re-enroll / verified / adaptive 的管理介面
- host-side tooling 的批次查詢與維護

也就是說，它比較像：

- catalog
- control plane storage
- management database

而不是：

- hot-path search matrix format

---

## 4. 建議分層

較合理的未來分層如下：

### 4.1 Runtime artifacts

保留：

- `.sfbp`
- `.sfsi`

用途：

- deterministic package boundary
- board/runtime direct-load
- replay / validation input

### 4.2 SQLite catalog

之後若引入 SQLite3，建議只保存：

- person table
- prototype metadata table
- artifact file registry
- package file paths
- status / timestamps / zone bookkeeping

而 embedding bulk payload 與 normalized matrix 不建議直接變成主線查詢格式。

比較務實的做法是：

- SQLite 記錄 `.sfbp / .sfsi` 的檔案位置與版本
- runtime 仍直接載入 `.sfsi`

---

## 5. 不建議的方向

目前不建議：

- 讓 search runtime 直接從 SQLite row-by-row 重建 index
- 讓 `FaceSearchV2` 直接依賴 SQL query 當 hot path
- 把 `.sfsi` 拿掉只保留 SQLite blob

原因：

- 啟動路徑變長
- runtime 與管理層耦合
- 失去目前 binary package 的可攜性與透明度

---

## 6. 建議導入時機

較合理的導入時機是出現以下需求後：

- 人員資料數量變多
- enrollment / re-enrollment 管理變複雜
- 需要 query 歷史 / audit / artifact 關聯
- host tool 需要更強的管理能力
- package 檔案數量開始需要 catalog 才能維護

在這之前，先把：

- `.sfbp`
- `.sfsi`
- import / verify / runtime direct-load

做穩，投報比更高。

---

## 7. 目前決策

目前主線決策為：

1. 不把 SQLite3 納入當前 runtime 主線必要依賴。
2. `.sfbp` 與 `.sfsi` 維持正式 binary 邊界。
3. 後續若引入 SQLite3，優先定位為 metadata / package catalog。
4. runtime search 仍應優先 direct-load `.sfsi`。

---

## 8. 對應文件

- [docs/enrollment/baseline_prototype_package_binary_spec.md](baseline_prototype_package_binary_spec.md)
- [docs/search/search_index_package_binary_spec.md](../search/search_index_package_binary_spec.md)
- [docs/enrollment/enrollment_store_v2_spec.md](enrollment_store_v2_spec.md)
- [docs/project/decision_log.md](../project/decision_log.md)
