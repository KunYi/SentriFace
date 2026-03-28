# Offline Sequence Runner Guide

## 1. 目的

這份文件說明目前專案提供的第一版 `offline_sequence_runner`。

它的角色是：

- 接收 manifest 驅動的離線 frame sequence
- 驗證 `camera::IFrameSource` 的另一條真實資料入口
- 為未來 RTSP 抽幀 artifacts 導入主資料流做準備

---

## 2. 執行檔

建置後可用：

```bash
./build/offline_sequence_runner <manifest.txt>
./build/offline_sequence_runner <frame_manifest.txt> <detection_manifest.txt>
./build/offline_sequence_runner <frame_manifest.txt> <detection_manifest.txt> <embedding_manifest.txt>
SENTRIFACE_PIPELINE_USE_V2=1 ./build/offline_sequence_runner <frame_manifest.txt> <detection_manifest.txt> [embedding_manifest.txt]
```

目前它會：

1. 讀取 manifest
2. 逐幀讀出 `Frame`
3. 若提供 detection manifest，則回放 detection 給 tracker
4. 統計處理幀數
5. 輸出 `offline_sequence_report.txt`
6. 若有 tracker replay，額外輸出 `offline_tracker_timeline.csv`
7. 若有 tracker replay，額外輸出 `offline_decision_timeline.csv`
8. 若 decision 進入 `unlock_ready`，可透過 unlock backend 驗證 unlock event

---

## 3. Manifest 格式

每一行定義一個 frame：

```text
<path> <timestamp_ms> <width> <height> <channels> <pixel_format>
```

範例：

```text
frame_000.jpg 1000 640 640 3 rgb888
frame_001.jpg 1033 640 640 3 rgb888
```

目前支援：

- `rgb888`
- `bgr888`
- `gray8`

如果 manifest 來源是 RTSP 抽幀 artifacts，可參考：

- `docs/pipeline/rtsp_artifact_to_offline_manifest.md`
- `docs/pipeline/offline_detection_replay_spec.md`
- `docs/pipeline/offline_embedding_replay_spec.md`

---

## 4. 目前限制

這一版還沒有真的 decode 影像內容。  
它先做的是：

- 把離線資料入口規格固定
- 驗證 manifest -> Frame 流程
- 驗證 detection replay -> tracker 基本流程

之後再往：

- 真正影像讀取
- detector/tracker/pipeline 整合

延伸。

---

## 5. Tracker Timeline

當提供 detection manifest 時，runner 目前也會輸出：

- `offline_tracker_timeline.csv`
- `offline_decision_timeline.csv`

欄位包含：

- `timestamp_ms`
- `track_id`
- `state`
- bbox
- `hits`
- `miss`
- `score`
- `face_quality`

這份檔案很適合用來做：

- tracker ID 穩定度觀察
- 短 clip 的追蹤 sanity check
- 後續門禁場景的 temporal 行為檢查

---

## 6. Mock Decision Timeline

當提供 detection manifest 時，runner 現在也會額外跑一條輕量的
`mock decision` 驗證線。

這條線的目的不是做真實身份辨識，而是驗證：

- confirmed track 的 decision 累積
- short-gap merge 是否被觸發
- 門禁事件層是否會把短時間斷掉的 track 接回同一次 session

輸出檔案：

- `offline_decision_timeline.csv`

欄位包含：

- `timestamp_ms`
- `track_id`
- `stable_person_id`
- `consistent_hits`
- `average_score`
- `average_margin`
- `unlock_ready`
- `short_gap_merges`

對目前專案來說，這份 timeline 很適合拿來觀察：

- tracker 雖然切段，但 decision session 是否仍保持連續
- 門禁最終結果是否比單純看 `track_id` 更穩

---

## 7. Embedding Replay

若提供第三個 `embedding_manifest.txt`，runner 也會把 embedding event 回放到 pipeline。

這條路主要用來驗證：

- embedding-assisted re-link
- short-gap merge 是否因 embedding 而更容易成立

也就是把驗證流程進一步擴成：

1. frame replay
2. detection replay
3. embedding event replay
4. decision session replay

若指定 `SENTRIFACE_PIPELINE_USE_V2=1`，runner 會改用：

- `EnrollmentStoreV2`
- `FaceSearchV2`
- `FacePipeline` 的 V2 相容層

stdout 也會額外輸出：

- `pipeline_mode=v2`

---

## 10. Logging

`offline_sequence_runner` 現在已接上 logging system。

可用 env：

- `SENTRIFACE_LOG_ENABLE=1`
- `SENTRIFACE_LOG_LEVEL=error|warn|info|debug`
- `SENTRIFACE_LOG_BACKEND=dummy|stdout|file|network_rpc`
- `SENTRIFACE_LOG_MODULES=offline_runner,pipeline,access`
- `SENTRIFACE_LOG_FILE=<path>`

Detector replay 相關 env：

- `SENTRIFACE_DETECTOR_MIN_FACE_WIDTH`
- `SENTRIFACE_DETECTOR_MIN_FACE_HEIGHT`
- `SENTRIFACE_DETECTOR_PREFER_LARGER_FACES=0|1`
- `SENTRIFACE_DETECTOR_MAX_OUTPUT_DETECTIONS`
- `SENTRIFACE_DETECTOR_ENABLE_ROI=0|1`
- `SENTRIFACE_DETECTOR_ROI_X`
- `SENTRIFACE_DETECTOR_ROI_Y`
- `SENTRIFACE_DETECTOR_ROI_W`
- `SENTRIFACE_DETECTOR_ROI_H`

Pipeline re-link 相關 env：

- `SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS`
- `SENTRIFACE_SHORT_GAP_MERGE_MIN_PREVIOUS_HITS`
- `SENTRIFACE_SHORT_GAP_MERGE_CENTER_GATE_RATIO`
- `SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MIN`
- `SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MAX`
- `SENTRIFACE_ENABLE_EMBEDDING_ASSISTED_RELINK=0|1`
- `SENTRIFACE_EMBEDDING_RELINK_SIMILARITY_MIN`

目前 runner 會記錄的高價值事件包括：

- `offline_sequence_runner_started`
- unlock event
- prototype update candidate decision
- `offline_sequence_runner_finished`

---

## 8. Unlock Backend Replay

`offline_sequence_runner` 現在也會整合 unlock backend。

預設行為：

- backend 預設為 `dummy`
- 若 `unlock_ready` 成立，會透過 backend 觸發 unlock event
- `offline_sequence_report.txt` 會記錄：
  - `unlock_ready_tracks`
  - `unlock_actions`

可用 env：

- `SENTRIFACE_UNLOCK_BACKEND=dummy|stdout|gpiod|uart|network_rpc`
- `SENTRIFACE_UNLOCK_PULSE_MS`
- `SENTRIFACE_UNLOCK_COOLDOWN_MS`
- `SENTRIFACE_UNLOCK_UART_DEVICE`
- `SENTRIFACE_UNLOCK_UART_BAUD`
- `SENTRIFACE_UNLOCK_RPC_ENDPOINT`

如果目前沒有真實門鎖裝置，建議先用：

```bash
SENTRIFACE_UNLOCK_BACKEND=stdout ./build/offline_sequence_runner \
  <frame_manifest.txt> <detection_manifest.txt> [embedding_manifest.txt]
```

這樣可以在 replay 過程中直接觀察 unlock event 是否被觸發，而不需要真實 GPIO。

---

## 9. Adaptive Update Replay

若指定：

```bash
SENTRIFACE_PIPELINE_USE_V2=1
SENTRIFACE_PIPELINE_APPLY_ADAPTIVE_UPDATES=1
```

則 `offline_sequence_runner` 也會在 replay 過程中嘗試套用：

- `recent adaptive` prototype update

若再指定：

```bash
SENTRIFACE_PIPELINE_PROMOTE_VERIFIED_UPDATES=1
```

則會改成嘗試寫入：

- `verified history`

此時 runner 會額外輸出：

- `offline_prototype_updates.csv`

而 `offline_sequence_report.txt` 也會多記錄：

- `adaptive_updates_applied`

`offline_prototype_updates.csv` 目前除了 accepted update，也會把未接受的 candidate
一起寫出來，方便做 diagnostics。

重要欄位包含：

- `accepted`
- `cooldown_blocked`
- `quality_ok`
- `decision_score_ok`
- `margin_ok`
- `liveness_ok`
- `rejection_reason`
