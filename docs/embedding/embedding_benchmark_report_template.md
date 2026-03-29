# Embedding Benchmark Report Template

## 1. Report Info

- date：
- author：
- candidate：
- model file：
- backend：
- board / host：
- dataset / artifact set：

---

## 2. Objective

本次 benchmark 的目標是：

- [ ] host reference
- [ ] ONNX vs RKNN consistency
- [ ] board latency
- [ ] access-control integration

---

## 3. Environment

- toolchain：
- onnxruntime version：
- rknn toolkit version：
- board firmware：
- build flags：

---

## 4. Input Definition

- input resolution：
- embedding dim：
- sample count：
- persons count：
- capture condition：

---

## 5. Raw Metrics

### 5.1 Runtime

- init latency：
- avg inference latency：
- p95 inference latency：
- peak memory：

### 5.2 Embedding Consistency

- same-sample ONNX/RKNN cosine mean：
- same-sample ONNX/RKNN cosine min：
- top-1 identity agreement：
- top1-top2 margin delta mean：

### 5.3 Pipeline Metrics

- verify top-1 hit rate：
- false accept observations：
- false reject observations：
- unlock-ready latency：

---

## 6. Qualitative Notes

- strong cases：
- weak cases：
- suspected failure mode：
- preprocessing issues：
- quantization issues：

---

## 7. Decision

- result：
  - `promote`
  - `hold`
  - `drop`

- rationale：

- next step：

