# RV1106 + Luckfox 人臉門禁系統基礎硬體規格

## 1. 文件目的

這份文件定義本專案的基礎硬體規格與部署假設，作為後續文件的共同前提，包含：

- 開發板基線
- SoC 與型號差異
- 攝像頭模組
- 安裝方向與機構假設
- 供電、網路、儲存、GPIO 與門鎖控制需求
- 對後續 `tracking / embedding / DB search / GPIO unlock` 的直接限制

這份文件刻意分成兩部分：

1. **官方已知硬體事實**
2. **本專案的系統設計假設**

這樣後面在寫 `rv1106_kf_tracking_spec.md` 時，所有人都知道哪些條件是固定的，哪些只是目前版本的工程選擇。

---

## 2. 系統定位

本專案目標是一套基於 `Luckfox Pico RV1106` 平台的人臉辨識門禁系統，具備以下特性：

- 固定鏡頭
- 固定安裝位置
- 單門口近距離辨識
- 以直立式門邊安裝為主
- 本地完成 camera / detection / tracking / alignment / embedding
- 門禁決策可本地完成，或與上位機配合完成

系統不是通用監控攝影機，而是：

- **近距離、單入口、保守開門決策** 的嵌入式視覺設備

---

## 3. 開發板基線

### 3.1 板型基線

目前基線開發平台定義為：

- `Luckfox Pico Pro` 對應 `RV1106G2`
- `Luckfox Pico Max` 對應 `RV1106G3`

兩者都是 `Luckfox Pico RV1106` 系列中的標準開發板，具備：

- `Cortex-A7 1.2 GHz`
- `MIPI CSI 2-lane`
- 內建 ISP
- 10/100M Ethernet
- USB 2.0 Host/Device
- GPIO

### 3.2 版本差異

| 項目 | Luckfox Pico Pro | Luckfox Pico Max |
|---|---:|---:|
| SoC | RV1106G2 | RV1106G3 |
| NPU | 0.5 TOPS | 1 TOPS |
| DDR | 128MB DDR3L | 256MB DDR3L |
| 預設儲存 | SPI NAND 256MB | SPI NAND 256MB |
| Camera | MIPI CSI 2-lane | MIPI CSI 2-lane |
| Ethernet | 10/100M | 10/100M |

### 3.3 專案建議主力版本

如果只選一個主力版本做正式門禁產品驗證，建議優先：

- `Luckfox Pico Max / RV1106G3`

原因：

- NPU 餘裕較大
- DDR 容量較大
- 更適合同時執行 detection、embedding、smoothing 與大庫查詢節流

`RV1106G2` 仍可作為：

- bring-up 平台
- 輕量版本
- 僅本地小型資料庫版本

---

## 4. SoC 與系統能力定義

### 4.1 RV1106 能力摘要

本專案以 `Rockchip RV1106` 為核心視覺 SoC，關鍵能力如下：

- 單核 `Arm Cortex-A7`
- 內建 NPU
- 內建 ISP
- 支援 MIPI CSI 視訊輸入
- ISP 最大輸入能力為 `5MP @ 30fps`
- 適合邊緣視覺 AI 任務

### 4.2 本專案對 RV1106 的能力切分

在本專案中，RV1106 預期負責：

- camera capture
- ISP image pipeline
- face detection
- face tracking
- face alignment
- embedding inference
- temporal smoothing
- GPIO 門鎖控制

若資料庫規模達上萬人，則建議：

- RV1106 繼續負責感知前端
- 大規模 DB search 優先交由上位機或伺服器

---

## 5. 攝像頭基線

### 5.1 官方相容鏡頭

依 Luckfox 官方 CSI Camera 文件，RV1106 板型相容的官方常見鏡頭包括：

- `SC3336 3MP Camera (A)`
- `MIS5001 5MP Camera`

其中：

- `SC3336` 側重 3MP 成像與低照度表現
- `MIS5001` 支援 5MP，並有廣角與廣角無畸變版本

### 5.2 專案建議鏡頭優先順序

目前本專案已確認開發基線鏡頭為：

- `SC3336 3MP Camera`

`MIS5001 5MP` 保留作為未來可能的替代方案，但不是目前基線。

原因如下。

#### SC3336 作為目前基線的原因

- 解析度已足夠支撐門口近距離人臉偵測
- 低光表現較適合室內門口
- 對整體頻寬、記憶體、ISP 與處理延遲壓力較低

#### MIS5001 保留價值

- 更高解析度
- 若需要較大視野或較高裁切彈性時有優勢

#### MIS5001 代價

- 對 ISP、記憶體與資料搬移壓力較大
- 在 RV1106G2 上特別需要更保守的系統設計

### 5.3 SC3336 基線含義

既然目前已確認使用 `SC3336`，後續系統文件都應視其為固定前提，這代表：

- 第一版 image tuning 以 `SC3336` 畫面特性為主
- 人臉最小像素門檻以 `SC3336` 視角與安裝距離實測調整
- detection / tracking / alignment 參數先以 `SC3336` 的成像結果做校準
- 後續若改用 `MIS5001`，應視為硬體變更而非單純參數切換

### 5.4 鏡頭介面定義

本專案相機介面定義如下：

- 介面：`MIPI CSI 2-lane`
- 鏡頭排線：依 Luckfox 官方安裝方向連接
- Bring-up 系統：優先 `buildroot`

### 5.5 鏡頭安裝方向

依 Luckfox 官方說明，CSI 排線方向需按照板端規範安裝。  
本專案要求在硬體 bring-up 階段確認：

- 上電前完成排線方向檢查
- 成功辨識鏡頭後確認視頻節點與相機設定檔存在
- 確保量產裝配時排線方向不可裝反

---

## 6. 影像與顯示方向基線

### 6.1 系統安裝方向

本專案預設為：

- **直立模式安裝**

也就是設備安裝在門框、牆面或門側立柱時，長邊朝垂直方向。

### 6.2 直立模式的工程含義

這不是 UI 偏好而已，而是整個視覺系統前提之一。
它會影響：

- camera sensor 安裝方向
- 畫面旋轉設定
- 偵測輸入尺寸
- ROI 規劃
- tracking 運動模型
- face box 最小尺寸

### 6.3 影像座標定義

本專案基線採用：

- 演算法處理時，最終輸入畫面定義為 **直立座標系**

換句話說，即使 sensor 原始輸出是橫向，系統也要在 pipeline 前段完成：

- 旋轉
- 或以等價方式重新定義座標

讓後續 detection / tracking / ROI 使用統一的直立座標。

### 6.4 解析度基線

第一版建議基線為：

- camera capture：`640x480` 或 `720p`
- detection pipeline：依模型需求 resize
- alignment crop：`112x112`

如果走直立模式，可進一步定義為：

- `480x640`
- 或 `720x1280` 的直立處理邏輯

但在 RV1106 第一版，仍建議優先控制在較低解析度以降低 latency。

---

## 7. 門禁場景機構假設

### 7.1 使用場景

本系統假設安裝於：

- 室內門禁
- 半戶外入口
- 工廠或辦公室單一出入口

### 7.2 鏡頭視角假設

基線目標不是看大片場景，而是看「靠近門口、準備通行的人臉」。
因此推薦：

- 中等視角
- 避免過度廣角
- 避免明顯桶狀畸變

### 7.3 安裝高度假設

第一版可先以下列範圍為設計基線：

- 鏡頭中心高度：約 `1.35m ~ 1.55m`

這是門禁很常見的可用高度區間，足以兼顧多數成人臉部入鏡。

### 7.4 最佳辨識距離假設

第一版系統應優先針對：

- 約 `0.4m ~ 1.0m`

的人臉辨識距離進行優化。

這樣可以讓：

- 人臉像素數更穩定
- detection 門檻更好調
- tracking easier
- alignment 品質更高

### 7.5 ROI 假設

門禁系統可以明確定義有效區域，例如畫面中間偏上到中間區域，避免：

- 遠距背景人臉
- 側邊路人
- 非操作人員進入辨識流程

因此硬體規格文件中將 ROI 視為系統層固定條件，而不是之後才臨時補的演算法技巧。

---

## 8. 供電與啟動基線

### 8.1 開發階段

開發階段可接受：

- USB 供電
- 桌面測試模式

### 8.2 產品化階段

正式門禁設備應定義為：

- 穩定 DC 供電
- 與門鎖供電隔離
- 系統電源與鎖控電源避免直接共地噪聲干擾

### 8.3 啟動要求

本系統屬於門禁設備，因此啟動要求包含：

- 上電自動啟動 camera pipeline
- 自動啟動 detection / tracking / recognition 服務
- GPIO 預設為鎖定安全態

也就是：

- **系統未完全 ready 前，不得輸出 unlock**

---

## 9. 儲存與記錄基線

### 9.1 本地儲存角色

本專案中的本地儲存主要用於：

- 韌體與 rootfs
- 模型檔
- 設定檔
- 少量 log
- enrollment cache 或暫存資料

### 9.2 不建議的做法

在 `Pico Pro/Max` 基線下，不建議：

- 把大量歷史影像長期寫進本地 flash
- 把大型向量資料庫完整長期依賴在小容量本地儲存上

### 9.3 大型資料庫條件

若系統目標為 `上萬人資料庫`，建議：

- 本地僅保存必要索引、快取或精簡版本
- 完整資料庫放上位機、NAS 或伺服器

---

## 10. 網路基線

### 10.1 預設網路模式

本專案基線優先定義為：

- `Ethernet 10/100M`

原因：

- 門禁設備固定安裝
- 有線網路穩定
- 較適合與後端資料庫或管理系統串接

### 10.2 Wi-Fi 角色

若後續改用 Ultra / Pi 等帶 Wi-Fi 型號，可作為：

- 測試便利功能
- 備援連線

但不是第一版門禁主通訊路徑。

### 10.3 與後端的分工

當資料庫規模大時，網路需求會提升，因為可能需要：

- 上傳 embedding
- 回傳 top-K 搜尋結果
- 同步人員資料與設定
- 上報事件記錄

因此網路基線不是附屬功能，而是大型資料庫門禁架構的一部分。

### 10.4 RTSP 驗證路徑

若目前 `RV1106G3` 已可提供 `RTSP video stream`，則可把它作為早期實機驗證路徑：

- 開發端使用 `ffmpeg` 抓流
- 驗證畫面方向、解析度、幀率與穩定性
- 收集真實資料供後續離線演算法測試

這條路徑可先存在，不必等待全部板端 pipeline 完成後才啟動。

---

## 11. GPIO 與門鎖控制基線

### 11.1 GPIO 的角色

RV1106 板上的 GPIO 在本專案中主要用於：

- unlock relay 控制
- 門磁或按鈕輸入
- 狀態 LED
- 蜂鳴器或警示輸出

### 11.2 門鎖控制原則

Pico Pro/Max 本身不是完整門禁控制器，因此正式系統應使用：

- **外接隔離式 relay / MOSFET driver / opto-isolated lock control board**

而不是直接讓 SoC GPIO 去承受門鎖負載。

### 11.3 安全定義

門鎖輸出須定義：

- 上電預設關閉
- 程式崩潰時維持安全態
- unlock 脈衝時間固定
- 需要 cooldown 與重入保護

### 11.4 建議門禁相關 I/O

第一版建議預留：

- `1 x lock control output`
- `1 x door sensor input`
- `1 x exit button input`
- `1 x status LED`
- `1 x buzzer output`

---

## 12. 音訊與活體擴充

### 12.1 第一版是否需要音訊

若目前主目標是人臉門禁，音訊不是第一版必要條件。

### 12.2 後續擴充方向

若未來要加：

- 語音提示
- 雙向對講
- 音訊輔助活體

可再考慮帶音訊介面的板型。

### 12.3 活體偵測的硬體含義

若未來要上較強的 liveness，硬體可能需要重新評估：

- 紅外補光
- IR camera
- 雙目
- 更受控的照明

因此第一版硬體規格先定義為：

- **RGB 單鏡頭門禁系統**

---

## 13. 作業系統與軟體基線

### 13.1 作業系統基線

依 Luckfox 官方 CSI Camera 文件，帶相機的這條使用路徑優先採用：

- `buildroot`

因此本專案基線也定義為：

- 第一版 camera + AI pipeline 以 `buildroot` 為主

### 13.2 軟體堆疊角色

本專案預期軟體模組包含：

- camera capture
- detection service
- tracking service
- face alignment
- embedding inference
- DB search client / local index
- unlock controller
- watchdog / health monitor

### 13.3 CPU-first 演算法驗證策略

雖然最終部署目標是 `RV1106 + Luckfox + SC3336`，  
本專案開發流程允許且建議先在 `Intel CPU` 環境完成：

- 演算法驗證
- 模組介面穩定化
- 測試案例建立

之後再遷移到：

- `RKNN`
- 或其他實際板端推論後端

這樣能降低上板初期的除錯成本，並把「演算法問題」與「後端整合問題」拆開處理。

### 13.4 CPU-first 不等於忽略板端限制

雖然可以先在桌面 CPU 驗證，但設計決策仍應服從：

- `RV1106G2 / RV1106G3` 的 NPU/DDR 限制
- `RKNN` 可承受的模型與頻率
- 門禁系統的即時性與保守決策需求

因此在 CPU 上進行驗證時，也應優先避免：

- 過大的 embedding 向量
- 過高的 embedding 觸發頻率
- 過多同時追蹤目標
- 高成本、桌面專用的搜尋策略

---

## 14. 系統硬體基線總結

### 14.1 本專案基礎硬體定義

- 開發板：`Luckfox Pico Pro (RV1106G2)` 或 `Luckfox Pico Max (RV1106G3)`
- 主力建議：`Pico Max / RV1106G3`
- 攝像頭：目前確認為 `SC3336 3MP`
- 相機介面：`MIPI CSI 2-lane`
- 作業系統基線：`buildroot`
- 安裝方向：**直立模式**
- 網路基線：`Ethernet`
- 鎖控：外接隔離式 relay / driver board
- 感測模式：`RGB 單鏡頭`
- 使用場景：固定式近距離人臉門禁

### 14.2 這份基線對後續文件的影響

這份硬體定義會直接影響：

- detection input size
- tracking state design
- KF 的量測頻率假設
- embedding 觸發節奏
- DB search 分工
- GPIO unlock 時序

也就是說，後續的 `rv1106_kf_tracking_spec.md` 應該完全建立在這份文件之上，而不是脫離硬體條件另寫一套理論規格。

---

## 15. 參考資料

- Luckfox Pico RV1106 Wiki
  https://wiki.luckfox.com/Luckfox-Pico-RV1106/
- Luckfox CSI Camera Wiki
  https://wiki.luckfox.com/Luckfox-Pico-Pi/CSI-Camera/
- Luckfox Pico Pro product page
  https://www.luckfox.com/EN-Luckfox-Pico-Pro
- Rockchip RV1106 Datasheet Rev 1.3
  https://files.luckfox.com/wiki/Luckfox-Pico/PDF/Rockchip%20RV1106%20Datasheet%20V1.3-20230522.pdf

---

## 16. 下一步

在這份硬體基線確立後，下一份最合理的文件就是：

- `rv1106_kf_tracking_spec.md`

它應該引用本文件的前提，並明確綁定：

- 直立座標系
- 固定鏡頭
- SCRFD detection output
- RV1106 資源限制
- 大型資料庫節流需求
