# Real Embedding Adaptive Replay Workflow

## 1. 目的

這份文件定義如何用真實 embedding replay 驗證：

- `FacePipeline` 的 V2 路徑
- `EnrollmentStoreV2` 的 adaptive update policy
- replay 過程中是否真的產生 prototype update event

這條流程回答的是：

- 真實 embedding 事件下，是否有形成可接受的 adaptive update candidate
- policy 是否真的接受這些事件
- 最終是否有 prototype update artifact 產生

---

## 2. 一鍵工具

目前專案提供：

- `tools/run_real_embedding_adaptive_replay.sh`

---

## 3. 用法

```bash
tools/run_real_embedding_adaptive_replay.sh <artifact_dir> [recognizer_model] [score_threshold]
```

範例：

```bash
tools/run_real_embedding_adaptive_replay.sh artifacts/webcam_bright
```

---

## 4. 流程內容

工具會自動完成：

1. `V2 baseline replay`
2. 使用真實 recognizer 產生 `embedding_output.csv`
3. 轉成 `embedding_manifest_real.txt`
4. 執行 `V2 + adaptive updates` replay
5. 輸出 comparison summary

---

## 5. 主要產物

執行後會更新或產生：

- `offline_sequence_report_v2_baseline.txt`
- `offline_sequence_report_v2_adaptive.txt`
- `offline_prototype_updates_v2_adaptive.csv`
- `real_embedding_adaptive_replay_comparison.txt`

---

## 6. 判讀重點

優先看：

- `adaptive_updates_applied`
- `adaptive_updates_applied_delta`
- `offline_prototype_updates_v2_adaptive.csv`

若：

- `adaptive_updates_applied > 0`

代表 replay 過程中確實出現了可接受的 adaptive update event。

若：

- `adaptive_updates_applied == 0`

則應優先檢查：

- embedding event 的觸發時機
- `face_quality`
- `decision_score`
- `top1_top2_margin`
- policy 門檻是否過嚴

這時最直接的方式是打開：

- `offline_prototype_updates_v2_adaptive.csv`

看每筆 candidate 的：

- `accepted`
- `cooldown_blocked`
- `rejection_reason`
