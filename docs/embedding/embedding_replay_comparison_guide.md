# Embedding Replay Comparison Guide

## 1. 目的

這份文件定義如何對同一批 offline replay 素材做：

- baseline decision replay
- mock embedding replay
- before / after comparison

這很適合目前專案階段，因為它能快速回答：

- embedding-assisted re-link 是否真的有幫助

---

## 2. 工具

目前專案提供：

- `tools/run_embedding_replay_comparison.sh`

---

## 3. 用法

```bash
tools/run_embedding_replay_comparison.sh <artifact_dir>
```

範例：

```bash
tools/run_embedding_replay_comparison.sh artifacts/webcam_bright
```

也可指定：

- 要注入 mock embedding 的 `track_ids`
- 使用的 embedding 向量

```bash
tools/run_embedding_replay_comparison.sh \
  artifacts/webcam_bright \
  0,1,2 \
  1.0,0.0,0.0,0.0
```

---

## 4. 產物

執行後會更新或產生：

- `offline_tracker_timeline.csv`
- `mock_embedding_manifest.txt`
- `offline_decision_summary.txt`
- `offline_decision_evaluation.txt`
- `offline_decision_summary_mock_embedding.txt`
- `offline_decision_evaluation_mock_embedding.txt`
- `embedding_replay_comparison.txt`

---

## 5. 判讀重點

優先看：

- `unlock_ready_row_delta`
- `short_gap_merge_delta`

若：

- `unlock_ready_row_delta > 0`
- `short_gap_merge_delta > 0`

則代表 embedding-assisted re-link 在這批素材上帶來正向效果。
