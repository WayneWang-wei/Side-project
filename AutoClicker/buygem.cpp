#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

// 定義控制項 ID
#define ID_START_BUTTON 101
#define ID_STOP_BUTTON 102
#define ID_SAVE_BUTTON 103
#define ID_CLEAR_BUTTON 104
#define ID_COORD_STATIC 105
#define ID_STATUS_STATIC 106
#define ID_INTERVAL_EDIT 107
#define ID_INTERVAL_STATIC 108
#define ID_FILENAME_EDIT 109
#define ID_FILENAME_STATIC 110
#define ID_TRANSPARENT_BUTTON 111  // 新增：透明化按鈕

// 全局變數
HWND g_hWnd = NULL;
HWND g_hCoordStatic = NULL;
HWND g_hStatusStatic = NULL;
HWND g_hIntervalEdit = NULL;
HWND g_hFilenameEdit = NULL;
bool g_bRunning = false;
HANDLE g_hThread = NULL;
POINT g_lastMousePos = {0, 0};
int g_clickInterval = 500;
std::string g_filename = "buygem_coords.txt";
DWORD g_threadId = 0;
bool g_isTransparent = false;  // 透明狀態標誌

// 點擊函數
void MouseClick(int x, int y) {
    // 保存原始滑鼠位置
    POINT originalPos;
    GetCursorPos(&originalPos);
    
    // 移動滑鼠並點擊
    SetCursorPos(x, y);
    Sleep(50);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(50);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(50);
    
    // 恢復滑鼠位置
    SetCursorPos(originalPos.x, originalPos.y);
}

// 從文件讀取座標
std::vector<POINT> ReadCoordinates(const std::string& filename) {
    std::vector<POINT> coords;
    std::ifstream file(filename);
    
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            POINT p;
            if (iss >> p.x >> p.y) {
                coords.push_back(p);
            }
        }
        file.close();
    }
    
    return coords;
}

// 保存座標到文件
void SaveCoordinate(const std::string& filename, int x, int y) {
    std::ofstream file(filename, std::ios::app);
    if (file.is_open()) {
        file << x << " " << y << std::endl;
        file.close();
    }
}

// 清除座標文件
bool ClearCoordinates(const std::string& filename) {
    std::ofstream file(filename, std::ios::trunc);
    if (file.is_open()) {
        file.close();
        return true;
    }
    return false;
}

// 更新狀態文本
void UpdateStatus(const char* status) {
    SetWindowText(g_hStatusStatic, status);
}

// 設置視窗透明度
void SetWindowTransparency(HWND hwnd, bool transparent) {
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if (transparent) {
        // 設置透明
        SetWindowLong(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, 128, LWA_ALPHA); // 50% 透明度
        g_isTransparent = true;
        UpdateStatus("視窗已設為半透明模式");
    } else {
        // 恢復不透明
        SetWindowLong(hwnd, GWL_EXSTYLE, style & ~WS_EX_LAYERED);
        RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        g_isTransparent = false;
        UpdateStatus("視窗已恢復正常模式");
    }
}

// 安全終止執行緒
void SafeTerminateThread() {
    if (g_hThread != NULL) {
        // 設置停止標誌
        g_bRunning = false;
        
        // 等待執行緒結束，但設置超時
        DWORD waitResult = WaitForSingleObject(g_hThread, 1000); // 等待最多1秒
        
        // 如果執行緒沒有在指定時間內結束，則強制終止
        if (waitResult == WAIT_TIMEOUT) {
            TerminateThread(g_hThread, 0);
            UpdateStatus("執行緒強制終止");
        }
        
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }
}

// 點擊執行緒
DWORD WINAPI ClickThread(LPVOID lpParam) {
    UpdateStatus("正在執行點擊...");
    
    // 讀取座標
    std::vector<POINT> coords = ReadCoordinates(g_filename);
    
    if (coords.empty()) {
        UpdateStatus("錯誤：座標文件為空或不存在！");
        g_bRunning = false;
        return 1;
    }
    
    // 獲取點擊間隔
    char intervalText[32];
    GetWindowText(g_hIntervalEdit, intervalText, sizeof(intervalText));
    g_clickInterval = atoi(intervalText);
    if (g_clickInterval < 100) g_clickInterval = 100;
    
    // 執行點擊循環
    while (g_bRunning) {
        for (size_t i = 0; i < coords.size() && g_bRunning; ++i) {
            char statusText[128];
            sprintf(statusText, "點擊座標 (%d, %d) - %zu/%zu", coords[i].x, coords[i].y, i+1, coords.size());
            UpdateStatus(statusText);
            
            MouseClick(coords[i].x, coords[i].y);
            
            // 點擊間隔 - 分段檢查停止標誌，以便更快響應
            for (int j = 0; j < g_clickInterval && g_bRunning; j += 50) {
                Sleep(50);
                // 檢查是否有停止請求
                if (!g_bRunning) break;
                
                // 檢查是否有按鍵按下 - 按任意鍵停止
                if (GetAsyncKeyState(VK_ESCAPE) & 0x8000 || 
                    GetAsyncKeyState(VK_F12) & 0x8000 ||
                    GetAsyncKeyState(VK_SPACE) & 0x8000) {
                    g_bRunning = false;
                    UpdateStatus("緊急停止 (按鍵按下)");
                    break;
                }
            }
        }
    }
    
    UpdateStatus("點擊已停止");
    return 0;
}

// 定時器回調函數 - 更新滑鼠座標顯示
void CALLBACK UpdateMousePosition(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    POINT p;
    GetCursorPos(&p);
    
    // 只有當座標變化時才更新顯示
    if (p.x != g_lastMousePos.x || p.y != g_lastMousePos.y) {
        g_lastMousePos = p;
        char buf[64];
        sprintf(buf, "滑鼠座標: X=%d, Y=%d", p.x, p.y);
        SetWindowText(g_hCoordStatic, buf);
    }
    
    // 檢查是否有按鍵按下 - 按任意鍵停止
    if (g_bRunning) {
        // 檢查常用鍵
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000 || 
            GetAsyncKeyState(VK_F12) & 0x8000 ||
            GetAsyncKeyState(VK_SPACE) & 0x8000) {
            UpdateStatus("緊急停止 (定時器檢測到按鍵)");
            SafeTerminateThread();
            UpdateStatus("點擊已被按鍵停止");
        }
    }
}

// 窗口過程函數
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            // 創建控制項
            CreateWindow("BUTTON", "開始點擊", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         20, 20, 100, 30, hwnd, (HMENU)ID_START_BUTTON, GetModuleHandle(NULL), NULL);
            
            CreateWindow("BUTTON", "停止點擊", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         130, 20, 100, 30, hwnd, (HMENU)ID_STOP_BUTTON, GetModuleHandle(NULL), NULL);
            
            CreateWindow("BUTTON", "保存當前座標", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         240, 20, 120, 30, hwnd, (HMENU)ID_SAVE_BUTTON, GetModuleHandle(NULL), NULL);
            
            CreateWindow("BUTTON", "清除所有座標", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         240, 60, 120, 30, hwnd, (HMENU)ID_CLEAR_BUTTON, GetModuleHandle(NULL), NULL);
            
            // 新增：透明化按鈕
            CreateWindow("BUTTON", "切換透明度", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         20, 170, 100, 30, hwnd, (HMENU)ID_TRANSPARENT_BUTTON, GetModuleHandle(NULL), NULL);
            
            g_hCoordStatic = CreateWindow("STATIC", "滑鼠座標: X=0, Y=0", WS_VISIBLE | WS_CHILD | SS_LEFT,
                                         20, 70, 200, 20, hwnd, (HMENU)ID_COORD_STATIC, GetModuleHandle(NULL), NULL);
            
            g_hStatusStatic = CreateWindow("STATIC", "就緒", WS_VISIBLE | WS_CHILD | SS_LEFT,
                                          20, 210, 340, 20, hwnd, (HMENU)ID_STATUS_STATIC, GetModuleHandle(NULL), NULL);
            
            CreateWindow("STATIC", "點擊間隔 (毫秒):", WS_VISIBLE | WS_CHILD | SS_LEFT,
                         20, 100, 120, 20, hwnd, (HMENU)ID_INTERVAL_STATIC, GetModuleHandle(NULL), NULL);
            
            g_hIntervalEdit = CreateWindow("EDIT", "500", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                                          150, 100, 80, 20, hwnd, (HMENU)ID_INTERVAL_EDIT, GetModuleHandle(NULL), NULL);
            
            CreateWindow("STATIC", "座標文件:", WS_VISIBLE | WS_CHILD | SS_LEFT,
                         20, 130, 120, 20, hwnd, (HMENU)ID_FILENAME_STATIC, GetModuleHandle(NULL), NULL);
            
            g_hFilenameEdit = CreateWindow("EDIT", "buygem_coords.txt", WS_VISIBLE | WS_CHILD | WS_BORDER,
                                          150, 130, 210, 20, hwnd, (HMENU)ID_FILENAME_EDIT, GetModuleHandle(NULL), NULL);
            
            // 添加說明文字
            CreateWindow("STATIC", "透明後將保存座標重疊至指定", WS_VISIBLE | WS_CHILD | SS_LEFT,
                         150, 170, 210, 20, hwnd, (HMENU)-1, GetModuleHandle(NULL), NULL);
            
            CreateWindow("STATIC", "運行時按F12/ESC可停止", WS_VISIBLE | WS_CHILD | SS_LEFT,
                         150, 190, 210, 20, hwnd, (HMENU)-1, GetModuleHandle(NULL), NULL);
            
            // 設置定時器更新滑鼠座標
            SetTimer(hwnd, 1, 100, UpdateMousePosition);
            
            // 註冊全局熱鍵 (F12) 用於緊急停止
            RegisterHotKey(hwnd, 1, 0, VK_F12);
            RegisterHotKey(hwnd, 2, 0, VK_ESCAPE);
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_START_BUTTON:
                    if (!g_bRunning) {
                        // 獲取文件名
                        char filename[MAX_PATH];
                        GetWindowText(g_hFilenameEdit, filename, MAX_PATH);
                        g_filename = filename;
                        
                        // 啟動點擊執行緒
                        g_bRunning = true;
                        g_hThread = CreateThread(NULL, 0, ClickThread, NULL, 0, &g_threadId);
                        
                        if (g_hThread == NULL) {
                            g_bRunning = false;
                            UpdateStatus("錯誤：無法創建執行緒！");
                        } else {
                            UpdateStatus("點擊已開始。按F12/ESC/空格停止。");
                        }
                    }
                    break;
                    
                case ID_STOP_BUTTON:
                    if (g_bRunning) {
                        UpdateStatus("正在停止點擊...");
                        SafeTerminateThread();
                        UpdateStatus("點擊已停止");
                    }
                    break;
                    
                case ID_SAVE_BUTTON:
                    {
                        // 獲取文件名
                        char filename[MAX_PATH];
                        GetWindowText(g_hFilenameEdit, filename, MAX_PATH);
                        g_filename = filename;
                        
                        // 保存當前滑鼠座標
                        SaveCoordinate(g_filename, g_lastMousePos.x, g_lastMousePos.y);
                        
                        char statusText[128];
                        sprintf(statusText, "已保存座標 (%d, %d) 到 %s", g_lastMousePos.x, g_lastMousePos.y, g_filename.c_str());
                        UpdateStatus(statusText);
                    }
                    break;
                    
                case ID_CLEAR_BUTTON:
                    {
                        // 確認對話框
                        if (MessageBox(hwnd, "確定要清除所有座標嗎？", "確認", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                            // 獲取文件名
                            char filename[MAX_PATH];
                            GetWindowText(g_hFilenameEdit, filename, MAX_PATH);
                            g_filename = filename;
                            
                            // 清除座標文件
                            if (ClearCoordinates(g_filename)) {
                                char statusText[128];
                                sprintf(statusText, "已清除文件 %s 中的所有座標", g_filename.c_str());
                                UpdateStatus(statusText);
                            } else {
                                UpdateStatus("清除座標失敗！");
                            }
                        }
                    }
                    break;
                    
                case ID_TRANSPARENT_BUTTON:  // 新增：透明化按鈕處理
                    SetWindowTransparency(hwnd, !g_isTransparent);
                    break;
            }
            break;
            
        case WM_KEYDOWN:  // 處理按鍵事件
            if (g_bRunning) {
                char keyName[32] = "按鍵";
                if (wParam == VK_F12) {
                    strcpy(keyName, "F12");
                } else if (wParam == VK_ESCAPE) {
                    strcpy(keyName, "ESC");
                } else if (wParam == VK_SPACE) {
                    strcpy(keyName, "空格");
                } else if (wParam >= 'A' && wParam <= 'Z') {
                    sprintf(keyName, "%c 鍵", (char)wParam);
                }
                
                char statusText[128];
                sprintf(statusText, "緊急停止 (按下了 %s)", keyName);
                UpdateStatus(statusText);
                SafeTerminateThread();
                UpdateStatus("點擊已緊急停止");
            }
            break;
            
        case WM_HOTKEY:
            if (wParam >= 1 && wParam <= 3) {  // 任何註冊的熱鍵ID
                if (g_bRunning) {
                    char statusText[128];
                    sprintf(statusText, "緊急停止 (熱鍵ID: %d)", (int)wParam);
                    UpdateStatus(statusText);
                    SafeTerminateThread();
                    UpdateStatus("點擊已被熱鍵停止");
                }
            }
            break;
            
        case WM_CLOSE:
            // 確保執行緒被終止
            if (g_bRunning) {
                SafeTerminateThread();
            }
            
            // 註銷所有熱鍵
            for (int i = 1; i <= 3; i++) {
                UnregisterHotKey(hwnd, i);
            }
            
            // 關閉定時器
            KillTimer(hwnd, 1);
            
            DestroyWindow(hwnd);
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// 程式入口點
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 註冊窗口類
    WNDCLASSEX wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "BuyGemClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "窗口註冊失敗！", "錯誤", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // 創建窗口
    g_hWnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "BuyGemClass",
        "自動點擊工具",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 280,
        NULL, NULL, hInstance, NULL
    );
    
    if (g_hWnd == NULL) {
        MessageBox(NULL, "窗口創建失敗！", "錯誤", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    // 顯示窗口
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    
    // 消息循環
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return msg.wParam;
}
