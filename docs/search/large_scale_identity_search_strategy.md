# Large-Scale Identity Search Strategy

## 1. 目的

這份文件定義當人臉資料庫擴大到：

- `10,000+` persons

時，本專案在：

- storage
- index
- search
- deployment split

上的產品化策略。

重點不是現在就把系統做成大型向量資料庫，而是先避免後面在：

- `EnrollmentStoreV2`
- `FaceSearchV2`
- host enrollment artifact
- `RV1106` 板端部署

之間做出互相衝突的決策。

---

## 2. 先講結論

對這個產品來說，`10,000+` 人的資料量不應預設由 `RV1106` 直接承擔完整搜尋主責。

較合理的方向是：

- `RV1106`：
  - detector
  - tracker
  - alignment
  - embedding
  - 小型 local cache / 小型 local index
  - 即時門禁事件節流與初步 decision
- host / server：
  - enrollment artifact 管理
  - baseline / verified / adaptive prototype 持久化
  - 大型 index
  - large-scale identity search
  - 後台重建 index / re-ranking / audit

也就是：

- 小型部署可全本地
- `10k+` 規模應優先考慮 **host/server 分層**

目前較務實的本地端產品基線可先定為：

- `200 persons`：第一個明確驗證基準
- `200 ~ 500 persons`：local-first 目標規模

這很符合一般小公司、中小型辦公室與單點門禁部署的實際起始需求。

---

## 3. 為什麼 10k 需要重新思考

假設：

- 每人 `3 ~ 5` 個 prototypes
- embedding dim `512`

那 `10,000` 人就會變成：

- `30,000 ~ 50,000` prototypes

這還不算：

- verified history 滾動更新
- recent adaptive
- index metadata
- 持久化格式
- debug / audit 資訊

所以真正要處理的不是只有：

- 「一個 cosine similarity 算不算得動」

而是整體：

- 記憶體
- 初始化時間
- 更新成本
- index rebuild
- query latency
- audit / maintainability

---

## 4. 規模分層建議

### 4.1 小型規模

適用於：

- `200 ~ 500` persons 為優先產品基線
- `<= 1,000` persons 為可持續觀察的本地端上限區
- prototype 總量仍保守

可接受策略：

- `CPU brute-force search`
- local weighted search
- 單機管理 prototype store

其中第一階段最建議先凍結並驗證的是：

- `200 persons`
- 每人 `3 ~ 5` 個 prototypes
- exact cosine + weighted prototype + person-level max aggregation

### 4.2 中型規模

適用於：

- 約 `1,000 ~ 5,000` persons

建議策略：

- 仍可保留 local exact search 作為 baseline
- 但開始明確區分：
  - local runtime search
  - background storage / rebuild
- 可以先準備 host-side index rebuild workflow

### 4.3 大型規模

適用於：

- `10,000+` persons

建議策略：

- `RV1106` 不預設保存完整 large-scale searchable corpus
- local 端只保留：
  - hot cache
  - site-specific allowlist
  - recent verified subset
- 完整 search 由 host/server 執行

---

## 5. Storage 策略

### 5.1 Runtime Store

板端 runtime store 應偏小、偏快：

- 只保留必要 metadata
- 只保留必要 prototype subset
- 優先支援低延遲事件決策

### 5.2 Durable Store

真正的大型資料庫應在 host/server：

- person identity
- baseline prototypes
- verified history
- recent adaptive
- artifact references
- index version

### 5.3 Artifact Layer

host enrollment 現在已經有：

- full frame
- face crop
- summary metadata

這層 artifact 很重要，因為當：

- embedding model 升版
- alignment 策略調整
- index 需要重建

時，可以從 artifact 重新生成更穩的 baseline，而不是只剩舊向量。

---

## 6. Index 策略

### 6.1 第一版

第一版仍維持：

- exact cosine search
- weighted prototype
- person-level max aggregation

理由：

- 行為可解釋
- debug 容易
- 和目前 decision 相容

### 6.2 10k+ 之後

當規模進入 `10k+`，應把 index 分成兩層：

- offline buildable index
- runtime queryable index

可接受的演進方向：

- ANN / HNSW 類索引
- coarse-to-fine search
- local cache + remote full search

但這些不應在第一版就硬做進 `RV1106` 主線。

---

## 7. Query 策略

### 7.1 板端

板端 query 應保持保守：

- 不每幀查
- 只對穩定 track 查
- query top-k 保守
- 與 relink / cooldown / event gating 一起工作

### 7.2 Host / Server

host/server query 可承擔：

- full corpus search
- rerank
- prototype-level debug
- audit log
- re-index

---

## 8. 對本專案的實務結論

目前這個專案最務實的方向是：

1. 先把 host enrollment artifact / import / prototype store 流程收斂
2. search V2 先維持 exact + weighted prototype
3. `10k+` 規模時，把 large-scale search 主責放到 host/server
4. `RV1106` 只保留必要 local cache 與門禁即時判斷能力

而在這兩者之間，當前最合理的 local deployment 假設是：

- `200 persons` 作為 baseline validation target
- `500 persons` 作為下一步擴展觀察目標

也就是說：

- 不要現在就把板端綁死成大型 index 節點
- 但也不要忽略 `10k+` 規模一定會帶來的 storage/index/search 分層需求

---

## 9. 目前建議的產品化路線

### Phase A

- host enrollment artifact 完整化
- import package 穩定化
- baseline prototype generation

### Phase B

- `EnrollmentStoreV2` 持久化格式
- host-side durable storage
- weighted search 持續驗證

### Phase C

- `10k+` 規模 search architecture
- local cache / remote full search split
- index rebuild / versioning / sync policy

---

## 10. 現階段不應假設的事

目前不應直接假設：

- `RV1106` 可以舒適承擔 `10k+` full search
- `CPU brute-force` 永遠足夠
- prototype 增長不會影響初始化與記憶體
- ANN 一定要先做進板端

正確做法是：

- 先讓資料邊界與 artifact 邊界正確
- 再根據真實規模決定 local/remote split
