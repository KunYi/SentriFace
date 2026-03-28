# SCRFD Offline Export Workflow

## 1. 目的

這份文件定義如何把桌面環境先跑出的 `SCRFD` 結果，轉成目前專案可直接回放的 detection manifest。

它的用途是：

- 不阻塞正式 `ScrfdFaceDetector` backend 開發
- 先讓 `SCRFD -> tracker` 這條驗證線跑起來
- 把桌面推論結果導入 `offline_sequence_runner`

---

## 2. 建議工作流

建議順序如下：

1. 用 `tools/ffmpeg_rtsp_smoke.sh` 從 `RV1106G3` RTSP 抽幀
2. 用 `tools/generate_offline_manifest.sh` 產生 frame manifest
3. 在桌面環境對抽出的 frames 跑 `SCRFD`
4. 將 `SCRFD` 輸出整理成 CSV
5. 用 `tools/convert_scrfd_csv_to_manifest.py` 轉成 detection manifest
6. 用 `./build/offline_sequence_runner` 回放 frame + detections

---

## 3. CSV 輸入格式

轉換工具目前採用 header-based CSV。

必要欄位：

```text
x,y,w,h,score,l0x,l0y,l1x,l1y,l2x,l2y,l3x,l3y,l4x,l4y
```

時間欄位二選一：

```text
timestamp_ms
```

或：

```text
frame_index
```

範例：

```csv
frame_index,x,y,w,h,score,l0x,l0y,l1x,l1y,l2x,l2y,l3x,l3y,l4x,l4y
0,100,120,96,104,0.95,128,156,168,155,148,176,132,200,166,199
1,104,121,96,104,0.96,132,157,172,156,152,177,136,201,170,200
```

---

## 4. 轉換工具

目前專案提供：

```bash
tools/convert_scrfd_csv_to_manifest.py
```

基本用法：

```bash
tools/convert_scrfd_csv_to_manifest.py <input_csv> <output_manifest>
```

若 CSV 使用 `frame_index`，建議再搭配 frame manifest：

```bash
tools/convert_scrfd_csv_to_manifest.py \
  artifacts/rtsp_check/scrfd_output.csv \
  artifacts/rtsp_check/detection_manifest.txt \
  --frame-manifest artifacts/rtsp_check/offline_manifest.txt
```

---

## 5. 輸出格式

輸出會符合：

- `docs/pipeline/offline_detection_replay_spec.md`

即每行一個 detection：

```text
<timestamp_ms> <x> <y> <w> <h> <score> <l0x> <l0y> <l1x> <l1y> <l2x> <l2y> <l3x> <l3y> <l4x> <l4y>
```

---

## 6. 例子

完整示例：

```bash
tools/ffmpeg_rtsp_smoke.sh "rtsp://192.168.1.10/live" artifacts/rtsp_check 10 1 tcp
tools/generate_offline_manifest.sh artifacts/rtsp_check/preview_1fps artifacts/rtsp_check/offline_manifest.txt
tools/convert_scrfd_csv_to_manifest.py \
  artifacts/rtsp_check/scrfd_output.csv \
  artifacts/rtsp_check/detection_manifest.txt \
  --frame-manifest artifacts/rtsp_check/offline_manifest.txt
./build/offline_sequence_runner \
  artifacts/rtsp_check/offline_manifest.txt \
  artifacts/rtsp_check/detection_manifest.txt
```

---

## 7. 設計意圖

這份 workflow 的重點不是定死某個桌面 `SCRFD` 實作，而是先固定：

- 可交換的 CSV 中繼格式
- 專案內部可回放的 detection manifest

這樣未來不管桌面推論是：

- Python
- ONNX Runtime
- NCNN
- RKNN toolkit 模擬

只要能輸出同樣的 CSV 欄位，就能接到目前的 replay 驗證線。
