# Model Asset Acquisition

## 1. 目的

這份文件定義本專案如何取得官方模型資產，特別是後續整合 `SCRFD` 時需要的 InsightFace release assets。

---

## 2. 官方來源

目前已確認可用的一手來源是：

- InsightFace GitHub Releases `v0.7`

可見的官方 assets 包含：

- `antelopev2.zip`
- `buffalo_l.zip`
- `buffalo_m.zip`
- `buffalo_s.zip`
- `buffalo_sc.zip`
- `inswapper_128.onnx`
- `scrfd_person_2.5g.onnx`

官方 release：

- `https://github.com/deepinsight/insightface/releases/tag/v0.7`

若需要檢查 ONNX model 的真實 input/output，可再搭配：

- `tools/inspect_scrfd_onnx.py`
- `docs/detector/scrfd_model_io_verified.md`

若要直接用已下載的 detector model 跑圖片，可再搭配：

- `tools/run_scrfd_onnx.py`
- `docs/detector/scrfd_onnx_runner_workflow.md`

---

## 3. 專案內下載工具

目前專案提供：

```bash
tools/download_insightface_asset.sh
```

用法：

```bash
tools/download_insightface_asset.sh <asset_name> [output_dir]
```

範例：

```bash
tools/download_insightface_asset.sh buffalo_sc.zip
tools/download_insightface_asset.sh buffalo_s.zip third_party/models
```

---

## 4. 目前策略

對本專案來說，這一步的重點不是先把所有模型都硬接進 runtime，而是：

1. 先建立正式的下載來源
2. 先把模型資產納入可重現 workflow
3. 再確認 package 內容與 detector 對應關係
4. 最後把對應的 `SCRFD` detector model 接進 `ScrfdFaceDetector`

---

## 4.1 已驗證的官方 package

目前已實際檢查：

- `buffalo_sc.zip`

package 內容包含：

- `det_500m.onnx`
- `w600k_mbf.onnx`

其中：

- `det_500m.onnx` 可視為目前最直接對應 `ScrfdFaceDetector` 整合的 detector 候選
- `w600k_mbf.onnx` 則較像 recognition / embedding 模型，不是 detector

因此以目前專案狀態來看，最合理的第一個官方整合落點是：

- 下載 `buffalo_sc.zip`
- 解開後使用 `third_party/models/buffalo_sc/det_500m.onnx`

---

## 5. 注意事項

- `third_party/models/` 已設為 git ignore，不應直接提交大型模型檔
- 若之後確認某個 package 內含我們要的 detector，應再補一份對應的內容清單與使用說明
- 若之後找到官方單獨發布的 `SCRFD-500M` detector model，也應優先在此文件補充
