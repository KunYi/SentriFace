# Webcam SCRFD Validation Guide

## 1. 目的

這份文件定義如何直接使用本機 webcam 做：

- 單張拍攝
- `SCRFD` ONNX detection
- score summary 檢查

它的定位是：

- 快速 smoke test
- 幫助確認真實近距離人臉時 `score` 是否落在合理範圍
- 不取代正式 RTSP / 實機場景驗證

---

## 2. 工具

目前專案提供：

- `tools/webcam_scrfd_smoke.sh`
- `tools/webcam_scrfd_series.sh`

它會輸出：

- `webcam_capture.jpg`
- `scrfd_webcam.csv`
- `scrfd_webcam_summary.txt`

---

## 3. 用法

```bash
tools/webcam_scrfd_smoke.sh <output_dir> [video_device] [score_threshold] [max_num]
```

範例：

```bash
tools/webcam_scrfd_smoke.sh artifacts/webcam_check
tools/webcam_scrfd_smoke.sh artifacts/webcam_check /dev/video0 0.02 5
```

---

## 4. 前提

這條驗證路徑預設需要：

- `ffmpeg`
- 專案 `.venv`
- `third_party/models/buffalo_sc/det_500m.onnx`

也就是說，它建立在目前已完成的：

- webcam 裝置可見
- Python `onnxruntime` 可用
- `SCRFD` 官方 model 已下載

---

## 5. 解讀方式

執行後優先看：

1. `webcam_capture.jpg`
   確認臉是否清楚、是否正對、光線是否足夠。

2. `scrfd_webcam_summary.txt`
   優先看：
   - `detections`
   - `max_score`
   - 分數分布

3. `scrfd_webcam.csv`
   若要進一步檢查 bbox 與 landmarks，再看這個檔。

---

## 6. 判讀原則

若主 detection 的 `max_score` 能穩定落在：

- 約 `0.6 ~ 0.9`

通常表示：

- 真實近距離人臉條件下，模型輸出是正常的

目前本專案已在本機 webcam smoke 測試中觀察到：

- 即使使用者主觀認為光線仍偏暗
- 主 detection `max_score` 仍可達 `0.6+`

這代表：

- detector 的主分數在真實 webcam 人臉條件下仍是健康的
- 先前極低分的主要懷疑方向，仍應放在 sample 條件而不是 pipeline 本身

若只有：

- `0.05 ~ 0.2`

則更可能是：

- 臉太暗
- 臉太小
- 沒有正對鏡頭
- 圖像品質不理想

而不一定代表整條 pipeline 有錯。

---

## 7. 注意事項

這條 webcam 驗證線的主要用途是：

- 本機快速 smoke test

它不能直接取代：

- `RV1106G3 + RTSP`
- 真實門禁安裝位置
- 正式 threshold tuning

正式 threshold 與 production decision，仍應回到：

- `docs/pipeline/rtsp_validation_milestone.md`

定義的實機驗證流程。

若要觀察多次拍攝下的穩定度，可再搭配：

- `docs/detector/webcam_scrfd_series_guide.md`
