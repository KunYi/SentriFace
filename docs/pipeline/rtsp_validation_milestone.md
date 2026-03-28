# RTSP Validation Milestone

## 1. 目的

這份文件定義一條不阻塞主開發線的實機驗證 milestone：

- 既有硬體：`Luckfox Pico Max / RV1106G3`
- 已可提供：`RTSP video stream`
- 驗證工具：`ffmpeg`

這條 milestone 的角色不是取代目前的 `CPU-first` 模組開發，而是讓開發者/測試者可以提早開始：

- 實際影像流驗證
- 畫面方向與畫質確認
- 後續 detection/tracking/alignment 的真實輸入資料收集

---

## 2. 定位

目前專案主線仍然是：

- `CPU-first` 演算法與介面開發
- 但設計邊界以 `RV1106 + RKNN` 為主

RTSP milestone 的定位是：

- **早期實機驗證線**

也就是：

- 不阻塞模組開發
- 但能提早暴露板端影像流問題

---

## 3. 驗證目標

第一階段只驗證以下幾件事情：

1. `RV1106G3` RTSP 串流是否穩定
2. 是否能透過 `ffmpeg` 在開發端穩定抓流
3. 畫面方向是否符合直立模式設計假設
4. 畫面解析度、幀率、延遲是否落在可接受範圍
5. 是否能穩定抽幀供後續離線演算法測試

---

## 4. 建議驗證方式

### 4.1 基本連線驗證

使用 `ffmpeg` 連接 RTSP stream，確認：

- 可以正常接收畫面
- 沒有頻繁斷流
- 延遲在可接受範圍內

### 4.2 抽幀驗證

從 RTSP stream 定期抽取單張影像或短片段，用於：

- 檢查直立方向
- 檢查人臉大小
- 建立後續 detection/tracking 測試資料

### 4.3 短片段錄製

錄製：

- 單人靠近
- 單人停留
- 轉頭
- 短暫遮擋
- 兩人同框

供後續 regression 測試使用。

目前若要把 RTSP 抽幀直接送進 `SCRFD -> replay` 驗證線，可再搭配：

- `tools/run_scrfd_offline_replay.sh`
- `docs/detector/scrfd_offline_replay_end_to_end.md`

---

## 5. 驗證通過條件

若要視為 milestone 完成，至少應滿足：

1. 測試者可穩定抓取 RTSP stream
2. 可穩定輸出至少一批測試影片或影格
3. 已確認直立模式畫面方向
4. 已記錄解析度、幀率、基本延遲
5. 已整理一組可供離線演算法測試的樣本資料

---

## 6. 對後續開發的價值

這條 milestone 完成後，會直接幫助：

- `detector`
- `tracker`
- `alignment`
- `embedding`

因為之後可以不只用合成測試資料，而是開始用：

- 真實 RTSP 影像

做離線驗證。

這裡也有一個明確分工原則：

- 目前公開 still-image sample 只負責確認 pipeline 有跑通
- 正式的 threshold / false positive / false negative 判斷
  應交由開發者或測試者，使用實際 RTSP / 實際安裝條件下的資料來驗證

---

## 7. 注意事項

這條 milestone 不應讓主線開發停住。
建議維持這個原則：

- 主線：繼續 CPU-first 模組開發
- 驗證線：由開發者/測試者用 `RTSP + ffmpeg` 進行實機資料驗證

目前的優先順序是：

- 先把整個 pipeline 跑通
- 再把 production threshold 留給後續實測

這樣可以同時前進。
