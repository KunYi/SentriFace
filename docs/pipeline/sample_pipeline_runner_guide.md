# Sample Pipeline Runner Guide

## 1. 目的

這份文件說明目前專案提供的最小可執行 sample runner。

它的角色是：

- 不依賴真實模型 backend
- 不依賴真實 embedding backend
- 先把 `frame source -> detector -> tracker -> enrollment -> search -> decision -> pipeline glue` 跑通一次

這很適合：

- 開發者快速確認骨架是否正常
- 未來接 RTSP / ffmpeg 前先驗證主資料流

---

## 2. 執行檔

建置後可用：

```bash
./build/sample_pipeline_runner
SENTRIFACE_PIPELINE_USE_V2=1 ./build/sample_pipeline_runner
```

它會：

1. 建立一個 `SequenceFrameSource`
2. 建立一個 `SequenceFaceDetector`
2. 建立一個 sample enrollment person
3. 載入 pipeline
4. 對每個 sample frame 執行模擬 `detect -> track -> search -> decision`
5. 印出每一幀的 decision 狀態
6. 額外示範一次 short-gap merge
7. 輸出 `sample_pipeline_report.txt`

若指定：

```bash
SENTRIFACE_PIPELINE_USE_V2=1
```

則 sample runner 會改走：

- `FaceSearchV2`
- `FacePipeline` 的 package-first V2 相容路徑
- `EnrollmentStoreV2` 只保留作為相容 / adaptive state 容器

並在 stdout 額外輸出：

- `pipeline_mode=v2`

若同時指定：

```bash
SENTRIFACE_SEARCH_INDEX_PATH=/path/to/search_index.sfsi
```

則 V2 runner 會優先直接載入 `.sfsi` search-ready index。
若只持有 `.sfbp`，也可指定：

```bash
SENTRIFACE_BASELINE_PACKAGE_PATH=/path/to/baseline_package.sfbp
```

runner 會透過正式 `.sfbp -> .sfsi` helper 載入。
未指定時，才退回 sample 內建的 package rebuild 路徑。

若要指定 `.sfbp` 載入後在 runtime 內對應的 `person_id`，可再加上：

```bash
SENTRIFACE_BASELINE_PERSON_ID=7
```

未指定時預設為 `1`。

---

## 3. 目前用途

這個 runner 不是最終應用程式。  
它是目前 CPU-first 架構下的：

- 最小整合驗證入口

後續可以沿這個方向演進成：

- 離線影片 runner
- RTSP runner
- 板端 camera runner

---

## 4. Short-Gap Merge 驗證

目前 sample runner 也會額外跑一段簡化情境，用來驗證：

- 舊 track 先消失
- 新 track 在短時間內重新出現
- 若 search / decision 結果一致

則 pipeline 會把舊 track 的 decision history 合併到新 track。

這段驗證的目的不是模擬真實 detector，而是確認：

- `FacePipeline`
- `FaceDecisionEngine`

這兩層對門禁最重要的「短斷軌仍視為同一次通行事件」邏輯已經成立。
