# Offline Detection Replay Spec

## 1. 目的

這份文件定義一條離線 detection replay 路徑，讓外部先跑出的 detection 結果，可以直接導入：

- `offline_sequence_runner`
- `tracker`
- `pipeline`

做回放驗證。

這特別適合目前專案的過渡階段：

- 主線仍以 `SCRFD-500M` 為目標
- 但真實 backend 可能先在桌面或其他環境產生結果

---

## 2. 使用情境

典型流程：

1. 先用 `ffmpeg` 從 RTSP 抽幀
2. 在桌面環境對這批 frames 跑 `SCRFD`
3. 把 detection 結果匯出成 text manifest
4. 用 `offline_sequence_runner` 回放 frame + detections
5. 驗證 tracker 穩定度與 peak track 數

若外部 detector 是桌面版 `SCRFD`，可參考：

- `docs/detector/scrfd_offline_export_workflow.md`

若後續還要回放 embedding event，可再搭配：

- `docs/pipeline/offline_embedding_replay_spec.md`

---

## 3. Detection Manifest 格式

每一行代表一個 detection：

```text
<timestamp_ms> <x> <y> <w> <h> <score> <l0x> <l0y> <l1x> <l1y> <l2x> <l2y> <l3x> <l3y> <l4x> <l4y>
```

範例：

```text
1000 100 120 96 104 0.95 128 156 168 155 148 176 132 200 166 199
1033 104 121 96 104 0.96 132 157 172 156 152 177 136 201 170 200
```

說明：

- `timestamp_ms` 必須和 frame manifest 的 timestamp 對齊
- `bbox` 使用 `x y w h`
- landmarks 使用 `5-point`，順序與專案既有 `SCRFD` 假設一致

---

## 4. 目前行為

`ManifestFaceDetector` 目前採：

- 依 timestamp 精確配對 frame
- 低於 `score_threshold` 的 detection 會被過濾
- 若某個 frame 沒有對應 timestamp，則回傳空 batch

這樣的行為可以讓 replay 結果可預期，也容易做 regression test。

---

## 5. 目前限制

第一版仍是簡化版：

- 不支援 JSON 或 binary format
- 不處理 detector class id
- 不處理 face quality 或姿態欄位
- timestamp 必須和 frame manifest 明確對齊

未來若需要更接近真實部署，可再擴充：

- frame path 對齊模式
- richer metadata
- detector backend export adapters
