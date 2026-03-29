# Baseline Embedding Import Workflow

這份文件定義如何將真實 baseline embedding 轉成正式 binary packages。
正式主線是 `.sfbp -> .sfsi`，CSV 只作互通輸入，`EnrollmentStoreV2` 只保留給 bring-up 驗證。

## 工具

目前專案提供：

- `tools/run_enrollment_baseline_embedding.sh`
- `tools/run_enrollment_baseline_embedding_cpp.sh`
- `enrollment_baseline_import_runner`
- `enrollment_artifact_runner`

## 用法

```bash
./build/enrollment_baseline_import_runner \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_embedding_output.csv \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_import_summary.txt \
  512
```

runner 也接受已落盤的 binary package：

```bash
./build/enrollment_baseline_import_runner \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_embedding_output.sfbp \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_import_summary.txt \
  512
```

## 目前定位

這條 workflow 的目的是：

- 先由 host artifact 產生 `baseline_embedding_input_manifest`
- 跑真實 embedding 產出 `baseline_embedding_output.csv`
- 立即導入成 `.sfbp`
- 再直接建出 `.sfsi` search-ready index package
- 避免必須先經過 `EnrollmentStoreV2 -> ExportWeightedPrototypes()`
- 若要做 bring-up 驗證，才寫入暫時的 `EnrollmentStoreV2`
- 輸出可讀 summary，方便 bring-up 驗證

若 build 已啟用 C++ `onnxruntime`，也可直接用：

- `tools/run_enrollment_baseline_embedding_cpp.sh`

讓 `enrollment_artifact_runner` 直接經由 `onnxruntime` backend 產出 `.sfbp / .sfsi`，
不必先經過 Python CSV 中繼。

`tools/run_enrollment_baseline_embedding.sh` 目前會直接落成：

- `baseline_artifact_summary.txt`
- `baseline_artifact_summary_embedding_input_manifest.txt`
- `baseline_embedding_output.csv`
- `baseline_import_summary.txt`
- `baseline_embedding_output.sfbp`
- `baseline_embedding_output.sfsi`

目前它仍屬 bring-up / validation path，但正式載入邊界已優先落在 `.sfbp` 與 `.sfsi`，CSV 保留為互通與除錯格式。

另外目前 `enrollment_artifact_runner` 也已支援：

- `mock`
- `onnxruntime`

兩種 baseline generation backend。
其中 `onnxruntime` backend 會直接由 artifact 的 preferred image
跑 C++ embedding runtime，輸出正式 `.sfbp / .sfsi`；
而 Python CSV workflow 仍保留作 oracle / regression / interoperability 路徑。

對應 binary format 規格：

- `docs/enrollment/baseline_prototype_package_binary_spec.md`
- `docs/search/search_index_package_binary_spec.md`
