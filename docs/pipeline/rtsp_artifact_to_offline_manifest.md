# RTSP Artifact To Offline Manifest

## 1. 目的

這份文件定義如何把：

- `tools/ffmpeg_rtsp_smoke.sh`

產出的 RTSP 驗證 artifacts，轉成：

- `offline_sequence_runner`

可直接讀取的 manifest。

這條路徑的目標是讓開發者能先用：

- `RV1106G3 + RTSP`
- `ffmpeg`

取得真實 frames，再把資料導入目前的 CPU-first 驗證流程。

---

## 2. 輸入來源

假設先執行：

```bash
tools/ffmpeg_rtsp_smoke.sh "rtsp://192.168.1.10/live" artifacts/rtsp_check 10 1 tcp
```

則主要會得到：

- `artifacts/rtsp_check/preview_1fps/`
- `artifacts/rtsp_check/artifact_manifest.txt`

其中 `preview_1fps/` 內的影像檔，是目前轉成 offline manifest 的主要來源。

---

## 3. 轉換工具

目前專案提供：

```bash
tools/generate_offline_manifest.sh
```

用法：

```bash
tools/generate_offline_manifest.sh <frames_dir> <manifest_path> [start_timestamp_ms] [frame_interval_ms] [pixel_format]
```

範例：

```bash
tools/generate_offline_manifest.sh \
  artifacts/rtsp_check/preview_1fps \
  artifacts/rtsp_check/offline_manifest.txt \
  0 \
  1000 \
  rgb888
```

---

## 4. 目前輸出假設

第一版工具會先產生：

- 相對於 `frames_dir` 的檔名
- 遞增 timestamp
- 固定 `640 x 640`
- 固定 `3 channels`
- 預設 `rgb888`

這是為了先打通：

- RTSP artifact collection
- manifest generation
- offline sequence ingestion

這條資料鏈。

因此要注意：

- 目前的 width/height/channels 是 stage-1 placeholder
- 未來若加入真實 image probing，應改由工具自動讀取影像尺寸

---

## 5. 建議工作流

1. 用 `ffmpeg_rtsp_smoke.sh` 取得 `preview_1fps/`
2. 用 `generate_offline_manifest.sh` 產生 `offline_manifest.txt`
3. 用 `offline_sequence_runner` 驗證 manifest 是否可被主程式入口讀取

範例：

```bash
tools/ffmpeg_rtsp_smoke.sh "rtsp://192.168.1.10/live" artifacts/rtsp_check 10 1 tcp
tools/generate_offline_manifest.sh artifacts/rtsp_check/preview_1fps artifacts/rtsp_check/offline_manifest.txt
./build/offline_sequence_runner artifacts/rtsp_check/offline_manifest.txt
```

---

## 6. 和主線開發的關係

這條路徑不代表最終板端部署流程。

它的價值是：

- 讓 RTSP 實機資料可提早導入主驗證鏈
- 降低 `RV1106 camera/RTSP` 問題與核心演算法問題互相耦合
- 為未來真正的 image decode、detector backend、pipeline integration 鋪路
