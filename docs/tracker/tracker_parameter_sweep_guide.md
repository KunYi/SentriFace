# Tracker Parameter Sweep Guide

## 1. 目的

這份文件定義如何對同一批 offline replay 素材做小範圍 tracker 參數掃描。

它的主要用途是：

- 觀察 `unique_track_ids` 是否可下降
- 比較 `state_confirmed` / `state_lost`
- 幫 stage-1 webcam clip 找到較穩定的追蹤參數

---

## 2. 工具

目前專案提供：

- `tools/sweep_tracker_params.py`

它會呼叫：

- `build/offline_sequence_runner`
- `tools/summarize_tracker_timeline.py`
- `tools/evaluate_tracker_summary.py`

---

## 3. 用法

```bash
python3 tools/sweep_tracker_params.py \
  <frame_manifest> \
  <detection_manifest> \
  <output_csv>
```

範例：

```bash
python3 tools/sweep_tracker_params.py \
  artifacts/webcam_bright/offline_manifest.txt \
  artifacts/webcam_bright/detection_manifest.txt \
  artifacts/webcam_bright/tracker_sweep.csv
```

若要明確指定 sweep 類型：

```bash
python3 tools/sweep_tracker_params.py \
  --profile geometry \
  artifacts/webcam_bright/offline_manifest.txt \
  artifacts/webcam_bright/detection_manifest.txt \
  artifacts/webcam_bright/tracker_sweep_geometry.csv
```

---

## 4. 目前掃描範圍

`baseline` profile 會掃描：

- `min_confirm_hits`
- `max_miss`
- `iou_min`

這三個參數已足夠做 stage-1 單人短 clip 的初步收斂。

`geometry` profile 會固定 baseline 為：

- `min_confirm_hits = 2`
- `max_miss = 2`
- `iou_min = 0.20`

並改為掃描：

- `center_gate_ratio`
- `area_ratio_min`
- `area_ratio_max`

這個 profile 更適合目前這種：

- 單人
- 近距離
- 有轉頭或頭部位移
- 背景可能存在固定小頭像

的門禁驗證素材。

---

## 5. 判讀方式

優先找：

- `overall=pass`
- `unique_track_ids` 越小越好
- `state_confirmed` 不要掉太多
- `state_lost` 不要大幅升高

對單人短 clip 而言，理想目標是：

- 盡量接近 `unique_track_ids=1`

但第一版若能從明顯 fragmentation 降到：

- `2`
- 或穩定停留在 `3` 以下

也具有實用價值。

---

## 6. 目前 `webcam_bright` 幾何 sweep 結果

針對 `artifacts/webcam_bright/` 這批單人近距離素材，目前已做過一輪
`geometry` profile 掃描。

主要觀察如下：

- `center_gate_ratio` 在 `0.45 / 0.60 / 0.75` 間，對結果幾乎沒有主導性影響
- `area_ratio_max = 1.60` 明顯過緊，會使 `unique_track_ids` 惡化到 `5`
- `area_ratio_min = 0.65` 也偏緊，會讓結果從 `3` 惡化到 `4`
- `area_ratio_max = 2.00` 或 `2.40` 都維持較穩定結果

因此目前較合理的 stage-1 baseline 仍建議維持：

- `center_gate_ratio = 0.60`
- `area_ratio_min = 0.50`
- `area_ratio_max = 2.00`

對這批素材來說，若 tracker 仍有 fragmentation，下一步應優先懷疑：

- 使用者姿態變化過大
- detection 時序抖動
- 是否需要更明確的 re-association 策略

而不是繼續把幾何 gate 一路收緊。
