# Real Embedding Replay Workflow

## 1. 目的

這份文件定義如何用真實 face recognition ONNX 模型產生 embedding event，並把它回放到 offline replay pipeline。

這條流程主要用來回答：

- 真實 embedding 是否真的能幫忙 short-gap re-link
- 真實 embedding 與 mock embedding 的效果差在哪裡

---

## 2. 先決條件

需要具備：

- `build/offline_sequence_runner`
- `.venv/bin/python`
- `third_party/models/buffalo_sc/w600k_mbf.onnx`
- 一組已經準備好的 offline artifacts

artifact 目錄至少要有：

- `offline_manifest.txt`
- `detection_manifest.txt`
- `scrfd_frames.csv`

---

## 3. 一鍵工具

目前專案提供：

- `tools/run_real_embedding_replay.sh`

---

## 4. 用法

```bash
tools/run_real_embedding_replay.sh <artifact_dir> [recognizer_model] [score_threshold]
```

範例：

```bash
tools/run_real_embedding_replay.sh artifacts/webcam_bright
```

也可指定模型與 score threshold：

```bash
tools/run_real_embedding_replay.sh \
  artifacts/webcam_bright \
  third_party/models/buffalo_sc/w600k_mbf.onnx \
  0.60
```

---

## 5. 流程內容

工具會自動完成：

1. baseline offline replay
2. 產生 `offline_tracker_timeline.csv`
3. 用 `w600k_mbf.onnx` 對 `scrfd_frames.csv` 產生 `embedding_output.csv`
4. 把 embedding CSV 轉成 `embedding_manifest_real.txt`
5. 執行 real-embedding-assisted offline replay
6. 產生 decision summary / evaluation / comparison

---

## 6. 產物

執行後會更新或產生：

- `offline_tracker_timeline.csv`
- `embedding_output.csv`
- `embedding_manifest_real.txt`
- `offline_decision_summary.txt`
- `offline_decision_evaluation.txt`
- `offline_decision_summary_real_embedding.txt`
- `offline_decision_evaluation_real_embedding.txt`
- `real_embedding_replay_comparison.txt`

---

## 7. 判讀重點

優先看：

- `short_gap_merge_delta`
- `unlock_ready_row_delta`

若：

- `short_gap_merge_delta > 0`

代表真實 embedding 確實有幫助 session re-link。

但若同時：

- `unlock_ready_row_delta <= 0`

則代表 merge 雖然增加了，但 decision 累積效果未必同步改善，後續應再檢查：

- embedding event 觸發時機
- score threshold
- short-gap merge 規則
- decision history 合併方式
