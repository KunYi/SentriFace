# Tracker Timeline Summary Guide

## 1. 目的

這份文件定義如何把 `offline_tracker_timeline.csv` 轉成較容易閱讀的追蹤摘要。

它適合用於：

- webcam clip 的 tracker sanity check
- replay 驗證後快速看 `track_id` 數量與狀態變化

---

## 2. 工具

目前專案提供：

- `tools/summarize_tracker_timeline.py`

---

## 3. 用法

```bash
python3 tools/summarize_tracker_timeline.py \
  offline_tracker_timeline.csv \
  tracker_summary.txt
```

---

## 4. 主要欄位

摘要會包含：

- `rows`
- `frames_with_tracks`
- `unique_track_ids`
- `track_ids`
- `state_tentative`
- `state_confirmed`
- `state_lost`

以及每個 `track_id` 的：

- `samples`
- `first_ts`
- `last_ts`
- `max_hits`
- `max_miss`
- `best_score`

---

## 5. 目前用途

第一版主要是讓開發者能快速回答：

- 這段 clip 總共追到幾條 track
- 哪些 track 進入 confirmed
- lost 狀態出現多少次
- 是否看起來有明顯 ID 斷裂

---

## 6. 單人場景的判讀提醒

若場景已知是：

- 單人
- 使用者有轉頭或頭部移動
- 背景含有固定小頭像或照片

則 timeline 中出現多條 track 時，優先解讀方向應是：

- 同一位使用者的姿態/位置變化造成 fragmentation
- 背景固定小臉造成額外低穩定度 track

不應直接把這種 timeline 當成多人通行場景的代表結果。

---

## 7. 門禁場景的實務解讀

對門禁系統來說，若 detector 端已經有最小臉尺寸過濾，例如：

- `min_face_width >= 120`
- `min_face_height >= 120`

那麼 timeline 中仍出現多條 track 時，通常更應該優先檢查：

- 單人轉頭或頭部位移是否造成 bbox 變化過大
- association gate 是否過緊
- detection 是否在部分快速動作下有短暫不穩

也就是說，當遠處小臉已先被淘汰後，timeline 的 fragmentation 會更接近真正的
單人追蹤穩定度問題，而不是背景小頭像的干擾問題。
