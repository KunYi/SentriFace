# Bring-up Notes

本文件只記錄目前仍有價值的 bring-up 現象、已知問題與非阻塞風險。
若某項已經收斂成正式基線，應搬到：
- [docs/project/decision_log.md](docs/project/decision_log.md)
- 或相關模組 spec

---

## Host Qt Enrollment

- `WebCAM preview` 先前黑畫面問題已修正；目前本機 preview 已可用
- host Qt enrollment 在目前桌面 `WebCAM` 測試環境下，guidance ready 條件已暫時放寬：
  - `quality_score` ready 門檻由較保守值下修到約 `0.62`
  - `yaw / pitch / roll` 容忍度略放寬
  - 目的僅是讓 `webcam-first` bring-up 能持續往前，不代表最終量產基線
- 目前 host enrollment capture 已改為 slot-based collection window：
  - 每個 slot 約收集 `3 秒`
  - 收集期間保留多個候選
  - 最後挑特徵可信度較高的樣本保存
- host enrollment 的 tuning 參數已集中到：
  - [host/qt_enroll_app/include/qt_enroll_app/tuning.hpp](../../host/qt_enroll_app/include/qt_enroll_app/tuning.hpp)
  - 後續調整前後左右門檻、橢圓比例、姿態角度與收集窗口時，應優先修改這個檔案
- 目前第一輪真實測試顯示：
  - 原本「卡在 first step 只顯示 Hold still」主因是 test environment 下 `quality_score` 偏低
  - 不是 preview / bbox 映射主因
- 目前已取得真實 artifact：
  - `E1001_20260328_193342`
  - `E1002_20260328_195030`
- 這兩包真實 artifact 的 face crop 目前判讀為：
  - 整體可用
  - 無明顯左右偏切或嚴重裁壞
  - 仍屬近距離、低光與桌面 webcam 條件下的 bring-up 素材
- `E1002` 的最新樣本品質目前約：
  - `quality_score = 0.63 ~ 0.64`
  - `sample_weight = 1.14 ~ 1.20`
- `Qt offscreen` 可能出現：
  - `This plugin does not support propagateSizeHints()`
  這目前視為已知非致命 warning
- host enrollment accepted frame 與 face crop 已會保存
- host enrollment UI 近期已收斂：
  - 正常模式移除多餘 debug 文本
  - 偵測框與十字線只留在 debug 模式
  - 姿態提示改為中央附近的圖示化引導
- `face crop` 在真實 video stream 下仍需持續驗證，不應過早視為完全正確

---

## Mock / Bring-up Boundaries

- `GenerateBaselinePrototypePackage(...)` 已是正式 dispatch 邊界
- 目前已補上 `onnxruntime` backend，可直接由 artifact preferred image 產生真實 embedding package
- `GenerateMockBaselinePrototypePackage(...)` 只用於打通 artifact -> baseline package -> store
- mock backend 不是最終真實 alignment / embedding runtime
- `enrollment_artifact_runner` 是 bring-up 工具，不是最終 production import pipeline
- 但它目前已會產出正式 binary 邊界：
  - `.sfbp`
  - `.sfsi`

---

## Validation Ordering

- host enrollment 問題請優先在 `WebCAM` 路徑收斂
- 不要一開始把：
  - UI / preview
  - RTSP
  - RV1106 板端部署

混在一起查

- 目前大多數已打通流程仍屬：
  - `PC + ONNX + local WebCAM`
- `RTSP`
- `RV1106G2 / RV1106G3`
- `RKNN runtime`
  都仍屬後續必要驗證項，不應提前宣稱已收斂

---

## Search / Scale

- local-first 目前以 `200 persons` 驗證、`200 ~ 500 persons` 目標為主
- 更大規模的 host/server 分層設計已有方向，但仍以後續實測與產品需求為準
