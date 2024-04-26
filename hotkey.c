#include <stdlib.h>
#include <windows.h>
#include <stdio.h>
#include <shellapi.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")

// 函数声明
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon(HWND hWnd);
void LoadConfig();
void ToggleTopmost(HWND hWnd);

// 全局变量
HWND g_hWnd;
#define TRAY_ICON_ID 1001

// 结构体存储热键和操作
typedef struct
{
    UINT modifier;
    UINT key;
    char* action; // 改为指针
} HotkeyAction;

HotkeyAction* hotkeyActions = NULL; // 使用指针
int numHotkeys = 0; // 记录热键数量

void ToggleTopmost(HWND hWnd)
{
    // 获取窗口当前的扩展风格
    LONG_PTR exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
    // 切换窗口置顶状态
    if (exStyle & WS_EX_TOPMOST) {
        SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    } else {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

int main()
{
    // 创建窗口
    g_hWnd = CreateWindowEx(0, "STATIC", "Hotkey", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, NULL, NULL);
    if (g_hWnd == NULL) {
        return 1;
    }

    // 设置窗口回调函数
    SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

    // 添加托盘图标
    AddTrayIcon(g_hWnd);

    // 加载配置文件
    LoadConfig();

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 释放内存
    for (int i = 0; i < numHotkeys; i++) {
        free(hotkeyActions[i].action);
    }
    free(hotkeyActions);

    // 移除托盘图标
    RemoveTrayIcon(g_hWnd);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_USER + 1:
            if (lParam == WM_LBUTTONDBLCLK) {
                // 双击托盘图标退出程序
                PostQuitMessage(0);
            }
            break;
        case WM_HOTKEY:
            // 查找对应的操作并执行
            for (int i = 0; i < numHotkeys; i++) {
                if (wParam == i + 1) {
                    if (_stricmp(hotkeyActions[i].action, "OnTop") == 0) {
                        // 切换当前活动窗口的置顶状态
                        HWND activeWindow = GetForegroundWindow();
                        ToggleTopmost(activeWindow);
                    } else {
                        ShellExecute(NULL, "open", hotkeyActions[i].action,
                            NULL, NULL, SW_SHOWNORMAL);
                    }
                    break;
                }
            }
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

void AddTrayIcon(HWND hWnd)
{
    // 准备托盘图标数据
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    nid.uTimeout = 10 * 1000;
    lstrcpy(nid.szTip,
        "Hot Key\n请配置 config.txt\n每一行一个热键定义，格式: 热键=操作\n例如 ctrl+win+c=cmd.exe\nOnTop为操作内置关键字，表示切换窗口置顶\n其他内容表示运行相关程序");

    // 在系统托盘中添加图标
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hWnd)
{
    // 准备托盘图标数据
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;

    // 从系统托盘中移除图标
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void LoadConfig()
{
    FILE* fp = fopen("config.txt", "r");
    if (fp == NULL) {
        return;
    }

    char line[4096];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char hotkeyStr[128];
        char action[4096 - 128];

        // 忽略空白字符，并跳过注释行
        if (sscanf(line, " #%[^\n]", action) == 1 ||
            sscanf(line, " %s #%[^\n]", hotkeyStr, action) == 2)
        {
            continue;
        }

        // 解析配置文件中的每一行，格式为 hotkey=action
        if (sscanf(line, "%[^=]=%s", hotkeyStr, action) != 2) {
            continue;
        }

        // 解析热键
        UINT modifier = 0;
        UINT key = 0;
        char* token = strtok(hotkeyStr, "+");
        while (token != NULL) {
            if (_stricmp(token, "ctrl") == 0) {
                modifier |= MOD_CONTROL;
            } else if (_stricmp(token, "alt") == 0) {
                modifier |= MOD_ALT;
            } else if (_stricmp(token, "shift") == 0) {
                modifier |= MOD_SHIFT;
            } else if (_stricmp(token, "win") == 0) {
                modifier |= MOD_WIN;
            } else {
                key = VkKeyScanA(token[0]);
            }
            token = strtok(NULL, "+");
        }

        // 动态分配内存并存储热键和操作
        HotkeyAction newHotkey;
        newHotkey.modifier = modifier;
        newHotkey.key = key;
        newHotkey.action = malloc(strlen(action) + 1);
        strcpy(newHotkey.action, action);

        // 重新分配内存
        hotkeyActions =
            realloc(hotkeyActions, (numHotkeys + 1) * sizeof(HotkeyAction));
        hotkeyActions[numHotkeys++] = newHotkey;

		RegisterHotKey(g_hWnd, numHotkeys, modifier, key);
    }

    fclose(fp);
}