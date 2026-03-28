# Mock Embedding Replay Workflow

## 1. 目的

這份文件定義如何從既有的 `offline_tracker_timeline.csv` 產生一份
`mock embedding manifest`，用來驗證：

- embedding-assisted re-link
- short-gap merge
- decision session replay

---

## 2. 工具

目前專案提供：

- `tools/generate_mock_embedding_manifest.py`

---

## 3. 用法

```bash
python3 tools/generate_mock_embedding_manifest.py \
  offline_tracker_timeline.csv \
  mock_embedding_manifest.txt \
  --track-ids 0,1,2 \
  --vector 1.0,0.0,0.0,0.0 \
  --confirmed-only
```

---

## 4. 適用情境

這條 workflow 適合：

- 先用 detection replay 得到 tracker timeline
- 想快速驗證 re-link 規則本身
- 暫時還沒有真實 embedding backend / export 工具

它不是 production 模式，而是：

- 受控驗證工具

---

## 5. 限制

由於這份 manifest 是人工或半人工產生的，它只能回答：

- 若某些 track 擁有相近 embedding，pipeline 會不會把它們接起來

它不能直接回答：

- 真實 embedder 在現場是否穩定
- 真實 embedding 相似度分布是否合理

因此後續仍需要：

- 真實 embedding event export
- 真實素材 replay
