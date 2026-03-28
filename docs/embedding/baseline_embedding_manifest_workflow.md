# Baseline Embedding Manifest Workflow

這份文件定義如何從 host enrollment artifact 產生真實 baseline embedding CSV。

## 工具

目前專案提供：

- `enrollment_artifact_runner`
- `tools/run_baseline_embedding_onnx.py`
- `tools/run_enrollment_baseline_embedding.sh`

## 流程

1. 使用 `enrollment_artifact_runner` 從 `enrollment_summary.txt` 產生：
   - `baseline_package_summary_*.txt`
   - `*_embedding_input_manifest.txt`
2. 使用 `tools/run_baseline_embedding_onnx.py` 讀取 embedding input manifest
3. 以 `w600k_mbf.onnx` 對每個 baseline image 產生真實 embedding CSV

## 快速用法

```bash
tools/run_enrollment_baseline_embedding.sh \
  artifacts/host_qt_enroll/E1007_20260328_174623/enrollment_summary.txt \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623
```

## 目前定位

這條 workflow 已經會產生真實 embedding CSV，但目前仍屬 bring-up 路徑：

- image 來源是 host enrollment artifact 中的 `face_crop`
- 尚未接上最終量產版 alignment runtime

所以它的定位是：

- 先把 host enrollment artifact 接到真實 embedding 模型
- 為後續 baseline prototype 入庫流程做真實資料驗證
