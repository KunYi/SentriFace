# Binary Logging Pipeline Spec

## 1. 目的

這份文件定義 logging system 的第二階段產品化方向。

第一階段已完成：

- `TAG/module`
- `level`
- `stdout / file / network_rpc`
- `TAG + macro + auto timestamp`

第二階段目標則是：

- log 寫入格式改為更適合機器處理的 binary record
- 再由 dump/tool 轉成不同的人類可讀格式
- 讓遠端除錯、離線分析、欄位裁切、顏色增強都由 viewer 決定

目前專案中，`file_binary` backend 已開始作為 binary v1 writer skeleton 落地。

這會更接近：

- Android `logcat`
- embedded product 的 ring-buffer logging
- 現場維護與回報 artifact 的實務需求

---

## 2. 設計原則

第二階段應遵守：

- logger 寫入成本低
- record 格式固定、容易 append
- 以 `32-bit RV1106` 為前提
- 避免複雜動態配置
- viewer 決定如何格式化，而不是 logger 在寫入時就把所有表現層決策綁死

也就是說：

- writer 偏向 `binary event stream`
- viewer 偏向 `text / color / json / field selection`

---

## 3. 架構分層

建議分成三層：

### 3.1 Writer

由板端或程式執行期寫出 binary records。

未來建議 backend 類型：

- `dummy`
- `stdout_text`
- `file_text`
- `file_binary`
- `network_rpc`

其中：

- `file_binary` 是第二階段的主線
- `stdout_text` 與 `file_text` 可保留給早期 bring-up

### 3.2 Storage

log 寫入位置可為：

- 單檔 append
- rolling files
- ring buffer
- RAM queue + flush thread

第一版 binary storage 建議先從：

- 單檔 append

開始，避免過早引入 rotation 複雜度。

### 3.3 Viewer / Dumper

由工具把 binary log 轉成人類可讀輸出。

viewer 應支援：

- format selection
- field selection
- level filter
- module filter
- grep / regex message filter
- tail / count style quick inspection
- stats summary
- json stats summary
- first/last time summary
- top message/event key summary
- top-N summary limit
- color output
- json export

---

## 4. 建議 Binary Record

第一版建議採固定 header + variable payload：

### 4.1 File Header

```text
magic      : 4 bytes = "FLOG"
version    : uint16  = 1
reserved   : uint16
```

### 4.2 Record Header

```text
mono_ms       : uint32
wall_sec      : uint32
wall_msec     : uint16
level         : uint8
flags         : uint8
module_len    : uint16
message_len   : uint16
reserved      : uint16
```

接著 payload：

```text
module bytes
message bytes
```

字串編碼預設：

- UTF-8

這樣的好處：

- record parsing 簡單
- `uint32` 為主，符合 `RV1106` 型別策略
- writer 實作容易
- dumper 可直接逐筆走訪

---

## 5. Timestamp 策略

binary record 建議同時保留兩種時間：

- `mono_ms`
  用來做事件排序、延遲分析、跨模組相對時間比較
- `wall_sec + wall_msec`
  用來做人類可讀時間顯示

原因：

- monotonic time 不會倒退
- wall-clock 比較適合現場人員閱讀
- viewer 可以自行決定只顯示其中一種或兩種都顯示

### 5.1 v1 時間邊界

目前 v1 record 以 `32-bit` 為主，需明確理解：

- `mono_ms : uint32`
  - 約 `49.7` 天後回捲
  - 適合單次執行內排序與延遲分析
  - 可以把它理解成「碼表」
  - 不適合當長期絕對時間單獨使用

- `wall_sec : uint32`
  - 若以 unsigned Unix seconds 表示，回捲時間約在 `2106-02-07`
  - 因此它本身不是典型 `2038` signed-32 問題
  - 可以把它理解成「時鐘」

比較白話的理解方式是：

- `mono_ms` 用來看「這兩件事隔了多久」
- `wall time` 用來看「這件事發生在幾點幾分」

因此即使 `mono_ms` 回捲，也不等於 log 整體不可用。只要：

- record 仍保有 `wall_sec + wall_msec`
- viewer 能依 wall time 顯示與排序
- 系統在超長 uptime 場景下有 rotate / restart / session 切換策略

就能把這件事當成可管理的產品邊界，而不是立即性的設計錯誤

但仍需注意：

- 系統其他模組若經過 signed 32-bit `time_t` 或外部 API 轉換，仍可能產生 `2038` 風險

因此 v1 的定位是：

- 優先支援板端 32-bit、低成本、可分析的產品 log
- 長期保存或法規稽核等需求，應在 v2 再考慮 `64-bit` 絕對時間欄位

---

## 6. Viewer 輸出格式

viewer 建議至少支援：

### 6.1 `brief`

```text
03-28 23:41:12.123 I/offline_runner: started
```

### 6.2 `elapsed`

```text
+1066ms I/access: unlock_event ...
```

### 6.3 `full`

```text
03-28 23:41:12.123 +1066ms I/access: unlock_event track_id=0 ...
```

### 6.4 `json`

```json
{"wall":"03-28 23:41:12.123","mono_ms":1066,"level":"I","module":"access","message":"unlock_event ..."}
```

---

## 7. Field Selection

viewer 應支援可選欄位，例如：

- `wall`
- `mono`
- `level`
- `module`
- `message`

例如：

```bash
tools/logcat_dump.py --fields wall,level,module,message ...
```

這樣之後：

- 現場維護可看簡短輸出
- 開發者可看完整時間與全部欄位
- CI/工具可吃 JSON

---

## 8. Color Output

viewer 應支援：

- `--color auto`
- `--color always`
- `--color never`

在 `xterm-256color` 或相容終端下，建議：

- `E`：紅色
- `W`：黃色
- `I`：綠色或青色
- `D`：灰色或藍色

注意：

- 顏色應只由 viewer 加上
- binary log 本體不應帶 ANSI escape sequence

---

## 9. Compression

第二階段應保留 log stream 可壓縮的能力。

建議原則：

- 壓縮是 transport/storage concern，不是 message formatting concern
- writer 可選擇直接輸出壓縮 stream，或由後處理工具壓縮
- viewer/dumper 應能透明讀取 `.gz` / `.zst`

第一版 config / spec 建議先支援：

- `none`
- `gzip`
- `zstd`

原因：

- `gzip` 生態普及
- 現場與 CI artifact 都容易處理
- Python / shell / 常見 Linux 工具鏈支援完整
- `zstd` 通常在速度與壓縮率上更佳，適合作為長期主線目標

產品化建議排序：

1. `gzip`
   作為第一版 baseline / compatibility option
2. `zstd`
   作為後續較優先的性能升級方向

也就是：

- 起步先以 `gzip` 降低導入風險
- 架構與 config 不要把路線鎖死
- 真正做 binary writer 時，再視板端依賴與體積評估是否直接走 `zstd`

目前工具鏈狀態：

- writer: `file_binary` 已可寫出 `FLOG` v1，且已有第一版 appendable gzip/zstd chunk skeleton
- viewer: `tools/logcat_dump.py` 已可讀 `.gz` / `.zst`
- archive: `tools/log_archive.sh` 可把既有 log artifact 壓成 `.gz` / `.zst`

---

## 10. 導入順序

建議順序：

1. 保持現有 text logging 可用
2. 補 binary log format spec
3. 補 viewer/dumper tool
4. 再新增 `file_binary` backend
5. 最後才考慮 ring buffer / rotation / remote streaming

這樣可以避免：

- 目前產品化測試路徑被一次性打壞
- 早期把 logger backend 與 viewer 耦合得太死

---

## 11. 與目前專案的關係

目前專案狀態：

- text logger 已可用
- `offline_sequence_runner`
- `access/unlock_controller`

已經接入 logger

下一階段不應先把 text backend 移除，而應採：

- text backend 保留
- binary backend 追加
- dumper tool 獨立演進

這樣最符合早期產品開發與後續維護雙方需求。
