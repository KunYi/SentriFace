# Geometry Numeric Boundary

## 1. 目的

這份文件用來釐清本專案中：

- 哪些資料應使用整數
- 哪些資料應保留浮點
- 什麼時候應做 rounding / clamp

避免在實作時把：

- pixel domain
- geometry / estimation domain

混在一起。

---

## 2. 核心原則

可以用一句話理解：

- **影像像素格點是整數**
- **幾何估計與狀態可以是浮點**

也就是說：

- 圖片 buffer 上的 pixel index 一定是整數
- 但 detector / tracker / alignment 中間算出的 bbox、landmark、state 可以是浮點

---

## 3. 應使用整數的地方

以下資料原則上應以 `int` / `uint32_t` 等整數表示：

- image width / height
- pixel `(x, y)` index
- buffer offset
- stride
- channels
- frame count
- track count
- ID / state enum / counters
- 真正送進 crop / draw / memory access / hardware API 的座標

簡單講：

- **只要會直接碰 image memory 或硬體邊界，就應回到整數**

---

## 4. 應使用浮點的地方

以下資料保留 `float` 較合理：

- detector bbox
- detector landmarks
- Kalman filter state
- Kalman covariance / update
- IoU 幾何計算
- resize / letterbox / reverse mapping
- affine transform
- alignment 幾何
- embedding similarity / score

原因是這些屬於：

- 估計值
- 連續空間幾何
- 次像素（sub-pixel）精度

若太早轉成整數，容易造成：

- tracking 抖動
- alignment 不穩
- decode / remap 誤差放大

---

## 5. 實務分層

本專案建議用下面的分層理解：

### 5.1 Pixel Domain

使用整數。

例如：

- frame size
- crop region 實際邊界
- draw rectangle
- image buffer read / write

### 5.2 Geometry Domain

使用 `float`。

例如：

- `RectF`
- landmark point
- tracking state
- letterbox 映射

### 5.3 Boundary Conversion

在「幾何層 -> 像素層」交界時，明確做：

- `round`
- `floor`
- `ceil`
- `clamp`

不要在演算法中途過早整數化。

---

## 6. Rounding / Clamp 準則

當浮點座標要進入像素層時，應明確定義規則：

- left / top：
  常用 `floor`
- right / bottom：
  常用 `ceil`
- center point / landmark draw：
  常用 `round`
- 所有輸出到 frame 邊界的值：
  都必須 `clamp` 到合法範圍

例如：

```text
x1 = floor(x)
y1 = floor(y)
x2 = ceil(x + w)
y2 = ceil(y + h)
```

再做：

```text
x1 = clamp(x1, 0, width)
y1 = clamp(y1, 0, height)
x2 = clamp(x2, 0, width)
y2 = clamp(y2, 0, height)
```

---

## 7. 本專案當前結論

目前使用 `float` 表示：

- bbox
- landmarks
- tracker state

是合理且建議保留的。

目前應避免的是：

- 不必要使用 `double`
- 在應該用整數的 pixel / buffer 邊界亂用浮點
- 在演算法中途太早把幾何值強制轉成整數

所以可以把目前設計理解成：

- **演算法內部：float**
- **碰到影像記憶體或硬體邊界：int**

---

## 8. 與 RV1106 32-bit 原則的關係

這份規則與 `RV1106` 的 32-bit 原則不衝突。

本專案目前的型別策略仍是：

- 能用整數就不用浮點
- 必須用浮點時優先 `float`
- 不任意引入 `double`

也就是：

- 不是所有座標都該用 `int`
- 也不是所有計算都該用浮點
- 而是依資料所處的 domain 決定
