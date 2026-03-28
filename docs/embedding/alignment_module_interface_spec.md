# Alignment Module Interface Spec

## 1. 目的

這份文件定義第一版 `alignment` 模組介面。

第一版先專注於：

- 接收 `5 landmarks`
- 計算 `image -> aligned face` 的 2x3 affine transform
- 定義固定輸出尺寸

暫時不綁定任何影像後端。

---

## 2. 介面定位

目前模組位置：

```text
include/align/face_aligner.hpp
src/align/face_aligner.cpp
```

主要類型：

- `AlignmentConfig`
- `AffineTransform2D`
- `AlignmentResult`
- `FaceAligner`

---

## 3. 第一版設計

`FaceAligner` 以 tracker / detector 提供的 `5 landmarks` 為輸入，估計對齊變換：

```cpp
class FaceAligner {
 public:
  explicit FaceAligner(const AlignmentConfig& config = AlignmentConfig {});

  AlignmentResult Estimate(const sentriface::tracker::Landmark5& landmarks) const;
};
```

### 3.1 預設模板

第一版使用常見的 `112 x 112` 五點模板。

### 3.2 輸出

輸出結果包含：

- `ok`
- `image_to_aligned`
- `output_width`
- `output_height`

---

## 4. 為什麼先只做幾何層

這樣做的好處：

- 容易測試
- 不被 OpenCV / RGA / ISP API 綁死
- 後續可把 transform 餵給不同影像後端

這很適合目前的 `SDD + TDD` 開發節奏。

---

## 5. CPU-First 與板端遷移

alignment 模組同樣遵守：

- 先在 `Intel CPU` 環境完成幾何與介面驗證
- 再遷移到板端影像處理路徑

之後若要接：

- OpenCV warpAffine
- RGA
- RKNN 前處理

都應沿用這份幾何輸出規格，而不是重新定義 alignment 介面。

### 5.1 RKNN 目標下的 alignment 約束

alignment 雖然先做幾何層，但實際設計仍應考慮：

- 板端 warp 成本不能太高
- 輸出尺寸要與後續 embedding 模型一致
- 不應設計成高解析、多分支、重複對齊流程

因此第一版固定 `112 x 112` 是有意識的板端導向選擇。
