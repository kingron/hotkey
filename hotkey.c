#include <stdlib.h>
#include <windows.h>
#include <stdio.h>
#include <shellapi.h>
#include <psapi.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "psapi")

// 函数声明
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon(HWND hWnd);
void LoadConfig();
void ToggleTopmost(HWND hWnd);

// 全局变量
HWND g_hWnd;
char g_ballonInfo[256] = {0};

#define MAX_BUFF 4096
#define TRAY_ICON_ID 1001

// 结构体存储热键和操作
typedef struct
{
    UINT modifier;
    UINT key;
    char *action;
} HotkeyAction;

HotkeyAction *hotkeyActions = NULL; // 使用指针
int numHotkeys = 0;                 // 记录热键数量

// 切换窗口置顶状态
void ToggleTopmost(HWND hWnd)
{
    LONG_PTR exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOPMOST)
    {
        SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    else
    {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

int main()
{
    HANDLE hMutex = CreateMutex(NULL, TRUE, "llds_hotkey");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hMutex);
        return 0;
    }

    // 创建窗口
    g_hWnd = CreateWindowEx(0, "STATIC", "Hotkey", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, NULL, NULL);
    if (g_hWnd == NULL)
    {
        return 1;
    }

    // 设置窗口回调函数
    SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

    // 加载配置文件
    LoadConfig();

    // 添加托盘图标
    AddTrayIcon(g_hWnd);
    EmptyWorkingSet(GetCurrentProcess());

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 释放内存
    for (int i = 0; i < numHotkeys; i++)
    {
        UnregisterHotKey(g_hWnd, i + 1);
        free(hotkeyActions[i].action);
    }
    free(hotkeyActions);

    // 移除托盘图标
    RemoveTrayIcon(g_hWnd);
    return 0;
}

void extractCmdAndParameter(const char *action, char *cmd, char *parameter)
{
    memset(cmd, 0, MAX_BUFF);
    memset(parameter, 0, MAX_BUFF);
    cmd[0] = action[0];
    char stopChar = ' ';
    if ('"' == action[0])
    {
        stopChar = '"';
    }
    size_t i = 1;
    for (; action[i] != '\0' && action[i] != stopChar; i++)
    {
        cmd[i] = action[i];
    }
    if ('"' == stopChar)
    {
        cmd[i] = action[i];
        i++;
    }
    strcpy(parameter, action + i);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_USER + 1:
        if (lParam == WM_LBUTTONDBLCLK)
        {
            // 双击托盘图标退出程序
            PostQuitMessage(0);
        }
        break;
    case WM_HOTKEY:
        // 查找对应的操作并执行
        for (int i = 0; i < numHotkeys; i++)
        {
            if (wParam == i + 1)
            {
                if (_stricmp(hotkeyActions[i].action, "Minimize") == 0)
                {
                    ShowWindow(GetForegroundWindow(), SW_MINIMIZE);
                }
                else if (_stricmp(hotkeyActions[i].action, "Close") == 0)
                {
                    SendMessage(GetForegroundWindow(), WM_CLOSE, 0, 0);
                }
                else if (_stricmp(hotkeyActions[i].action, "OnTop") == 0)
                {
                    ToggleTopmost(GetForegroundWindow());
                }
                else
                {
                    char cmd[MAX_BUFF] = {0};
                    char parameter[MAX_BUFF] = {0};
                    extractCmdAndParameter(hotkeyActions[i].action, cmd, parameter);
                    ShellExecute(NULL, "open", cmd, parameter, NULL, SW_SHOWNORMAL);
                    EmptyWorkingSet(GetCurrentProcess());
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

// 在系统托盘中添加图标
void AddTrayIcon(HWND hWnd)
{
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    nid.uTimeout = 10 * 1000;
    if (numHotkeys == 0)
    {
        strncpy_s(nid.szTip, sizeof(nid.szTip), "请配置 config.txt，每行一个热键，格式: 热键=操作\n例: ctrl+win+c=cmd.exe\nOnTop为操作内置关键字，表示切换窗口置顶", _TRUNCATE);
    }
    else
    {
        snprintf(nid.szTip, sizeof(nid.szTip), "Hotkey 运行中，共 %d 个操作", numHotkeys);
    }

    if (g_ballonInfo[0] != '\0')
    {
        nid.uFlags |= NIF_INFO;
        nid.dwInfoFlags = NIIF_INFO;
        strncpy_s(nid.szInfoTitle, sizeof(nid.szInfoTitle), "注册失败的热键", _TRUNCATE);
        strncpy_s(nid.szInfo, sizeof(nid.szInfo), g_ballonInfo, _TRUNCATE);
    }

    Shell_NotifyIcon(NIM_ADD, &nid);
}

// 从系统托盘中移除图标
void RemoveTrayIcon(HWND hWnd)
{
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

typedef struct
{
    const char *keyName;
    UINT keyCode;
} KeyMapping;

KeyMapping keyMappings[] = {
    {"Numpad0", VK_NUMPAD0},
    {"Numpad1", VK_NUMPAD1},
    {"Numpad2", VK_NUMPAD2},
    {"Numpad3", VK_NUMPAD3},
    {"Numpad4", VK_NUMPAD4},
    {"Numpad5", VK_NUMPAD5},
    {"Numpad6", VK_NUMPAD6},
    {"Numpad7", VK_NUMPAD7},
    {"Numpad8", VK_NUMPAD8},
    {"Numpad9", VK_NUMPAD9},
    {"F1", VK_F1},
    {"F2", VK_F2},
    {"F3", VK_F3},
    {"F4", VK_F4},
    {"F5", VK_F5},
    {"F6", VK_F6},
    {"F7", VK_F7},
    {"F8", VK_F8},
    {"F9", VK_F9},
    {"F10", VK_F10},
    {"F11", VK_F11},
    {"F12", VK_F12},
    {"Left", VK_LEFT},
    {"Up", VK_UP},
    {"Right", VK_RIGHT},
    {"Down", VK_DOWN},
    {"PageUp", VK_PRIOR},
    {"PageDown", VK_NEXT},
    {"Home", VK_HOME},
    {"End", VK_END},
    {"Insert", VK_INSERT},
    {"Delete", VK_DELETE},
    {"Space", VK_SPACE},
    {"Backspace", VK_BACK},
    {"Add", VK_ADD},
    {"Multiply", VK_MULTIPLY},
    {"Separator", VK_SEPARATOR},
    {"Subtract", VK_SUBTRACT},
    {"Decimal", VK_DECIMAL},
    {"Divide", VK_DIVIDE},
    {"Print", VK_PRINT},
    {"Pause", VK_PAUSE},
    {"Scroll", VK_SCROLL},
    {"Enter", VK_RETURN},
    {"Escape", VK_ESCAPE},
    {"NumLock", VK_NUMLOCK},
    {"Select", VK_SELECT},
    {"Tab", VK_TAB},
};

// 查找键名对应的键码
UINT GetKeyCodeFromMapping(const char *keyName)
{
    for (size_t i = 0; i < sizeof(keyMappings) / sizeof(keyMappings[0]); i++)
    {
        if (_stricmp(keyMappings[i].keyName, keyName) == 0)
        {
            return keyMappings[i].keyCode;
        }
    }
    return VkKeyScanA(*keyName); // 找不到则返回默认键码
}

void LoadConfig()
{
    FILE *fp = fopen("config.txt", "r");
    if (fp == NULL)
    {
        return;
    }

    char line[MAX_BUFF];
    char hotkeyStr[32];
    char action[sizeof(line) - sizeof(hotkeyStr)];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // 忽略空白字符，并跳过注释行，注释以 # 开头
        char *p = line;
        while ((p < line + sizeof(line) - 1) && (*p == ' ' || *p == '\t'))
        {
            p++;
        }
        if (*p == '#')
        {
            continue;
        }

        // 解析配置文件中的每一行，格式为 hotkey=action
        if (sscanf(line, "%[^=]=%[^\n]", hotkeyStr, action) != 2)
        {
            continue;
        }

        UINT modifier = 0;
        UINT key = 0;
        char *token;
        char *nextToken;
        token = strtok_s(hotkeyStr, "+", &nextToken);
        while (token != NULL)
        {
            if (_stricmp(token, "ctrl") == 0)
            {
                modifier |= MOD_CONTROL;
            }
            else if (_stricmp(token, "alt") == 0)
            {
                modifier |= MOD_ALT;
            }
            else if (_stricmp(token, "shift") == 0)
            {
                modifier |= MOD_SHIFT;
            }
            else if (_stricmp(token, "win") == 0)
            {
                modifier |= MOD_WIN;
            }
            else
            {
                key = GetKeyCodeFromMapping(token);
            }
            token = strtok_s(NULL, "+", &nextToken);
        }

        // 动态分配内存并存储热键和操作
        HotkeyAction newHotkey;
        newHotkey.modifier = modifier;
        newHotkey.key = key;
        size_t actionLength = strlen(action);
        newHotkey.action = (char *)malloc(actionLength + 1);
        strncpy_s(newHotkey.action, actionLength + 1, action, actionLength);
        OutputDebugString("Read action:");
        OutputDebugString(action);

        // 重新分配内存
        hotkeyActions = realloc(hotkeyActions, (numHotkeys + 1) * sizeof(HotkeyAction));
        hotkeyActions[numHotkeys++] = newHotkey;

        if (!RegisterHotKey(g_hWnd, numHotkeys, modifier, key))
        {
            sscanf(line, "%[^=]=%s", hotkeyStr, action);
            strncat_s(g_ballonInfo, sizeof(g_ballonInfo), hotkeyStr, _TRUNCATE);
            strncat_s(g_ballonInfo, sizeof(g_ballonInfo), "\n", _TRUNCATE);
        }
    }

    fclose(fp);
}
