# RGB Liveness Anti-Spoof Spec

## 1. 目的

這份文件定義第一版 `RGB camera` 輕量 anti-spoof / liveness 邊界。

目標不是做高成本、重模型、強對抗等級的 PAD，而是先在：

- `SC3336`
- 單 RGB camera
- `RV1106`
- 固定式近距離門禁

這個條件下，提供可落地、可量測、可維護的第一版防照片 / 防螢幕重播能力。

---

## 2. 範圍與定位

第一版定位：

- `RGB-only`
- `CPU-first`
- 盡量重用現有：
  - detector bbox
  - `SCRFD` 5 landmarks
  - tracker history
  - face quality / pose

不預設引入：

- IR camera
- depth sensor
- 大型 liveness model
- 高成本 challenge-response UX

這代表第一版是：

- `anti-photo / anti-screen baseline`

不是：

- 高安全等級 presentation attack defense

---

## 3. Threat Model

### 3.1 第一版要優先處理的攻擊

- 手持平面照片
- 列印照片
- 手機 / 平板螢幕播放的人臉
- 粗糙的 face crop 翻拍

### 3.2 第一版不承諾處理的攻擊

- 高品質 3D 面具
- 高品質半立體紙模
- 高品質 replay，且攻擊者能精細模仿 challenge
- 專門針對本系統調參過的對抗樣本
- 多光源精細控制下的反射欺騙

### 3.3 文件化結論

第一版可宣稱：

- 能提升低成本假體攻擊門檻
- 能作為 unlock / adaptive update 的安全 gating

第一版不可宣稱：

- 符合金融或高安全等級 PAD
- 可單獨對抗高品質 spoof

---

## 4. 設計原則

### 4.1 輕量

運算應優先落在：

- landmark 幾何
- 短時序 motion
- 局部亮度 / 紋理統計

避免：

- 每幀大型 CNN
- 大量 optical flow dense 計算
- 高解析多尺度影像分析

### 4.2 Event-driven

liveness 不應每幀都做重判斷。

應以：

- `track` 為單位
- 在 face quality 可用時更新
- 在 unlock / adaptive update 前做 gating

### 4.3 可退化

若環境條件不足，應允許輸出：

- `unknown`

而不是強行把低資訊條件判成：

- `live`

---

## 5. 第一版輸入

每次 liveness update 可使用：

- face bbox
- 5 landmarks
- detection score
- face quality score
- track history
- 短時間的 face crop / ROI

可選：

- 當前 guidance/challenge state

---

## 6. 第一版特徵

### 6.1 Pose / Motion Consistency

觀察短時間內：

- `yaw`
- `pitch`
- `roll`
- bbox center
- bbox size

是否存在自然微動。

預期：

- 真人會有小幅、連續、非完美剛體的自然變化
- 平面照片 / 螢幕常呈現整張剛體平移或幾乎無臉內相對變化

### 6.2 Landmark Relative Deformation

利用 5 landmarks 觀察：

- 鼻子相對雙眼中點的位置變化
- 嘴角相對雙眼線的幾何關係
- 臉部旋轉時的非剛體 / 透視變化

目標不是做 3D 重建，而是區分：

- 真臉的小透視差
- 平面介質的近似剛體運動

### 6.3 Local Brightness Response

在 face ROI 內定義少量固定區塊，例如：

- forehead
- nose bridge
- cheeks

觀察短時序亮度變化是否有：

- 曲面反射
- 區域不一致的自然細微亮度漂移

風險：

- 強環境光變動時，這個特徵會變弱

因此它只應作為弱特徵，不應單獨決策。

### 6.4 Texture Flatness Cue

可用低成本局部統計，例如：

- Laplacian variance
- high-frequency energy
- block contrast irregularity

用來輔助分辨：

- 列印照
- 螢幕摩爾紋
- 翻拍造成的異常平面紋理

此特徵只能作為弱特徵，不能獨立作 `live/spoof` 判定。

### 6.5 Challenge-lite Response

第一版允許非常輕量的 challenge，例如：

- 微微左轉
- 微微右轉
- 稍微抬頭

若 passive score 不足但仍接近可接受區，系統可要求這種低干擾 challenge。

目的：

- 低成本提升對平面照片 / 螢幕 replay 的攔截率

---

## 7. 第一版不做的特徵

- blink / eye-state 為主的強依賴方案
- mouth-open 為主的 challenge
- dense optical flow 全臉估計
- 3D face reconstruction
- 單張靜態深度估計模型
- 大型 anti-spoof CNN

理由：

- 成本高
- 對 RV1106 第一版不友善
- 會把主線從「門禁可落地」拉向研究型專案

---

## 8. 輸出介面

第一版建議輸出：

```cpp
enum class LivenessState : std::uint8_t {
  kUnknown = 0,
  kLikelyLive = 1,
  kLikelySpoof = 2,
};

struct LivenessResult {
  LivenessState state = LivenessState::kUnknown;
  float score = 0.0f;
  bool passive_ready = false;
  bool challenge_required = false;
  bool challenge_passed = false;
};
```

其中：

- `score` 建議範圍 `0.0 ~ 1.0`
- `kUnknown` 應是合法輸出

---

## 9. Decision Policy

### 9.1 Passive-only 通過

若：

- quality 足夠
- track 穩定
- passive liveness score 達標

可直接視為：

- `liveness_ok = true`

### 9.2 Challenge-lite 升級

若：

- passive score 落在灰區

則進入：

- challenge-lite

challenge 通過才允許：

- unlock ready
- adaptive prototype update

### 9.3 保守原則

若：

- 觀察時間太短
- 亮度條件太差
- face size 太小
- motion evidence 不足

應優先輸出：

- `unknown`

而不是 `live`

---

## 10. 與現有模組的責任分工

### 10.1 Detector / Tracker

提供：

- bbox
- landmarks
- track history

### 10.2 Liveness 模組

負責：

- 計算 liveness features
- 維護短時序 evidence
- 輸出 `LivenessResult`

### 10.3 Pipeline / Decision

負責：

- 把 `liveness_ok` 納入 unlock gating
- 把 `liveness_ok` 納入 adaptive update gating

### 10.4 Enrollment Store V2

不直接做 liveness 推論，只吃：

- `metadata.liveness_ok`

---

## 11. 與現有主線的整合優先順序

第一版建議整合順序：

1. offline replay 上先驗證 liveness feature extraction
2. sample / offline runner 可回報 `liveness_score`
3. pipeline 先把 `liveness_ok` 接到 adaptive update gating
4. 再接到 unlock gating
5. 最後才接 host enrollment UX 的 challenge-lite 提示

---

## 12. 驗證資料需求

至少要收：

- 真人近距離自然站位
- 真人小幅左右轉頭
- 真人小幅前後移動
- 列印照平移 / 傾斜
- 手機螢幕重播平移 / 傾斜

驗證時應至少分開看：

- passive score 分佈
- challenge 通過率
- 真人 false reject
- spoof false accept

---

## 13. KPI 建議

第一版建議先看：

- 真人通過率
- 照片 / 螢幕攻擊攔截率
- 額外延遲
- 每次 update 的 CPU 成本

第一版不應先承諾正式 FAR / FRR 數字，除非資料集與條件已固定。

---

## 14. 風險與限制

- 單 RGB camera 天生有安全上限
- 低光 / 背光下 brightness feature 會退化
- 小臉 / 遠臉下 landmarks 幾何不穩
- 使用者過度僵硬時 passive score 可能偏低
- 過於激進的 challenge 會傷 UX

因此第一版的正確定位是：

- `lightweight anti-spoof baseline`

不是：

- `high-assurance PAD`

---

## 15. 結論

對本專案目前條件，第一版最合理方向是：

- `RGB-only`
- `lightweight passive liveness`
- 必要時搭配 `challenge-lite`

它能在不引入重模型與額外硬體的前提下，先有效提升：

- 防照片
- 防螢幕 replay

但仍必須明確承認：

- 對高品質 spoof 的防護能力有限
- 最終安全邊界仍取決於後續實測與產品 threat model
