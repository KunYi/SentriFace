# Dot Kernel Benchmark Guide

## 1. 目的

這份文件說明如何量測 `512-d` dot kernel 的 hot path。

它不是學術型 benchmark。
它的角色是讓開發者快速回答：

- 目前 build 實際選到哪個 backend
- `Dot512` 單次吞吐大約在哪個量級
- `Dot512Batch` 在目前 corpus size 下的吞吐與延遲

---

## 2. 執行檔

建置後可用：

```bash
./build/dot_kernel_benchmark_runner
./build/dot_kernel_benchmark_runner <prototypes> <iterations> [seed]
tools/run_dot_kernel_benchmark.sh
tools/run_dot_kernel_benchmark.sh <prototypes> <iterations> [seed]
```

預設值：

- `prototypes = 1000`
- `iterations = 200`
- `seed = 20260328`

---

## 3. 輸出欄位

runner 會輸出：

- `backend`
- `lanes`
- `prototypes`
- `iterations`
- `seed`
- `dot_seconds`
- `dot_ops_per_sec`
- `batch_seconds`
- `batch_ops_per_sec`
- `avg_batch_latency_ms`

另外也會印出：

- `dot_accumulator`
- `batch_accumulator`

這兩個欄位主要是避免 benchmark loop 被優化掉。

---

## 4. 資料模型

目前 benchmark 使用：

- fixed `512-d float`
- normalized query / prototype vectors
- contiguous `count x 512` prototype matrix

這和目前 `FaceSearch` / `FaceSearchV2` 的 512-d hot path 一致。

---

## 5. 使用建議

在 Intel 主機上：

- 可先觀察 `backend=avx2` 或 `backend=sse2`
- 可用較大的 `prototypes` 看 batch throughput 是否穩定

在 `ARMv7 Cortex-A7` 上：

- 應確認 build 真的落到 `backend=neon`
- 建議以接近產品容量的設定量測，例如：
  - `200`
  - `500`
  - `1000`

範例：

```bash
tools/run_dot_kernel_benchmark.sh 200 500
tools/run_dot_kernel_benchmark.sh 500 500
tools/run_dot_kernel_benchmark.sh 1000 300
```

---

## 6. 注意事項

- 這份 benchmark 是 repo 內 regression / bring-up 工具，不是最終產品 KPI。
- 主機上的結果不能直接宣稱等於 `RV1106` 板端結果。
- 真正板端結論仍應以：
  - cross build
  - target execution
  - 實機量測

為準。
