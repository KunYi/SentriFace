# Face Embedding ONNX Workflow

## 1. 目的

這份文件定義如何使用 `buffalo_sc/w600k_mbf.onnx` 產生真實 embedding CSV。

---

## 2. 工具

目前專案提供：

- `tools/run_face_embedding_onnx.py`

---

## 3. 用法

```bash
.venv/bin/python tools/run_face_embedding_onnx.py \
  third_party/models/buffalo_sc/w600k_mbf.onnx \
  detection.csv \
  embedding.csv
```

它會：

1. 讀取 detection CSV
2. 使用 5 landmarks 做 aligned crop
3. 跑 `w600k_mbf.onnx`
4. 輸出 embedding CSV

---

## 4. 輸入要求

detection CSV 需包含：

- `image_path`
- `frame_index`
- bbox
- `score`
- `l0x..l4y`

這與目前 `tools/run_scrfd_onnx.py` 的輸出相容。

---

## 5. 目前定位

這條 workflow 的目的是：

- 先建立真實 embedding event 的輸出流程

後續可再接：

- `tools/convert_embedding_csv_to_manifest.py`
- `offline_sequence_runner`
