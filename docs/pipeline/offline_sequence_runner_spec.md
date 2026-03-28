# Offline Sequence Runner Spec

## 1. 目的

這份文件定義下一個驗證目標：

- `offline sequence runner`

它的角色是把：

- RTSP 抽幀 artifacts
- 離線圖片序列
- 短片轉出的影格

接進目前已建立的：

- `camera::IFrameSource`
- `detector`
- `tracker`
- `pipeline glue`

---

## 2. 為什麼需要它

目前專案已有：

- `sample_pipeline_runner`
- `tools/ffmpeg_rtsp_smoke.sh`

但兩者之間還缺一層：

- 能直接吃真實 frame sequence 的 runner

這會是從 CPU-first 架構走向 RTSP/真實資料驗證的重要中繼點。

---

## 3. 目前已實作能力

目前 `offline_sequence_runner` 已可支援：

1. `frame manifest`
2. 可選的 `detection manifest`
3. tracker timeline 輸出
4. pipeline report 輸出

目前主要用途是：

- 重播 RTSP / webcam 抽幀資料
- 用固定 detection manifest 驗證 tracker 行為
- 對單人短 clip 做 regression

---

## 4. 目前可用輸出

若有提供 detection replay，runner 目前會產生：

- `offline_sequence_report.txt`
- `offline_tracker_timeline.csv`
- `offline_decision_timeline.csv`

這兩個檔案可再交給：

- `tools/summarize_tracker_timeline.py`
- `tools/evaluate_tracker_summary.py`

做進一步判讀。

其中 `offline_decision_timeline.csv` 主要用於：

- 觀察 confirmed track 的 decision 累積
- 驗證 short-gap merge 是否觸發
- 檢查門禁事件層是否比單純 `track_id` 更穩定

此外 runner 也可選擇接收：

- `embedding event manifest`

讓已經算過的 embedding event 回放到 pipeline，進一步驗證：

- embedding-assisted re-link
- embedding 是否能補足短時間斷軌的事件連續性

---

## 5. 目前環境變數覆蓋

目前 `offline_sequence_runner` 已支援以環境變數覆蓋部分 detector / tracker 參數。

Detector：

- `SENTRIFACE_DETECTOR_MIN_FACE_WIDTH`
- `SENTRIFACE_DETECTOR_MIN_FACE_HEIGHT`
- `SENTRIFACE_DETECTOR_PREFER_LARGER_FACES`
- `SENTRIFACE_DETECTOR_MAX_OUTPUT_DETECTIONS`
- `SENTRIFACE_DETECTOR_ENABLE_ROI`
- `SENTRIFACE_DETECTOR_ROI_X`
- `SENTRIFACE_DETECTOR_ROI_Y`
- `SENTRIFACE_DETECTOR_ROI_W`
- `SENTRIFACE_DETECTOR_ROI_H`

Tracker：

- `SENTRIFACE_TRACKER_MIN_CONFIRM_HITS`
- `SENTRIFACE_TRACKER_MAX_MISS`
- `SENTRIFACE_TRACKER_MAX_TRACKS`
- `SENTRIFACE_TRACKER_IOU_MIN`
- `SENTRIFACE_TRACKER_CENTER_GATE_RATIO`
- `SENTRIFACE_TRACKER_AREA_RATIO_MIN`
- `SENTRIFACE_TRACKER_AREA_RATIO_MAX`

Pipeline re-link：

- `SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS`
- `SENTRIFACE_SHORT_GAP_MERGE_MIN_PREVIOUS_HITS`
- `SENTRIFACE_SHORT_GAP_MERGE_CENTER_GATE_RATIO`
- `SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MIN`
- `SENTRIFACE_SHORT_GAP_MERGE_AREA_RATIO_MAX`
- `SENTRIFACE_ENABLE_EMBEDDING_ASSISTED_RELINK`
- `SENTRIFACE_EMBEDDING_RELINK_SIMILARITY_MIN`

這使它很適合拿來做 stage-1 的離線 sweep 與 regression。

---

## 6. 門禁場景建議

對目前這個單入口門禁專案，建議 baseline 如下：

- detector 先淘汰過小臉框
- 預設 `min_face_width = 120`
- 預設 `min_face_height = 120`
- 預設 `prefer_larger_faces = true`
- 若驗證場景非常明確是單人門禁，可考慮 `max_output_detections = 1`
- 若鏡頭安裝位置已固定，建議加上 detector ROI，把畫面中不可能成為有效門禁事件的區域先排除

這樣可先把遠距離多人、小頭像、背景海報中的小臉從主流程排除，再觀察真正的
單人追蹤穩定度。
