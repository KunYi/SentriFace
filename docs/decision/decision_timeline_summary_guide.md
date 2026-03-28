# Decision Timeline Summary Guide

## 1. 目的

這份文件定義如何把 `offline_decision_timeline.csv` 轉成較容易閱讀的摘要。

它適合用於：

- 驗證 decision session 是否連續
- 觀察 unlock-ready 的形成過程
- 檢查 short-gap merge 是否有被觸發

---

## 2. 工具

目前專案提供：

- `tools/summarize_decision_timeline.py`

---

## 3. 用法

```bash
python3 tools/summarize_decision_timeline.py \
  offline_decision_timeline.csv \
  decision_summary.txt
```

---

## 4. 主要欄位

摘要會包含：

- `rows`
- `unique_track_ids`
- `track_ids`
- `unlock_ready_rows`
- `max_short_gap_merges`
- `stable_person_ids`

以及每個 `track_id` 的：

- `samples`
- `max_consistent_hits`
- `best_average_score`
- `unlock_ready_seen`

---

## 5. 判讀重點

當 tracker 有切段時，除了看 `offline_tracker_timeline.csv` 之外，也應一起看：

- `offline_decision_timeline.csv`

因為門禁系統更在意的是：

- 是否形成穩定 decision session
- 是否最後能得到合理的 `unlock_ready`

而不是單純要求 `track_id` 永遠不變。

若 `max_short_gap_merges > 0`，表示 pipeline 已經把部分短斷軌接回同一次事件。
