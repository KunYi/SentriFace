# SCRFD Model IO Verified

## 1. 目的

這份文件記錄目前已實際驗證的 `SCRFD` ONNX model I/O 結果，避免後續 decode 實作仍建立在猜測上。

---

## 2. 已驗證模型

目前已實際檢查：

- `third_party/models/buffalo_sc/det_500m.onnx`

檢查方式：

- `.venv` 內安裝 `onnx` 與 `onnxruntime`
- `tools/inspect_scrfd_onnx.py`

---

## 3. 實際 Input

Graph / session 顯示的 input 為：

```text
input.1 [1, 3, ?, ?]
```

也就是：

- batch 固定 `1`
- channel 固定 `3`
- spatial size 為 dynamic
- tensor type 為 `float`

這與目前 `ScrfdFaceDetector` 的 `CHW float tensor` preprocessing 設計一致。

---

## 4. 實際 Outputs

目前已讀出的 outputs 共 `9` 個，可分成三組 head：

### 4.1 Score heads

```text
[12800, 1]
[3200, 1]
[800, 1]
```

### 4.2 Box heads

```text
[12800, 4]
[3200, 4]
[800, 4]
```

### 4.3 Landmark heads

```text
[12800, 10]
[3200, 10]
[800, 10]
```

---

## 5. 推導出的結構

如果輸入採用 `640 x 640`，則：

- `12800 = 80 x 80 x 2`
- `3200 = 40 x 40 x 2`
- `800 = 20 x 20 x 2`

這強烈表示目前這個 detector 的 head 結構可視為：

- feature strides: `8, 16, 32`
- anchors per location: `2`
- 每個 stride 都有：
  - score branch
  - box branch
  - landmark branch

這是後續 decode 實作非常關鍵的事實。

---

## 6. 對系統實作的意義

這個驗證結果代表：

1. `ScrfdFaceDetector` 的 preprocess 方向是對的
2. 後續 decode 不應再用通用猜測，而應以：
   - 3 scales
   - 2 anchors
   - score/box/landmark 三分支
   為基礎
3. 若之後接 RKNN 版本，也應確認輸出 head 是否維持相同語意

---

## 7. 工具

可直接用以下工具重跑檢查：

```bash
.venv/bin/python tools/inspect_scrfd_onnx.py third_party/models/buffalo_sc/det_500m.onnx
```
