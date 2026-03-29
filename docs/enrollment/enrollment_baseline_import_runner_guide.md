# Enrollment Baseline Import Runner Guide

這份文件定義 host enrollment artifact 如何一路轉成可驗證的 baseline import summary。

## 一鍵 workflow

```bash
tools/run_enrollment_baseline_import.sh \
  artifacts/host_qt_enroll/E1007_20260328_174623/enrollment_summary.txt \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623
```

這會依序完成：

1. `enrollment_artifact_runner`
2. `baseline_embedding_input_manifest`
3. `w600k_mbf.onnx` baseline embedding CSV
4. `enrollment_baseline_import_runner`
5. 若 `.sfbp` / import summary 已存在則直接重用

## 主要產物

- `baseline_artifact_summary.txt`
- `baseline_artifact_summary_embedding_input_manifest.txt`
- `baseline_embedding_output.sfbp`
- `baseline_embedding_output.sfsi`
- `baseline_embedding_output.csv`
- `baseline_import_summary.txt`

主線 handoff 以 `.sfbp / .sfsi` 為主，`CSV` 只保留作 embedding 互通與 debug。

## Bring-up 目的

這條 workflow 的目的不是直接宣告量產完成，而是讓 host enrollment artifact 可以：

- 先做真實 embedding
- 先把 embedding 落成可 direct-load 的 `.sfbp` package
- 再把 search index 落成可 direct-load 的 `.sfsi` package
- CSV 只保留作互通 / debug 輸入
- 再做 baseline import
- 最後產生可人工檢查的 summary

這樣真實 artifact 驗證時，只要換掉 `enrollment_summary.txt`，就能快速重跑整段資料流。

格式細節請參考：

- `docs/enrollment/baseline_prototype_package_binary_spec.md`
- `docs/search/search_index_package_binary_spec.md`
