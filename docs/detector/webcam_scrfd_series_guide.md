# Webcam SCRFD Series Guide

## 1. 目的

這份文件定義如何連續拍攝多張 webcam 圖片，並觀察：

- `SCRFD` 主 detection `score`
- 低光或姿態小變化下的穩定度

這條流程比單張 smoke test 更適合做快速本機 sanity check。

---

## 2. 工具

目前專案提供：

- `tools/webcam_scrfd_series.sh`

它會在每次 capture 下建立：

- `capture_001/`
- `capture_002/`
- ...

並在最外層輸出：

- `series_summary.txt`

---

## 3. 用法

```bash
tools/webcam_scrfd_series.sh <output_dir> [captures] [interval_sec] [video_device] [score_threshold] [max_num]
```

範例：

```bash
tools/webcam_scrfd_series.sh artifacts/webcam_series
tools/webcam_scrfd_series.sh artifacts/webcam_series 5 1 /dev/video0 0.02 5
```

---

## 4. 解讀方式

優先看：

- `best_max_score`
- `worst_max_score`
- `avg_max_score`

如果在偏暗但仍可辨識的人臉條件下，`worst_max_score` 仍大致維持在：

- `0.6` 以上

通常表示本機近距離人臉的 detector score 行為是健康的。

---

## 5. 專案目前觀察

目前本機 webcam smoke 測試已觀察到：

- 在使用者主觀判斷「光線仍然不夠」的情況下
- 主 detection `max_score` 仍可達 `0.6+`

這支持目前的判讀：

- `SCRFD` 在真實 webcam / 近距離人臉情境下，score 表現正常
- 先前低分現象較可能來自 sample 條件，而非整條 pipeline 異常

---

## 6. 注意事項

這條 series 驗證路徑仍然只是：

- 本機 smoke validation

不能直接取代：

- 板端 RTSP 驗證
- 門禁安裝位置測試
- 正式 threshold tuning
