# Access Control Backend Spec

## 1. 目的

這份文件定義第一版 `access control backend` / `unlock backend` 介面。

第一版目標不是直接控制真實門鎖，而是先提供：

- 可編譯的 unlock controller 骨架
- `dummy / stdout / local GPIO / UART / network RPC` backend 選擇
- 讓整體流程在沒有硬體時也能驗證

---

## 2. 設計原則

本專案是產品開發，不應把 unlock 控制寫死在單一本機 GPIO 實作中。

因此第一版策略是：

- 預設 backend 為 `dummy`
- 若環境指定，改用 `stdout`
- 本機 GPIO 路線保留給 `libgpiod`
- 遠端裝置控制可預留給 `UART / network RPC`

也就是：

- 沒硬體時，仍能驗證 unlock 流程
- 有本機 GPIO 時，可切到 `libgpiod`
- 若門鎖控制器在外部裝置，也可切到 `UART / network RPC`

---

## 3. 目前介面

目前模組已從 `gpio/` 重命名到更貼近產品語意的 `access/`。

模組位置：

- `include/access/unlock_controller.hpp`
- `src/access/unlock_controller.cpp`

主要型別：

- `UnlockBackend`
- `UnlockControllerConfig`
- `UnlockEvent`
- `UnlockResult`
- `UnlockController`

---

## 4. Backend 定義

### 4.1 `dummy`

- 不實際輸出 GPIO
- 只回報 unlock 已觸發
- 適合單元測試與流程驗證

### 4.2 `stdout`

- 不實際輸出 GPIO
- 會把 unlock 事件印到 stdout
- 適合 sample runner / CLI 流程驗證

### 4.3 `libgpiod`

- 預留給實機 GPIO 控制
- 目前仍是框架階段
- 若沒有啟用對應支援，會回報 unavailable

### 4.4 `uart`

- 預留給透過 serial/UART 對外部控制器送 unlock 命令
- 第一版先保留框架與 config
- 目前以 mock-success 方式支援流程驗證

### 4.5 `network_rpc`

- 預留給透過 TCP/HTTP/RPC 方式控制遠端裝置
- 第一版先保留框架與 config
- 目前以 mock-success 方式支援流程驗證

---

## 5. Env 規則

目前支援：

- `SENTRIFACE_UNLOCK_BACKEND`
  - `dummy`
  - `stdout`
  - `gpiod`
  - `uart`
  - `network_rpc`
- `SENTRIFACE_UNLOCK_PULSE_MS`
- `SENTRIFACE_UNLOCK_COOLDOWN_MS`
- `SENTRIFACE_UNLOCK_UART_DEVICE`
- `SENTRIFACE_UNLOCK_UART_BAUD`
- `SENTRIFACE_UNLOCK_RPC_ENDPOINT`

相容保留：

- `SENTRIFACE_GPIO_BACKEND`
  - `dummy`
  - `stdout`
  - `gpiod`
- `SENTRIFACE_GPIO_LINE_OFFSET`
- `SENTRIFACE_GPIO_PULSE_MS`
- `SENTRIFACE_GPIO_COOLDOWN_MS`
- `SENTRIFACE_GPIO_ACTIVE_LOW`
- `SENTRIFACE_GPIO_CHIP_PATH`
- `SENTRIFACE_GPIO_LINE_NAME`

---

## 6. Sample Runner 整合

目前這兩條驗證線都已整合 `UnlockController`：

- `sample_pipeline_runner`
- `offline_sequence_runner`

預設行為：

- backend 預設為 `dummy`
- 若 env 指定 `stdout`，則會印出 unlock 訊息
- 後續也可用相同介面切到 `uart / network_rpc`

也就是：

- 不接實體門鎖控制硬體，也可以確認：
  - `unlock_ready_tracks`
  - `unlock_actions`
  - cooldown 是否生效

---

## 7. 產品化方向

後續若接真實硬體或遠端裝置，建議順序如下：

1. 先保留 `dummy / stdout` workflow
2. 再依產品需要選擇補：
   - `libgpiod`
   - `UART`
   - `network RPC`
3. 再決定是否需要把 unlock backend 配置做成檔案化

不要一開始就把：

- 本機 GPIO
- 遠端裝置通訊協定
- 門鎖 pulse 策略
- fail-safe 策略

全部綁進 sample runner。
