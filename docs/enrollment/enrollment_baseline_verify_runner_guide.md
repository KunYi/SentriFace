# Enrollment Baseline Verify Runner Guide

這份文件定義 baseline import 之後如何直接做 self-check。

## 一鍵 workflow

```bash
tools/run_enrollment_baseline_verify.sh \
  artifacts/host_qt_enroll/E1007_20260328_174623/enrollment_summary.txt \
  1007 \
  artifacts/host_qt_enroll/E1007_20260328_174623
```

這會依序完成：

1. baseline embedding generate
2. workflow 直接產出 `.sfbp` / `.sfsi`
3. 若缺少 import summary，補跑 baseline import
4. verify runner 優先直接載入 `.sfsi`
5. 若 `.sfsi` 不存在，再直接由 `.sfbp` 重建並回寫 sibling `.sfsi`
6. 用 `.sfbp` package 的 query prototypes 做 `FaceSearchV2` self-check

## 主要產物

- `baseline_embedding_output.sfbp`
- `baseline_embedding_output.sfsi`
- `baseline_embedding_output.csv`
- `baseline_import_summary.txt`
- `baseline_verify_summary.txt`

CSV 只作輸入互通與 debug。

## Summary 重點

`baseline_verify_summary.txt` 目前會輸出：

- `query_prototypes`
- `search_index_input`
- 每個 sample 的 `top1_person_id`
- `top1_score`
- `top1_margin`
- `matched_top1`
- `all_top1_match`

這條 workflow 的目的是先確認：

- baseline import 是否成功
- 本地 search index 是否能正確找回 enrollment 本人

格式細節請參考：

- `docs/enrollment/baseline_prototype_package_binary_spec.md`
- `docs/search/search_index_package_binary_spec.md`
