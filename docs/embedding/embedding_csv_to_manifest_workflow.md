# Embedding CSV To Manifest Workflow

## 1. 目的

這份文件定義如何將真實 embedding CSV 對齊到 tracker timeline，產生：

- `embedding_manifest.txt`

供 `offline_sequence_runner` replay 使用。

---

## 2. 工具

目前專案提供：

- `tools/convert_embedding_csv_to_manifest.py`

---

## 3. 用法

```bash
python3 tools/convert_embedding_csv_to_manifest.py \
  embedding.csv \
  offline_tracker_timeline.csv \
  embedding_manifest.txt \
  --frame-manifest offline_manifest.txt
```

---

## 4. 對齊方式

目前以：

- `timestamp_ms`
- bbox IoU

做對齊。

也就是：

1. 先找到同一 timestamp 的 tracker rows
2. 再以 bbox IoU 找最佳匹配 track
3. 若 IoU 達門檻，輸出該 `track_id` 的 embedding event

---

## 5. 目前用途

這條 workflow 的主要價值是：

- 把真實 embedding event 導入 replay
- 驗證 embedding-assisted re-link 是否在真實素材上成立
