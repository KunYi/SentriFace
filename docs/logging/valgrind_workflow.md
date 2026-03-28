# Valgrind Workflow

## 1. 目的

這份文件定義專案目前的 `Valgrind` 驗證路徑。

目標不是取代單元測試，而是補強：

- 記憶體洩漏檢查
- 無效讀寫
- 初始化問題

---

## 2. 一鍵工具

目前專案提供：

- `tools/run_valgrind_checks.sh`

---

## 3. 用法

```bash
tools/run_valgrind_checks.sh
```

若不指定測試，預設會跑：

- `system_logger_test`
- `unlock_controller_test`
- `offline_sequence_runner_v2_test`

也可指定特定測試：

```bash
tools/run_valgrind_checks.sh unlock_controller_test
```

---

## 4. 目前定位

這條 workflow 比較適合作為：

- pre-release sanity check
- 新 backend / logging / replay integration 後的回歸檢查

不建議一開始對所有測試都跑 Valgrind，因為成本較高。

較合理的策略是：

1. 先挑高價值 integration tests
2. 確保沒有明顯 memory issue
3. 再視需要擴大覆蓋面
