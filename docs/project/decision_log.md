# Decision Log

本文件記錄目前已確認的高價值設計決策。
請依功能區塊查閱，不要全文通讀後再開始工作。

---

## Project Identity

- 文件主名稱暫定為 `SentriFace`

---

## Hardware / Platform

- 目標板端：`Luckfox Pico RV1106G2 / RV1106G3`
- 目前主力板型：`Pico Max (RV1106G3)`
- 鏡頭：`SC3336 3MP`
- 安裝方向：直立模式
- 型別策略以 `32-bit RV1106` 為前提：
  - 優先 `int32/uint32`
  - 避免任意擴散 `int64`
  - 避免不必要的 `double`

---

## Detection

- detection 主線採 `SCRFD-500M`
- `SCRFD` 已是正式整合方向，不只是評估候選
- detector 預設先過濾小臉：
  - `min_face_width = 120`
  - `min_face_height = 120`
- 門禁場景下應優先讓大臉 / 近臉排在前面
- 若站位區域已知，可使用 detector ROI 排除非有效區域
- `SCRFD` 的 CPU / ONNX 路線已完成 bring-up；RKNN 仍需板端實測驗證
- ONNX 可跑不代表 RKNN 可部署；operator compatibility 必須單獨驗證

相關文件：
- [docs/detector/detector_module_interface_spec.md](docs/detector/detector_module_interface_spec.md)
- [docs/detector/scrfd_detector_backend_spec.md](docs/detector/scrfd_detector_backend_spec.md)
- [docs/platform/rknn_operator_compatibility_risk.md](docs/platform/rknn_operator_compatibility_risk.md)

---

## Tracking / Relink

- tracking 主方案採 `IoU + Kalman Filter`
- state 以 `cx, cy, w, h, vx, vy, vw, vh` 為主
- stage-1 geometry baseline：
  - `center_gate_ratio = 0.60`
  - `area_ratio_min = 0.50`
  - `area_ratio_max = 2.00`
- 允許在 pipeline / decision 層做短時間 re-link
- embedding-assisted relink 是保守補強，不是每幀 appearance tracker
- 前一段 track 要先有基本穩定度，才允許透過 embedding 補接

相關文件：
- [docs/tracker/tracker_module_interface_spec.md](docs/tracker/tracker_module_interface_spec.md)
- [docs/embedding/embedding_assisted_relink_design.md](docs/embedding/embedding_assisted_relink_design.md)
- [docs/tracker/relink_parameter_sweep_guide.md](docs/tracker/relink_parameter_sweep_guide.md)

---

## Alignment / Embedding

- alignment 第一版採 `SCRFD 5 landmarks`
- embedding 的產品定位是 event-driven identity signal
- 不做每幀高頻 embedding
- 第一版產品化延遲目標：`2 秒內` 得到穩定門禁結果
- embedding cache 優先用於：
  - short-gap relink
  - 節流，避免重複推論

相關文件：
- [docs/embedding/alignment_module_interface_spec.md](docs/embedding/alignment_module_interface_spec.md)
- [docs/embedding/embedding_module_interface_spec.md](docs/embedding/embedding_module_interface_spec.md)
- [docs/embedding/embedding_product_strategy.md](docs/embedding/embedding_product_strategy.md)

---

## Search / Capacity

- search V1 先做 local exact / weighted search
- search V2 方向：
  - prototype-level weighted score
  - person-level max aggregation
- local-first 產品基線：
  - 驗證：`200 persons`
  - 實務目標：`200 ~ 500 persons`
- `10,000+ persons` 不應預設由 `RV1106` 完整承擔 full search
- 大規模查詢應優先考慮 host/server 分層

相關文件：
- [docs/search/search_module_interface_spec.md](docs/search/search_module_interface_spec.md)
- [docs/search/search_v2_interface_spec.md](docs/search/search_v2_interface_spec.md)
- [docs/search/weighted_search_spec.md](docs/search/weighted_search_spec.md)
- [docs/search/large_scale_identity_search_strategy.md](docs/search/large_scale_identity_search_strategy.md)

---

## Enrollment / Prototype Store

- prototype store 應朝三區演進：
  - `baseline`
  - `verified history`
  - `recent adaptive`
- `verified history` 必須有容量上限
- 第一版基線：每人最多 `5` 個 verified history prototypes
- adaptive update 只能在高信心條件下發生
- 不得直接覆蓋原始 enrollment baseline
- host enrollment artifact 已是正式導入邊界
- baseline sample selection 的正式邊界是：
  - `BuildBaselineEnrollmentPlan(...)`
- baseline generation 應保留正式 backend dispatch 邊界：
  - `GenerateBaselinePrototypePackage(...)`
- mock baseline generation 目前只作 bring-up：
  - `GenerateMockBaselinePrototypePackage(...)`

相關文件：
- [docs/enrollment/enrollment_module_interface_spec.md](docs/enrollment/enrollment_module_interface_spec.md)
- [docs/enrollment/enrollment_store_v2_spec.md](docs/enrollment/enrollment_store_v2_spec.md)
- [docs/enrollment/adaptive_prototype_policy.md](docs/enrollment/adaptive_prototype_policy.md)
- [docs/enrollment/enrollment_artifact_runner_guide.md](docs/enrollment/enrollment_artifact_runner_guide.md)

---

## Host Enrollment Tool

- host 端應有獨立 `Qt 5.15.x` enrollment tool
- 目前驗證順序固定：
  - `WebCAM`
  - `RTSP`
  - `RV1106` 實機
- UI 應偏產品化：
  - 一次按 `Start Enrollment`
  - 中間自動引導
  - overlay 提示
  - 橢圓虛擬框
- host enrollment sample selection 應偏向「特徵可信度高」而不是只看單張畫質：
  - 臉在有效目標區中心附近
  - 尺寸適中
  - slot 姿態接近目標
  - pitch / roll 較穩
  - quality 較高
  - 在收集窗口中表現穩定
- host enrollment capture 應採 slot-based collection window：
  - 每個 slot 先收集數秒候選
  - 再選較佳樣本保存
  - 不採一達標就立刻抓單張的策略
- 測試 / bring-up 階段可先把 `User ID / Name` 與 guided capture 放同畫面
- accepted frame 與 face crop 必須保存
- `WebCAM-first` bring-up 條件允許暫時放寬，但必須明確記錄，後續再依 RTSP / 實機環境重調

相關文件：
- [docs/host/host_qt_enroll_tool_spec.md](docs/host/host_qt_enroll_tool_spec.md)
- [docs/host/host_qt_enroll_state_machine.md](docs/host/host_qt_enroll_state_machine.md)
- [docs/host/host_qt_enroll_webcam_guide.md](docs/host/host_qt_enroll_webcam_guide.md)

---

## Access / Unlock

- unlock backend 應視為通用控制抽象，不只 GPIO
- backend 方向：
  - `dummy`
  - `stdout`
  - `libgpiod`
  - `uart`
  - `network_rpc`
- unlock 行為應建立在 temporal smoothing / decision 之後

相關文件：
- [docs/access/access_control_backend_spec.md](docs/access/access_control_backend_spec.md)

---

## Logging / Diagnostics

- logging 是輔助系統，不是主線交付物
- 呼叫端優先採 `TAG + macro`
- text log 預設保留：
  - wall time
  - mono elapsed
  - level/tag
  - message
- 長期方向採 binary writer + viewer/dumper
- compression：
  - `gzip` 為 baseline
  - `zstd` 為優先升級方向
- `mono_ms = 碼表`
- `wall time = 時鐘`

相關文件：
- [docs/logging/logging_system_spec.md](docs/logging/logging_system_spec.md)
- [docs/logging/binary_logging_pipeline_spec.md](docs/logging/binary_logging_pipeline_spec.md)
- [docs/logging/valgrind_workflow.md](docs/logging/valgrind_workflow.md)

---

## Validation Strategy

- `CPU-first` 驗證可以，但不能忽略板端邊界
- 真實結論以：
  - RTSP
  - RV1106 板端
  - RKNN runtime

實測為準

- PC 上 ONNX 可跑，不等於板端可部署
- 多模型常駐可作為合理方向，但必須板端量測驗證
- 目前專案狀態仍應明確表述為：
  - 目標平台是 `RV1106G2 / RV1106G3`
  - 但現階段主體仍是 `PC-first bring-up`

---

## Maintenance Rule

新增決策時請遵守：
- 只記錄高價值結論
- 放到正確功能區塊
- 如果只是暫時 bring-up 現象，改寫到 `bringup_notes.md`
