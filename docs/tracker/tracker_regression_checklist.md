# Tracker Regression Checklist

## 1. 目的

這份文件定義 stage-1 tracker regression 的基本判讀標準。

它不是最終 production benchmark，而是讓開發者能快速回答：

- 這段短 clip 的追蹤結果有沒有明顯異常
- 是否值得繼續往後面的 embedding / decision 流程推

---

## 2. 建議輸入

第一版建議以：

- 單人
- 近距離
- webcam 或 RTSP 抽幀
- 5 到 15 秒短 clip

作為最小驗證基線。

---

## 3. 核心觀察項目

優先看：

- `unique_track_ids`
- `state_confirmed`
- `state_lost`
- 每個 track 的 `max_hits`
- 每個 track 的 `best_score`

---

## 4. Stage-1 判讀原則

對單人短 clip，第一版可用的簡單判讀如下：

1. `unique_track_ids = 1`
   最理想，代表單人 clip 幾乎沒有 ID fragmentation。

2. `unique_track_ids <= 3`
   仍可接受，代表 tracker 大致可用，但存在一些斷裂或重建。

3. `unique_track_ids > 3`
   應優先 review，通常表示追蹤穩定度不足。

4. 至少有一條 track 達到：
   - `max_hits >= 4`
   - `best_score >= 0.70`
   這可視為「本段 clip 至少有形成強 confirmed track」。

5. `state_lost / state_confirmed`
   若這個比例偏高，通常代表：
   - detection 抖動仍大
   - clip 條件不穩
   - tracker 參數可能還要調

---

## 5. 自動化工具

目前專案提供：

- `tools/summarize_tracker_timeline.py`
- `tools/evaluate_tracker_summary.py`

建議流程：

```bash
python3 tools/summarize_tracker_timeline.py \
  offline_tracker_timeline.csv \
  tracker_summary.txt

python3 tools/evaluate_tracker_summary.py \
  tracker_summary.txt \
  tracker_evaluation.txt
```

---

## 6. 注意事項

這份 checklist 只適合：

- stage-1
- controlled single-person validation

它不能直接外推到：

- 多人同框
- 門口交錯通行
- production acceptance test

正式 acceptance criteria 仍應建立在：

- RTSP 實機資料
- 更多 clip
- 實際門禁站位與光線條件

---

## 7. 場景註記的重要性

在解讀 tracker regression 時，務必同時記錄 clip 的場景條件，例如：

- 實際只有 1 個人
- 使用者是否主動轉頭、移動頭部
- 背景是否存在照片、海報、螢幕中的小臉

這些資訊會直接影響：

- `unique_track_ids`
- 背景假陽性
- fragmentation 的解讀方式

例如：

- 若 clip 實際只有 1 人，但背景有固定小頭像

那麼出現額外 track 時，應先懷疑：

- 同一位操作者因姿態改變被切段
- 背景小臉造成固定假陽性

而不是先把結果解讀成正常多人追蹤行為。

---

## 8. 門禁場景的小臉淘汰原則

對門禁系統來說，遠距離多人或背景小頭像通常不屬於有效使用情境。

因此第一版建議明確採用 detector-side 最小臉尺寸過濾，例如：

- `min_face_width >= 120`
- `min_face_height >= 120`

這樣做的目的不是提高一般監控召回率，而是讓：

- tracking 更穩
- 背景小頭像不易形成假 track
- 後續 alignment / embedding / decision 更貼近真實門禁流程

若在單人近距離 clip 中套用最小臉尺寸後，tracker 結果仍出現多條 track，則應更優先懷疑：

- 使用者姿態變化導致 fragmentation
- association gate 仍需調整

而不是先歸因於遠處小臉干擾。
