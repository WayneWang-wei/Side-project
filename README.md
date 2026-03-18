# 高精度 Windows 自動點擊與座標管理工具

### 🚀 專案亮點
這是一個基於 **C++** 與 **Win32 API** 開發的系統級自動化工具，旨在解決市面上巨集軟體在「螢幕座標管理」與「操作直覺性」上的不足。本專案展現了對 Windows 訊息迴圈、多執行緒同步以及系統層級輸入模擬的深度應用。

---

### 🛠️ 技術核心 (Technical Core)

* **多執行緒異步架構 (Multithreading)**：
    * 將 **UI 渲染**與**點擊執行邏輯**分離，確保在高頻率點擊任務下，介面仍能保持即時響應（Non-blocking UI）。
    * 實作 **SafeTerminateThread** 機制：透過 `g_bRunning` 標誌位元與 `WaitForSingleObject` 達成優雅退出 (Graceful Shutdown)，避免直接終止執行緒導致的資源洩漏或死鎖。

* **系統層級監控與模擬**：
    * **座標選取**：利用 `GetCursorPos` 與 `SetTimer` 實作即時座標回傳，達成「所見即所得」的座標設定體驗。
    * **緊急停止機制**：整合 `RegisterHotKey` (系統熱鍵) 與 `GetAsyncKeyState` (異步按鍵偵測)，確保使用者在任何情況下皆能透過 F12/ESC 奪回滑鼠控制權。
    * **輸入模擬**：使用 `mouse_event` 模擬硬體層級點擊，具備穿透 UI 層級的穩定性。

* **視窗技術應用**：
    * 運用 **Layered Windows (WS_EX_LAYERED)** 實作半透明切換功能，解決自動化工具在設定座標時擋住底層視窗的 UX 痛點。

---

### 📈 軟體工程實踐 (Engineering Best Practices)

1.  **回應性優化**：將長延時 (Sleep) 拆解為小區塊循環監控，將程式對停止指令的響應時間控制在 50ms 以內。
2.  **資源管理**：完整實作 `WM_CLOSE` 流程，確保程式關閉時正確註銷熱鍵 (UnregisterHotKey) 並清理執行緒資源。
3.  **持久化設計**：實作結構化檔案 I/O，支援座標設定的導出與導入 (`.txt`)，提升工具的重複使用率。

---

### 💻 開發環境
* **語言**：C++
* **框架**：Win32 API (Native SDK)
* **工具**：MinGW / Visual Studio