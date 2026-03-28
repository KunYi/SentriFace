# Logging System Spec

## 1. 目的

這份文件定義第一版產品化 logging system 骨架。

目標不是一開始就把整個專案塞滿重型 log framework，而是先建立：

- 可關閉/可分 level 的 logging 抽象
- `local / stdout / network` 的 backend 方向
- 後續遠端除錯與維護的基礎介面

---

## 2. 設計原則

第一版應保持：

- 預設可關閉
- level 明確
- backend 可切換
- 不依賴大型第三方 logging library

這樣比較符合：

- `RV1106` 板端資源限制
- 早期產品開發的可控性
- 後續遠端維護需求

也就是說，logging 的定位應是：

- 幫助 bring-up、驗證、遠端除錯與維護
- 降低定位問題成本
- 不反過來主導核心產品架構

本專案的主線仍然是：

- detection
- tracking
- embedding
- search
- decision
- access control

logging 只應做到「夠用、可關閉、可分析」，而不是變成主要交付物。

---

## 3. 模組位置

- `include/logging/system_logger.hpp`
- `src/logging/system_logger.cpp`

主要型別：

- `LogLevel`
- `LogBackend`
- `LoggerConfig`
- `LogMessage`
- `LogResult`
- `SystemLogger`

主要 helper：

- `CurrentTimestampMs()`
- `SystemLogger::LogNow(...)`
- `SENTRIFACE_LOGE/W/I/D(...)`
- `SENTRIFACE_LOGE/W/I/D_TAG(...)`

第一版也引入：

- `module/tag` 概念

text backend 目前的原始輸出已偏向 `adb logcat` 風格，並同時保留：

- wall-clock time
- monotonic elapsed time
- level
- module/tag
- message

例如：

```text
03-28 16:07:36.843 +1234ms I/tracker: hello
```

---

## 4. Backend

### 4.1 `dummy`

- 預設安全 backend
- 不實際輸出
- 適合關閉或最小化 logging 成本

### 4.2 `stdout`

- 直接輸出到 stdout
- 適合 sample runner / CLI 驗證

### 4.3 `file`

- 寫入本機檔案
- 適合 early product 本地除錯與 artifact 留存

### 4.4 `file_binary`

- 第二階段主線 backend
- 寫出 `FLOG` binary v1 record
- 交由 dumper/viewer 決定如何轉成人類可讀格式

### 4.5 `network_rpc`

- 預留給遠端 log 收集
- 第一版先保留 framework / stub path

第二階段 binary 方向則另見：

- [binary_logging_pipeline_spec.md](binary_logging_pipeline_spec.md)

---

## 5. Level

第一版 level：

- `off`
- `error`
- `warn`
- `info`
- `debug`

設計原則：

- `off` 完全不輸出
- 只輸出 `<= configured level` 的訊息

---

## 6. Env

目前支援：

- `SENTRIFACE_LOG_ENABLE`
- `SENTRIFACE_LOG_LEVEL=off|error|warn|info|debug`
- `SENTRIFACE_LOG_BACKEND=dummy|stdout|file|file_binary|network_rpc`
- `SENTRIFACE_LOG_COMPRESSION=none|gzip|zstd`
- `SENTRIFACE_LOG_MODULES=tracker,decision,...`
- `SENTRIFACE_LOG_FILE`
- `SENTRIFACE_LOG_RPC_ENDPOINT`

若 `SENTRIFACE_LOG_MODULES` 為空，代表不做 module filter。

目前 `compression` 先視為 capability/config baseline：

- 先在 config 層與 dump tool 層定義
- 目前 `file` / `file_binary` 已有第一版 chunked compression skeleton
- 目前採 appendable compressed chunk / frame 方式落地，重點是先把工作流跑通
- 後續仍可演進成更高效的 streaming compression writer

壓縮策略目前建議：

- `gzip`：第一版 baseline / compatibility option
- `zstd`：後續較優先的產品化方向

原因：

- `gzip` 工具鏈最成熟、現場環境幾乎一定可用
- `zstd` 在速度與壓縮率上通常更有優勢，更適合長時間 log stream

---

## 7. Module / Tag

第一版 logger 保留底層結構化介面：

- `LogMessage`
- `SystemLogger::Log(...)`

但呼叫端的主要建議用法應改成：

- 每個 `.cpp` 檔案只定義一次：

```cpp
#define SENTRIFACE_LOG_TAG "tracker"
```

- 平常使用：

```cpp
SENTRIFACE_LOGI(logger, "tracker_started");
SENTRIFACE_LOGD(logger, "track_id=" << track_id << " score=" << score);
```

- 若在少數 bridge/orchestrator 檔案中，確實需要代替其他 module 發 log，可用：

```cpp
SENTRIFACE_LOGI_TAG(logger, "pipeline", "prototype_update_candidate ...");
```

這樣可以達成：

- `TAG` 不需要每次手寫
- `timestamp` 在進入 logger 時自動補上
- 呼叫端更接近 Android / `adb logcat` 風格

`LogMessage` 內部仍保留：

- `module`

`component` 保留作相容欄位；若 `module` 為空，才會退回使用 `component`。

設計目標是讓後續 log 更接近：

- `tracker`
- `decision`
- `detector`
- `pipeline`
- `access`
- `rtsp`

這種產品維護時較容易理解的 tag。

---

## 8. Timestamp

第一版 macro / `LogNow(...)` 路徑的 timestamp 規則是：

- 由 logger 在寫入前自動產生
- text backend 會同時寫入：
  - wall-clock timestamp
  - process-local monotonic milliseconds
- binary backend 會保留：
  - `wall_sec + wall_msec`
  - `mono_ms`
- 型別維持 `uint32_t`

這樣比較符合：

- `RV1106 32-bit` 型別策略
- 不要求每個呼叫點手動維護 timestamp
- 避免 log 因系統時間調整而倒退

viewer/dumper 應負責決定：

- 要顯示 wall、mono 或兩者
- 要使用 `logcat`、`threadtime`、`json` 或其他格式
- 是否顯示欄位名稱、顏色與過濾結果

而不是讓每個模組自己手動拼裝時間字串。

另外目前 logger / binary record 的時間欄位邊界應明確理解為：

- `mono_ms`
  - 型別：`uint32_t`
  - 用途：同次執行內的相對時間、延遲觀察、事件排序
  - 可以把它理解成「碼表」
  - 邊界：大約 `49.7` 天後回捲
  - 不應當作長期絕對時間單獨使用

- `wall_sec`
  - 型別：`uint32_t`
  - 用途：提供人類可讀 wall-clock 顯示基礎
  - 可以把它理解成「時鐘」
  - 邊界：若以 `uint32` Unix seconds 表示，回捲點約在 `2106-02-07`
  - 因此它本身不是典型的 `2038` signed-32 問題

換句話說：

- `mono_ms` 負責回答「兩個事件相差多久」
- `wall time` 負責回答「事件發生在真實世界的什麼時間」

所以即使 `mono_ms` 在超長 uptime 下回捲，也不代表整體時間線就無法理解，因為：

- 原始 log 仍有 `wall time`
- viewer / backend 仍可依 `wall time` 來串接不同時間段
- 若產品設計允許定期 rotate / restart，也可在接近 `49.7` 天前主動切換 session

實務上可把這理解成：

- 碼表會歸零
- 牆上的時鐘不會因此失去意義
- 兩者一起用，就足夠支撐產品 bring-up、維運與問題追查

需要額外注意的是：

- 本專案 log record 自己使用 `uint32_t wall_sec`，不等於整個系統就自動避開 `2038`
- 若板端 libc / kernel / third-party API 仍使用 signed 32-bit `time_t` 或相容欄位，系統其他交界仍可能存在 `2038` 風險

所以目前產品化建議是：

- `mono_ms` 繼續保留 `uint32_t`
- `wall_sec` 在 v1 維持 `uint32_t`
- 若未來要支援超長期保存、法規稽核或跨年大規模部署，應考慮：
  - binary log v2 引入 `uint64` 等級的絕對時間欄位
  - 或由 host/viewer/export layer 補強長期時間語意

## 9. 產品化方向

這套 logging system 後續可逐步接到：

- `offline_sequence_runner`
- `sample_pipeline_runner`
- `access backend`
- `RTSP / replay workflow`

目前第一個正式接入點已是：

- `offline_sequence_runner`
- `access/unlock_controller`

建議 module/tag 先收斂成：

- `offline_runner`
- `pipeline`
- `access`

建議順序：

1. 先有統一 logger 抽象
2. 再挑少數高價值節點接入
3. 最後才考慮更完整的遠端收集或 rotation 策略

---

## 10. Viewer / Dumper Roadmap

未來建議 workflow：

1. runtime 寫出 text 或 binary log
2. 由 dump/viewer 工具轉成：
   - `brief`
   - `full`
   - `json`
   - colorized terminal output

目前已先建立第一版工具骨架：

- [../tools/logcat_dump.py](../tools/logcat_dump.py)

它目前已能：

- 解析現有 text log
- 解析 `.gz` 壓縮過的 text log
- 解析 `.zst` 壓縮過的 text / binary log
- 預留解析未來 binary log v1
- 支援 level/module filter
- 支援 `grep` / regex message filter
- 支援 `tail N`
- 支援 matched record `count`
- 支援 `stats` summary
- 支援 `stats json`
- 支援常見 message/event key 摘要
- 支援 `--stats-top N` 控制摘要長度
- 支援 text log `follow`
- 支援 field selection
- 支援 ANSI color 輸出
- 支援 `logcat` / `threadtime` / `json` 等 viewer format
- `stats` 目前也包含 `first/last wall time`、`first/last mono ms`、以及常見事件名稱分布

目前也新增了簡單的壓縮工具：

- [../tools/log_archive.sh](../tools/log_archive.sh)

可用於把既有 `.log` / `.flog` artifact 壓成：

- `.gz`
- `.zst`

viewer 使用示例：

```bash
tools/logcat_dump.py build/offline_sequence_runner_v2.flog \
  --input-format binary \
  --output-format logcat
```

```bash
tools/logcat_dump.py build/offline_sequence_runner_v2.flog.zst \
  --input-format binary \
  --output-format threadtime \
  --color always
```

```bash
tools/logcat_dump.py build/offline_sequence_runner_v2.flog \
  --input-format binary \
  --grep unlock \
  --tail 5
```

```bash
tools/logcat_dump.py build/offline_sequence_runner_v2.flog \
  --input-format binary \
  --modules access,pipeline \
  --count
```

```bash
tools/logcat_dump.py build/offline_sequence_runner_v2.flog \
  --input-format binary \
  --stats
```

```bash
tools/logcat_dump.py build/offline_sequence_runner_v2.flog \
  --input-format binary \
  --stats \
  --stats-format json
```

```bash
tools/logcat_dump.py build/offline_sequence_runner_v2.flog \
  --input-format binary \
  --stats \
  --stats-top 5
```

```bash
tools/logcat_dump.py build/offline_sequence_runner_v2.log \
  --follow \
  --output-format logcat
```

另外目前也新增一個簡單的收集工具：

- [../tools/log_collect.sh](../tools/log_collect.sh)

可用於把多個 log / artifact 複製到同一個輸出目錄並產生 `manifest.txt`：

```bash
tools/log_collect.sh artifacts/log_bundle \
  build/offline_sequence_runner_v2.log \
  build/offline_sequence_runner_v2.flog.zst \
  build/offline_sequence_report.txt
```

另外目前也新增一個單一入口檢視工具：

- [../tools/log_inspect.sh](../tools/log_inspect.sh)

可一次輸出：

- `stats.txt`
- `stats.json`
- `logcat.txt`
- `threadtime.txt`
- `bundle/manifest.txt`

示例：

```bash
tools/log_inspect.sh build/offline_sequence_runner_v2.flog.zst
```
