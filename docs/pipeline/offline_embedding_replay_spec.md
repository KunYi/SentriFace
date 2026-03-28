# Offline Embedding Replay Spec

## 1. 目的

這份文件定義離線 `embedding event replay` 的格式，讓已經算過的 embedding
可以回放到：

- `offline_sequence_runner`
- `FacePipeline`
- short-gap merge / embedding-assisted re-link

---

## 2. 使用情境

這條路特別適合目前專案的驗證方式：

1. 先跑 detection replay
2. 先確認 tracker timeline
3. 再把少量、節流過的 embedding event 回放進來
4. 驗證 decision session 與 re-link 是否改善

也就是：

- 不要求每幀都有 embedding
- 只回放真正算過的 embedding 事件

---

## 3. Manifest 格式

每一行代表一筆 embedding event：

```text
<timestamp_ms> <track_id> <e0> <e1> ... <eN-1>
```

範例：

```text
1033 0 1.0 0.0 0.0 0.0
1066 0 1.0 0.0 0.0 0.0
```

說明：

- `timestamp_ms` 必須和 replay frame 對齊
- `track_id` 必須和 replay 過程中的 tracker id 對齊
- 後面為 embedding 向量
- 維度需與 `pipeline_config.search.embedding_dim` 一致

---

## 4. 目前限制

這是一條驗證用 replay 規格，因此目前有幾個限制：

- 需要先知道 replay 期間的 `track_id`
- 較適合兩段式驗證
- 不適合直接拿來當 production IPC 格式

但對目前專案來說，這已足以驗證：

- embedding-assisted re-link
- short-gap merge
- decision session 是否因 embedding 而更穩

---

## 5. 與 Adaptive Prototype Update 的關係

當使用：

- `SENTRIFACE_PIPELINE_USE_V2=1`
- `SENTRIFACE_PIPELINE_APPLY_ADAPTIVE_UPDATES=1`

時，`offline_sequence_runner` 會把 replay 過程中的：

- `unlock_ready`
- `embedding`
- `quality_score`
- `decision_score`
- `top1_top2_margin`

整理成 adaptive update candidate，再交給 `EnrollmentStoreV2` 的 policy API 判斷是否可寫入。

這條 replay 驗證的目的不是直接做 production 寫庫，而是先確認：

- 事件條件是否足夠形成 update candidate
- policy 是否會接受
- 實際有沒有產生 update event artifact
