# InsightFace Package Inspection

## 1. 目的

這份文件定義如何在下載官方 InsightFace package 後，快速確認其中哪些模型檔可以對應到本專案的 detector 整合。

---

## 2. 工具

目前專案提供：

```bash
tools/inspect_insightface_package.sh
```

用法：

```bash
tools/inspect_insightface_package.sh <package.zip> [extract_dir]
```

範例：

```bash
tools/inspect_insightface_package.sh third_party/models/buffalo_sc.zip
tools/inspect_insightface_package.sh third_party/models/buffalo_sc.zip third_party/models/buffalo_sc
```

---

## 3. 它會做什麼

工具會輸出三類資訊：

1. zip 內容總覽
2. package 內所有 `.onnx` 檔
3. 以檔名 heuristics 找出的 detector 候選

目前 detector 候選主要看：

- 檔名含 `det`
- 檔名含 `scrfd`

---

## 4. 為什麼需要這一步

InsightFace 官方 release 往往提供的是 model package，不一定直接以：

- `scrfd_500m_bnkps.onnx`

這種單檔方式發布。

因此在整合 `ScrfdFaceDetector` 前，需要先確認：

- package 內到底有哪些模型
- 哪一個是 detector
- 哪一些其實是 embedding / recognition 模型

---

## 5. 後續動作

確認 detector 檔後，下一步應做：

1. 記錄 package 內容與 detector 檔名
2. 將 detector model path 對應到 `ScrfdDetectorConfig`
3. 開始落實 CPU backend 或 RKNN backend 的真正 runtime integration

---

## 6. 目前已確認結果

目前已實際檢查：

- `third_party/models/buffalo_sc.zip`

內容為：

- `det_500m.onnx`
- `w600k_mbf.onnx`

其中 detector 候選已明確是：

- `det_500m.onnx`

若已解壓至：

- `third_party/models/buffalo_sc/`

則可再用：

```bash
tools/resolve_scrfd_model_path.sh third_party/models/buffalo_sc
```

快速取得 detector model path。
