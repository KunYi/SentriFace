# Decision Regression Checklist

## 1. 目的

這份文件定義 stage-1 `decision session` 驗證的基本判讀標準。

它回答的不是：

- tracker 有沒有完全不切段

而是：

- 門禁事件層是否仍然穩
- `unlock_ready` 是否有形成
- embedding / short-gap merge 是否帶來改善

---

## 2. 工具

目前專案提供：

- `tools/summarize_decision_timeline.py`
- `tools/evaluate_decision_summary.py`

---

## 3. 基本流程

```bash
python3 tools/summarize_decision_timeline.py \
  offline_decision_timeline.csv \
  decision_summary.txt

python3 tools/evaluate_decision_summary.py \
  decision_summary.txt \
  decision_evaluation.txt
```

若要和 baseline 比較：

```bash
python3 tools/evaluate_decision_summary.py \
  decision_summary_after.txt \
  decision_evaluation_after.txt \
  --baseline-summary decision_summary_before.txt
```

---

## 4. 主要觀察項目

優先看：

- `unlock_ready_rows`
- `max_short_gap_merges`
- `unique_track_ids`
- 每個 `track_id` 的 `max_consistent_hits`
- 每個 `track_id` 是否曾出現 `unlock_ready`

---

## 5. Stage-1 判讀原則

對單人短 clip，第一版可用的簡單判讀如下：

1. 只要 `unlock_ready_rows > 0`
   代表 decision layer 至少成功形成過可用門禁結果。

2. `unique_track_ids <= 3`
   在單人短 clip 下仍屬可接受。

3. 若 `max_short_gap_merges > 0`
   代表 pipeline 的事件級補強有真正被用到。

4. 若和 baseline 比較後：
   - `unlock_ready_rows` 增加
   - `max_short_gap_merges` 增加

   可視為 embedding / re-link 方向帶來正向效果。

---

## 6. 注意事項

decision regression 不能單獨解讀，最好和：

- `offline_tracker_timeline.csv`
- `tracker_summary.txt`
- clip 場景註記

一起看。

因為門禁最終在意的是：

- 事件層是否穩

而不是單純要求 tracker 從頭到尾只出現一個 `track_id`。
