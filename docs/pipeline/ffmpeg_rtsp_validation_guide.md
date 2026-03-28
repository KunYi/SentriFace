# FFmpeg RTSP Validation Guide

## 1. 目的

這份文件提供一條可直接執行的 RTSP 驗證路徑，對應：

- `docs/pipeline/rtsp_validation_milestone.md`
- `RV1106G3` 已能輸出 `RTSP video stream`

目標是讓開發者/測試者可以快速完成：

- 抓流
- 抽幀
- 錄短片
- 保存基本 stream metadata

---

## 2. 工具

目前專案已提供：

- `tools/ffmpeg_rtsp_smoke.sh`

它會輸出：

- `stream_info.txt`
- `preview.jpg`
- `preview_1fps/`
- `clip.mp4`
- `artifact_manifest.txt`

---

## 3. 用法

```bash
tools/ffmpeg_rtsp_smoke.sh <rtsp_url> <output_dir> [duration_sec] [fps] [transport]
```

範例：

```bash
tools/ffmpeg_rtsp_smoke.sh "rtsp://192.168.1.10/live" artifacts/rtsp_check
tools/ffmpeg_rtsp_smoke.sh "rtsp://192.168.1.10/live" artifacts/rtsp_check 10 1 tcp
```

---

## 4. 驗證重點

執行後，請優先檢查：

1. `preview.jpg`
   確認畫面是否正常、方向是否正確、臉是否足夠大。

2. `preview_1fps/`
   快速查看亮暗變化、臉部位置與視角穩定性。

3. `clip.mp4`
   確認串流在短時間內沒有明顯斷流或異常跳幀。

4. `stream_info.txt`
   記錄實際解析度、codec、fps 等資訊。

5. `artifact_manifest.txt`
   記錄本次驗證使用的 duration、fps、transport 與輸出檔案集合。

---

## 5. 建議驗證情境

建議至少收集以下 RTSP 樣本：

1. 單人正面靠近
2. 單人停留 2 到 3 秒
3. 單人轉頭
4. 光線較亮
5. 光線較暗
6. 兩人同框但只一人靠近操作區

---

## 6. 和主線開發的關係

這條 RTSP 驗證線：

- 不阻塞主線模組開發
- 但能提早提供真實影像資料

之後這些資料可以直接用於：

- detection 比對
- tracking regression
- alignment 視覺檢查
- embedding sample collection

如果要把 `preview_1fps/` 直接轉成 `offline_sequence_runner` 可讀的 manifest，可再搭配：

- `tools/generate_offline_manifest.sh`
- `docs/pipeline/rtsp_artifact_to_offline_manifest.md`

---

## 7. 注意事項

目前這條路徑只是一個 smoke/validation 工具，不代表最終板端 pipeline 已定案。

特別要注意：

- `CPU-first` 開發仍繼續
- 設計邊界仍以 `RV1106 + RKNN` 為主
- `ffmpeg` 只是幫助驗證 RTSP stream，不是最終部署執行架構
