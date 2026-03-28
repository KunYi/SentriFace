# Baseline Embedding Import Workflow

這份文件定義如何將真實 baseline embedding CSV 導入 `EnrollmentStoreV2` 的 baseline zone。

## 工具

目前專案提供：

- `enrollment_baseline_import_runner`

## 用法

```bash
./build/enrollment_baseline_import_runner \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_embedding_output.csv \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623/baseline_import_summary.txt \
  512
```

## 目前定位

這條 workflow 的目的是：

- 讀取真實 baseline embedding CSV
- 轉成 `BaselinePrototypePackage`
- 寫入暫時的 `EnrollmentStoreV2`
- 輸出可讀 summary，方便 bring-up 驗證

目前它仍屬 bring-up / validation path，但已經不再只停留在 mock embedding。
