# AGENTS.md

## Project

本專案目前的文件主名稱定為 `SentriFace`。

它是一套基於 `Luckfox Pico RV1106G2 / RV1106G3` 的人臉辨識門禁系統。

產品方向：
- 固定式、近距離、單入口門禁
- `SC3336 3MP`，直立安裝
- detection 主線採 `SCRFD-500M`
- tracking 主線採 `IoU + Kalman Filter`
- local-first，優先支援 `200 persons` 驗證，目標 `200 ~ 500 persons`

---

## Read Order

後續代理或開發者請先讀：

1. [README.md](README.md)
2. [AGENTS.md](AGENTS.md)
3. [docs/project/decision_log.md](docs/project/decision_log.md)
4. 依任務**按需要載入**相關 spec

不要一次把全部 `docs/` 讀進 context。
原則是：
- 只讀目前要改的模組
- 只讀直接相關的 guide / spec
- 歷史決策優先看 `decision_log.md`
- bring-up 或已知問題優先看 `bringup_notes.md`

---

## Core Rules

- 開發流程預設採 `SDD + TDD`：
  - `Spec-Driven Development`
  - `Test-Driven Development`
- 若新增或調整正式邊界：
  - 先補或先更新 spec
  - 再補對應測試
  - 最後再落實作
- 能補測試就先補測試
- 若設計明顯偏離既有基線，先更新文件
- `CPU-first` 驗證可以，但設計邊界仍必須服從 `RV1106 + RKNN`
- 新程式碼預設對 `32-bit RV1106` 友善：
  - 能用 `int32/uint32` 就不要先用 `int64`
  - 能用整數就不要先用浮點
  - 必須用浮點時優先 `float`
- 幾何/演算法層可用 `float`，pixel / memory / hardware boundary 要回到整數
- logging 是輔助系統，不是主線交付物；做到可關閉、可分析、可維護即可

---

## Repository Layout

- [docs](docs/): 規格、設計、guide、decision log
- [include](include/): 模組公開介面
- [src](src/): 核心實作
- [tests](tests/): 單元與整合測試
- [tools](tools/): build / replay / validation 輔助工具
- [host](host/): host-side tooling，例如 Qt enrollment app

---

## Frozen Baseline

目前凍結基線如下：

- camera / sensor：`SC3336`
- orientation：直立模式
- detector：`SCRFD-500M`
- tracker：`IoU + Kalman Filter`
- align：`SCRFD 5 landmarks`
- detector 預設先過濾小臉：
  - `min_face_width = 120`
  - `min_face_height = 120`
- detector 可優先大臉排序，必要時限制輸出數量
- detector 可使用 ROI 排除非門禁有效區域
- embedding 是 event-driven identity signal，不是每幀 tracking
- 第一版產品延遲目標：`2 秒內` 得到穩定門禁結果
- local-first 容量基線：
  - 驗證：`200 persons`
  - 目標：`200 ~ 500 persons`

更完整取捨請看：
- [docs/project/decision_log.md](docs/project/decision_log.md)

---

## Key Specs By Topic

### Hardware / platform
- [docs/platform/rv1106_system_hardware_baseline.md](docs/platform/rv1106_system_hardware_baseline.md)
- [docs/platform/rv1106_numeric_type_policy.md](docs/platform/rv1106_numeric_type_policy.md)
- [docs/platform/geometry_numeric_boundary.md](docs/platform/geometry_numeric_boundary.md)

### Detection / tracking
- [docs/detector/detector_module_interface_spec.md](docs/detector/detector_module_interface_spec.md)
- [docs/detector/scrfd_detector_backend_spec.md](docs/detector/scrfd_detector_backend_spec.md)
- [docs/tracker/tracker_module_interface_spec.md](docs/tracker/tracker_module_interface_spec.md)
- [docs/tracker/relink_parameter_sweep_guide.md](docs/tracker/relink_parameter_sweep_guide.md)

### Alignment / embedding / search
- [docs/embedding/alignment_module_interface_spec.md](docs/embedding/alignment_module_interface_spec.md)
- [docs/embedding/embedding_module_interface_spec.md](docs/embedding/embedding_module_interface_spec.md)
- [docs/embedding/embedding_assisted_relink_design.md](docs/embedding/embedding_assisted_relink_design.md)
- [docs/embedding/baseline_embedding_manifest_workflow.md](docs/embedding/baseline_embedding_manifest_workflow.md)
- [docs/search/embedding_search_intuition.md](docs/search/embedding_search_intuition.md)
- [docs/search/search_module_interface_spec.md](docs/search/search_module_interface_spec.md)
- [docs/search/search_v2_interface_spec.md](docs/search/search_v2_interface_spec.md)
- [docs/search/weighted_search_spec.md](docs/search/weighted_search_spec.md)

### Enrollment
- [docs/enrollment/enrollment_module_interface_spec.md](docs/enrollment/enrollment_module_interface_spec.md)
- [docs/enrollment/enrollment_store_v2_spec.md](docs/enrollment/enrollment_store_v2_spec.md)
- [docs/enrollment/adaptive_prototype_policy.md](docs/enrollment/adaptive_prototype_policy.md)
- [docs/enrollment/enrollment_artifact_runner_guide.md](docs/enrollment/enrollment_artifact_runner_guide.md)
- [docs/enrollment/baseline_embedding_import_workflow.md](docs/enrollment/baseline_embedding_import_workflow.md)
- [docs/enrollment/enrollment_baseline_import_runner_guide.md](docs/enrollment/enrollment_baseline_import_runner_guide.md)
- [docs/enrollment/enrollment_baseline_verify_runner_guide.md](docs/enrollment/enrollment_baseline_verify_runner_guide.md)

### Replay / validation
- [docs/pipeline/offline_sequence_runner_spec.md](docs/pipeline/offline_sequence_runner_spec.md)
- [docs/pipeline/offline_sequence_runner_guide.md](docs/pipeline/offline_sequence_runner_guide.md)
- [docs/pipeline/offline_detection_replay_spec.md](docs/pipeline/offline_detection_replay_spec.md)
- [docs/pipeline/offline_embedding_replay_spec.md](docs/pipeline/offline_embedding_replay_spec.md)
- [docs/pipeline/rtsp_validation_milestone.md](docs/pipeline/rtsp_validation_milestone.md)

### Host enrollment tool
- [docs/host/host_qt_enroll_tool_spec.md](docs/host/host_qt_enroll_tool_spec.md)
- [docs/host/host_qt_enroll_state_machine.md](docs/host/host_qt_enroll_state_machine.md)
- [docs/host/host_qt_enroll_webcam_guide.md](docs/host/host_qt_enroll_webcam_guide.md)
- [docs/host/host_qt_enroll_detector_integration_spec.md](docs/host/host_qt_enroll_detector_integration_spec.md)

### Access / logging / diagnostics
- [docs/access/access_control_backend_spec.md](docs/access/access_control_backend_spec.md)
- [docs/access/rgb_liveness_anti_spoof_spec.md](docs/access/rgb_liveness_anti_spoof_spec.md)
- [docs/logging/logging_system_spec.md](docs/logging/logging_system_spec.md)
- [docs/logging/binary_logging_pipeline_spec.md](docs/logging/binary_logging_pipeline_spec.md)
- [docs/logging/valgrind_workflow.md](docs/logging/valgrind_workflow.md)

---

## Current Mainline

目前主線優先順序：

1. host enrollment artifact
2. artifact import / baseline plan
3. baseline generation 邊界
4. 真實 align/embed runtime 接入
5. local-first enrollment / search / decision 收斂

已打通的 bring-up 路徑：

- host Qt enroll tool 可輸出：
  - `enrollment_summary.txt`
  - accepted frame
  - face crop
- `enrollment_artifact_import` 可讀回 `EnrollmentArtifactPackage`
- `BuildBaselineEnrollmentPlan(...)` 可做 baseline sample selection
- `GenerateBaselinePrototypePackage(...)` 已是正式 baseline generation dispatch 邊界
- `GenerateMockBaselinePrototypePackage(...)` 可打通 artifact -> baseline package -> store
- `enrollment_artifact_runner` 可直接驗證上述資料流
- `baseline_embedding_input_manifest` 已可作為 host artifact 接真實 embedding workflow 的橋接產物
- `LoadBaselinePrototypePackage(...)` 與 `.sfbp -> .sfsi` 已是正式主線；`LoadBaselinePrototypePackageFromEmbeddingCsv(...)` 只保留作真實 baseline embedding CSV 的互通導入邊界

注意：
- `GenerateMockBaselinePrototypePackage(...)` 只是 bring-up 骨架，不是最終真實 embedding runtime
- face crop 在真實 video stream 下仍需持續驗證，不應過早視為完全正確
- host enrollment 在 `WebCAM-first` bring-up 階段允許暫時放寬 guidance ready 條件，
  但必須同步寫入 `docs/project/bringup_notes.md`，後續再配合 `RTSP / RV1106` 實測重調

---

## Validation Order

驗證順序固定如下：

1. 本機 `WebCAM`
2. `RTSP`
3. `RV1106G2 / RV1106G3` 實機

不要在 host enrollment 早期階段，把：
- UI / guidance
- RTSP 傳輸
- 板端部署

三種問題混在一起查。

---

## Decision / Bring-up Logs

歷史決策與取捨：
- [docs/project/decision_log.md](docs/project/decision_log.md)

bring-up 現象、已知問題、非阻塞風險：
- [docs/project/bringup_notes.md](docs/project/bringup_notes.md)

這兩份文件應持續維護，但都不應無限制膨脹。
新增內容時請：
- 只記錄高價值結論
- 同類問題寫進對應區塊
- 避免重複貼整段歷史

---

## Working Rules For Future Agents

- 先讀 `AGENTS.md`
- 再讀 `decision_log.md`
- 然後只載入目前任務相關的 spec
- 不要把整個 `docs/` 一次塞進 context
- 若有新基線決策：
  - 先更新 `decision_log.md`
  - 再視需要更新模組 spec
  - 只有真正需要長期保留的基線，才回寫到 `AGENTS.md`
