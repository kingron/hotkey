#include <stdlib.h>
#include <windows.h>
#include <stdio.h>
#include <shellapi.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")

// ��������
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon(HWND hWnd);
void LoadConfig();
void ToggleTopmost(HWND hWnd);

// ȫ�ֱ���
HWND g_hWnd;
#define TRAY_ICON_ID 1001

// �ṹ��洢�ȼ��Ͳ���
typedef struct
{
    UINT modifier;
    UINT key;
    char* action; // ��Ϊָ��
} HotkeyAction;

HotkeyAction* hotkeyActions = NULL; // ʹ��ָ��
int numHotkeys = 0; // ��¼�ȼ�����

void ToggleTopmost(HWND hWnd)
{
    // ��ȡ���ڵ�ǰ����չ���
    LONG_PTR exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
    // �л������ö�״̬
    if (exStyle & WS_EX_TOPMOST) {
        SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    } else {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

int main()
{
    // ��������
    g_hWnd = CreateWindowEx(0, "STATIC", "Hotkey", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, NULL, NULL);
    if (g_hWnd == NULL) {
        return 1;
    }

    // ���ô��ڻص�����
    SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

    // �������ͼ��
    AddTrayIcon(g_hWnd);

    // ���������ļ�
    LoadConfig();

    // ��Ϣѭ��
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // �ͷ��ڴ�
    for (int i = 0; i < numHotkeys; i++) {
        free(hotkeyActions[i].action);
    }
    free(hotkeyActions);

    // �Ƴ�����ͼ��
    RemoveTrayIcon(g_hWnd);
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_USER + 1:
            if (lParam == WM_LBUTTONDBLCLK) {
                // ˫������ͼ���˳�����
                PostQuitMessage(0);
            }
            break;
        case WM_HOTKEY:
            // ���Ҷ�Ӧ�Ĳ�����ִ��
            for (int i = 0; i < numHotkeys; i++) {
                if (wParam == i + 1) {
                    if (_stricmp(hotkeyActions[i].action, "OnTop") == 0) {
                        // �л���ǰ����ڵ��ö�״̬
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
    // ׼������ͼ������
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    nid.uTimeout = 10 * 1000;
    lstrcpy(nid.szTip,
        "Hot Key\n������ config.txt\nÿһ��һ���ȼ����壬��ʽ: �ȼ�=����\n���� ctrl+win+c=cmd.exe\nOnTopΪ�������ùؼ��֣���ʾ�л������ö�\n�������ݱ�ʾ������س���");

    // ��ϵͳ���������ͼ��
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hWnd)
{
    // ׼������ͼ������
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;

    // ��ϵͳ�������Ƴ�ͼ��
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

        // ���Կհ��ַ���������ע����
        if (sscanf(line, " #%[^\n]", action) == 1 ||
            sscanf(line, " %s #%[^\n]", hotkeyStr, action) == 2)
        {
            continue;
        }

        // ���������ļ��е�ÿһ�У���ʽΪ hotkey=action
        if (sscanf(line, "%[^=]=%s", hotkeyStr, action) != 2) {
            continue;
        }

        // �����ȼ�
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

        // ��̬�����ڴ沢�洢�ȼ��Ͳ���
        HotkeyAction newHotkey;
        newHotkey.modifier = modifier;
        newHotkey.key = key;
        newHotkey.action = malloc(strlen(action) + 1);
        strcpy(newHotkey.action, action);

        // ���·����ڴ�
        hotkeyActions =
            realloc(hotkeyActions, (numHotkeys + 1) * sizeof(HotkeyAction));
        hotkeyActions[numHotkeys++] = newHotkey;

		RegisterHotKey(g_hWnd, numHotkeys, modifier, key);
    }

    fclose(fp);
}