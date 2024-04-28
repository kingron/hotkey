// #define UNICODE

#pragma comment(linker, "/SUBSYSTEM:windows")
#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "psapi")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "shlwapi")

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define LWSTDAPI_(type) EXTERN_C DECLSPEC_IMPORT type WINAPI
LWSTDAPI_(WINBOOL) StrTrimA(LPSTR psz,LPCSTR pszTrimChars);
LWSTDAPI_(WINBOOL) StrTrimW(LPWSTR psz,LPCWSTR pszTrimChars);

#ifdef UNICODE
#define U(x) L##x
#define MCHAR WCHAR
#define my_strcat wcscat_s
#define my_snprintf swprintf_s
#define my_fgets fgetws
#define my_strlen wcslen
#define my_strncpy wcsncpy_s
#define my_stricmp wcsicmp
#define my_strtok wcstok_s
#define my_sscanf swscanf
#define my_strnicmp _wcsnicmp
#define Shell_NotifyIcon Shell_NotifyIconW
#define ShellExecute ShellExecuteW
#define Debug(x) OutputDebugStringW(L##x)
#define DebugV(x) OutputDebugStringW(x)
#define StrTrim StrTrimW
#else
#define U(x) x
#define MCHAR char
#define my_strcat strcat_s
#define my_snprintf snprintf
#define my_fgets fgets
#define my_strlen strlen
#define my_strncpy strncpy_s
#define my_stricmp _stricmp
#define my_strtok strtok_s
#define my_sscanf sscanf
#define my_strnicmp strnicmp
#define Shell_NotifyIcon Shell_NotifyIconA
#define ShellExecute ShellExecuteA
#define Debug(x) OutputDebugStringA(x)
#define DebugV(x) OutputDebugStringA(x)
#define StrTrim StrTrimA
#endif

#define MAX_STACK_SIZE 10
typedef struct {
    void* items[MAX_STACK_SIZE];
    int top;
} Stack;

typedef struct _NOTIFYICONDATA {
  DWORD cbSize;
  HWND hWnd;
  UINT uID;
  UINT uFlags;
  UINT uCallbackMessage;
  HICON hIcon;
  MCHAR szTip[128];
  DWORD dwState;
  DWORD dwStateMask;
  MCHAR szInfo[256];
  union {
    UINT uTimeout;
    UINT uVersion;
  } DUMMYUNIONNAME;
  MCHAR szInfoTitle[64];
  DWORD dwInfoFlags;
#if (_WIN32_IE >= 0x600)
  GUID guidItem;
#endif
} NOTIFYICONDATA, *PNOTIFYICONDATA;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon(HWND hWnd);
void LoadConfig();
void ToggleTopmost(HWND hWnd);

MCHAR **g_argv;
int g_argc;
HWND g_hWnd;
HWND g_hwndEdit;
HANDLE hMutex;
MCHAR g_ballonInfo[256] = {0};
Stack stack;

#define _TRUNCATE ((size_t)-1)
#define MAX_BUFF 4096
#define TRAY_ICON_ID 1001
#define NIF_MESSAGE 0x00000001
#define NIF_ICON 0x00000002
#define NIF_TIP 0x00000004
#define NIF_STATE 0x00000008
#define NIF_INFO 0x00000010
#define NIIF_INFO 0x00000001
#define NIM_ADD 0x00000000
#define NIM_DELETE 0x00000002

void init(Stack* stack) {
  stack->top = -1;
}

int isFull(Stack* stack) {
  return stack->top == MAX_STACK_SIZE - 1;
}

int isEmpty(Stack* stack) {
  return stack->top == -1;
}

BOOLEAN push(Stack* stack, void* item) {
  if (!isFull(stack)) {
    stack->top++;
    stack->items[stack->top] = item;
    return TRUE;
  } else {
    return FALSE;
  }
}

void* pop(Stack* stack) {
  if (!isEmpty(stack)) {
    void* item = stack->items[stack->top];
    stack->top--;
    return item;
  } else {
    return NULL;
  }
}

typedef struct {
  UINT modifier;
  UINT key;
  MCHAR *action;
} HotkeyAction;

HotkeyAction *hotkeyActions = NULL;
int numHotkeys = 0;

void ToggleTopmost(HWND hWnd) {
  LONG_PTR exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
  if (exStyle & WS_EX_TOPMOST) {
    SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  } else {
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  }
}

void SetDefaultFont(HWND hWnd)
{
    LOGFONT lf;
    HFONT hFont;
    HDC hdc = GetDC(hWnd);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hWnd, hdc);

    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfHeight = -MulDiv(10, dpiY, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    my_strncpy(lf.lfFaceName, sizeof(lf.lfFaceName), U("Cascadia Code"), _TRUNCATE);

    hFont = CreateFontIndirect(&lf);
    SendMessage(hWnd, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void createWindow() {
  g_hWnd = CreateWindowEx(0, U("STATIC"), U("Hotkey"), WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT, 600, 480, NULL, NULL, NULL, NULL);
  if (g_hWnd == NULL) {
    exit(1);
  }

  SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
  g_hwndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, U("EDIT"), NULL,
                              WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | WS_HSCROLL | ES_AUTOHSCROLL,
                              0, 0, 600, 300, g_hWnd, (HMENU) 100, (HINSTANCE) GetWindowLongPtr(g_hWnd, GWLP_HINSTANCE), NULL);
  RECT desktopRect;
  SystemParametersInfo(SPI_GETWORKAREA, 0, &desktopRect, 0);
  int desktopWidth = desktopRect.right - desktopRect.left;
  int desktopHeight = desktopRect.bottom - desktopRect.top;
  SetWindowPos(g_hWnd, HWND_TOPMOST, desktopWidth - 600, desktopHeight - 300, 600, 300, 0);
  SetDefaultFont(g_hwndEdit);
}

#ifdef UNICODE
int wmain(int argc, MCHAR *argv[])
#else
int main(int argc, char *argv[])
#endif
{
  hMutex = CreateMutex(NULL, TRUE, U("llds_hotkey"));
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(hMutex);
    return 0;
  }

  g_argv = argv;
  g_argc = argc;
  init(&stack);

  createWindow();
  LoadConfig();
  AddTrayIcon(g_hWnd);
  EmptyWorkingSet(GetCurrentProcess());

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  for (int i = 0; i < numHotkeys; i++) {
    UnregisterHotKey(g_hWnd, i + 1);
    free(hotkeyActions[i].action);
  }
  free(hotkeyActions);
  RemoveTrayIcon(g_hWnd);
  return 0;
}

void appendText(HWND hWndEdit, MCHAR* lpText) {
  DWORD len = GetWindowTextLength(hWndEdit);
  SendMessage(hWndEdit, EM_SETSEL, len, len);
  SendMessage(hWndEdit, EM_REPLACESEL, 0, (LPARAM) lpText);
}

void extractCmdAndParameter(const MCHAR *action, MCHAR *cmd, MCHAR *parameter) {
  memset(cmd, 0, MAX_BUFF);
  memset(parameter, 0, MAX_BUFF);
  cmd[0] = action[0];
  MCHAR stopChar = '"' == action[0] ? '"' : ' ';

  size_t i = 1;
  for (; action[i] != '\0' && action[i] != stopChar; i++) {
    cmd[i] = action[i];
  }
  if ('"' == stopChar) {
    cmd[i] = action[i];
    i++;
  }
  my_strcat(parameter, MAX_BUFF, action + i);
}

void unescape(const MCHAR* input, MCHAR* output) {
    while (*input) {
        if (*input == '\\' && *(input + 1)) {
            switch (*(input + 1)) {
                case 'n':
                    *output++ = '\n';
                    break;
                case 't':
                    *output++ = '\t';
                    break;
                case 'r':
                    *output++ = '\r';
                    break;
                case '\\':
                    *output++ = '\\';
                    break;
                default:
                    *output++ = *(input + 1);
                    break;
            }
            input += 2;
        } else {
            *output++ = *input++;
        }
    }
    *output = '\0';
}

void SendText(const MCHAR* string) {
  MCHAR text[my_strlen(string)];
  unescape(string, text);
  for (size_t i = 0; i < my_strlen(text); ++i) {
    INPUT inputs[2];
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = 0;
    inputs[0].ki.wScan = text[i];
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0;
    inputs[1].ki.wScan = text[i];
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
  }
}

void doHotKey(int index) {
  if (index < 0 || index >= numHotkeys) {
    return;
  }

  if (my_stricmp(hotkeyActions[index].action, U("Minimize")) == 0) {
    ShowWindow(GetForegroundWindow(), SW_MINIMIZE);
  } else if (my_stricmp(hotkeyActions[index].action, U("Close")) == 0) {
    SendMessage(GetForegroundWindow(), WM_CLOSE, 0, 0);
  } else if (my_stricmp(hotkeyActions[index].action, U("Hide")) == 0) {
    HWND hwndActive = GetForegroundWindow();
    if (isFull(&stack)) {
      MessageBox(GetForegroundWindow(), U("Too many hide windows"), 
                 U("Error"), MB_ICONERROR | MB_OK | MB_TOPMOST);
      return;
    }

    if (ShowWindow(hwndActive, SW_HIDE)) {
      push(&stack, hwndActive);
    }
  } else if (my_stricmp(hotkeyActions[index].action, U("Unhide")) == 0) {
    HWND hwndLast = (HWND) pop(&stack);
    if (hwndLast == NULL) {
      MessageBox(GetForegroundWindow(), U("No hidden window"), 
                 U("Error"), MB_ICONERROR | MB_OK | MB_TOPMOST);
      return;
    }

    ShowWindow(hwndLast, SW_SHOW);
    SetForegroundWindow(hwndLast);
  } else if (my_stricmp(hotkeyActions[index].action, U("Maximize")) == 0) {
    ShowWindow(GetForegroundWindow(), SW_MAXIMIZE);
  } else if (my_stricmp(hotkeyActions[index].action, U("Reload")) == 0) {
    CloseHandle(hMutex);
    RemoveTrayIcon(g_hWnd);
    ShellExecute(NULL, U("open"), g_argv[0], g_argv[1], NULL, SW_SHOWDEFAULT);
    exit(0);
  } else if (my_strnicmp(hotkeyActions[index].action, U("Text "), 5) == 0) {
    SendText(hotkeyActions[index].action + 5);
  } else if (my_stricmp(hotkeyActions[index].action, U("OnTop")) == 0) {
    ToggleTopmost(GetForegroundWindow());
  } else {
    MCHAR cmd[MAX_BUFF] = {0};
    MCHAR parameter[MAX_BUFF] = {0};
    extractCmdAndParameter(hotkeyActions[index].action, cmd, parameter);
    ShellExecute(NULL, U("open"), cmd, parameter, NULL, SW_SHOWNORMAL);
    EmptyWorkingSet(GetCurrentProcess());
  }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_SIZE:
    SetWindowPos(g_hwndEdit, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
    break;
  case WM_CLOSE:
    ShowWindow(hWnd, SW_HIDE);
    break;
  case WM_USER + 1:
    if (lParam == WM_RBUTTONUP) {
      PostQuitMessage(0);
    } else if (lParam == WM_LBUTTONDOWN) {
      if (IsWindowVisible(hWnd)) {
        ShowWindow(hWnd, SW_HIDE);
      } else {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        ShowWindow(hWnd, SW_SHOW);
        SetForegroundWindow(hWnd);
      }
    }
    break;
  case WM_HOTKEY:
    doHotKey(wParam -1);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }

  return 0;
}

void AddTrayIcon(HWND hWnd) {
  NOTIFYICONDATA nid;
  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = hWnd;
  nid.uID = TRAY_ICON_ID;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_USER + 1;
  nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  nid.uTimeout = 10 * 1000;
  if (numHotkeys == 0) {
    my_strncpy(nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]),
               U("Please edit config.txt, line format: \nkey=action"), _TRUNCATE);
  } else {
    my_snprintf(nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]),
                U("Hotkey running with %d action(s)"), numHotkeys);
  }
#ifdef UNICODE
  my_strcat(nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]), L"\nHotkey v1.0(Unicode)");
#else
  my_strcat(nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]), "\nHotkey v1.0");
#endif

  if (g_ballonInfo[0] != '\0') {
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    my_strncpy(nid.szInfoTitle, sizeof(nid.szInfoTitle) / sizeof(nid.szInfoTitle[0]),
               U("Failed hot keys"), _TRUNCATE);
    my_strncpy(nid.szInfo, sizeof(nid.szInfo) / sizeof(nid.szInfo[0]), g_ballonInfo, _TRUNCATE);
  }

  Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hWnd) {
  NOTIFYICONDATA nid;
  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = hWnd;
  nid.uID = TRAY_ICON_ID;
  Shell_NotifyIcon(NIM_DELETE, &nid);
}

typedef struct {
  const MCHAR *keyName;
  UINT keyCode;
} KeyMapping;

KeyMapping keyMappings[] = {
    {U("Numpad0"), VK_NUMPAD0},
    {U("Numpad1"), VK_NUMPAD1},
    {U("Numpad2"), VK_NUMPAD2},
    {U("Numpad3"), VK_NUMPAD3},
    {U("Numpad4"), VK_NUMPAD4},
    {U("Numpad5"), VK_NUMPAD5},
    {U("Numpad6"), VK_NUMPAD6},
    {U("Numpad7"), VK_NUMPAD7},
    {U("Numpad8"), VK_NUMPAD8},
    {U("Numpad9"), VK_NUMPAD9},
    {U("F1"), VK_F1},
    {U("F2"), VK_F2},
    {U("F3"), VK_F3},
    {U("F4"), VK_F4},
    {U("F5"), VK_F5},
    {U("F6"), VK_F6},
    {U("F7"), VK_F7},
    {U("F8"), VK_F8},
    {U("F9"), VK_F9},
    {U("F10"), VK_F10},
    {U("F11"), VK_F11},
    {U("F12"), VK_F12},
    {U("Left"), VK_LEFT},
    {U("Up"), VK_UP},
    {U("Right"), VK_RIGHT},
    {U("Down"), VK_DOWN},
    {U("PageUp"), VK_PRIOR},
    {U("PageDown"), VK_NEXT},
    {U("Home"), VK_HOME},
    {U("End"), VK_END},
    {U("Insert"), VK_INSERT},
    {U("Delete"), VK_DELETE},
    {U("Space"), VK_SPACE},
    {U("Backspace"), VK_BACK},
    {U("Add"), VK_ADD},
    {U("Multiply"), VK_MULTIPLY},
    {U("Separator"), VK_SEPARATOR},
    {U("Subtract"), VK_SUBTRACT},
    {U("Decimal"), VK_DECIMAL},
    {U("Divide"), VK_DIVIDE},
    {U("Print"), VK_PRINT},
    {U("Pause"), VK_PAUSE},
    {U("Scroll"), VK_SCROLL},
    {U("Enter"), VK_RETURN},
    {U("Escape"), VK_ESCAPE},
    {U("NumLock"), VK_NUMLOCK},
    {U("Select"), VK_SELECT},
    {U("Tab"), VK_TAB},
};

UINT GetKeyCodeFromMapping(const MCHAR *keyName) {
  for (size_t i = 0; i < sizeof(keyMappings) / sizeof(keyMappings[0]); i++) {
    if (my_stricmp(keyMappings[i].keyName, keyName) == 0) {
      return keyMappings[i].keyCode;
    }
  }
  return VkKeyScan(*keyName);
}

void LoadConfig() {
#ifdef UNICODE
  FILE *fp = _wfopen(L"config.txt", L"r, ccs=UTF-8");
#else
  FILE *fp = fopen("config.txt", "r");
#endif
  if (fp == NULL) {
    return;
  }

  MCHAR line[MAX_BUFF];
  MCHAR hotkeyStr[32];
  MCHAR action[MAX_BUFF - 32];
  while (my_fgets(line, MAX_BUFF, fp) != NULL) {
    MCHAR *p = line;
    while ((p < line + MAX_BUFF - 1) && (*p == ' ' || *p == '\t')) {
      p++;
    }
    if (*p == '#') {
      continue;
    }

    if (my_sscanf(line, U("%[^=]=%[^\n]"), hotkeyStr, action) != 2) {
      continue;
    }

    // DebugV(line);
    UINT modifier = 0;
    UINT key = 0;
    MCHAR *token;
    MCHAR *nextToken;
    token = (MCHAR*) my_strtok(hotkeyStr, U("+"), &nextToken);
    while (token != NULL) {
      StrTrim(token, U(" \r\t\n"));
      if (my_stricmp(token, U("ctrl")) == 0) {
        modifier |= MOD_CONTROL;
      } else if (my_stricmp(token, U("alt")) == 0) {
        modifier |= MOD_ALT;
      } else if (my_stricmp(token, U("shift")) == 0) {
        modifier |= MOD_SHIFT;
      } else if (my_stricmp(token, U("win")) == 0) {
        modifier |= MOD_WIN;
      } else {
        key = GetKeyCodeFromMapping(token);
      }
      token = (MCHAR*) my_strtok(NULL, U("+"), &nextToken);
    }

    HotkeyAction newHotkey;
    newHotkey.modifier = modifier;
    newHotkey.key = key;
    size_t actionLength = my_strlen(action);
    newHotkey.action = (MCHAR *)malloc((actionLength + 1) * sizeof(MCHAR));
    my_strncpy(newHotkey.action, actionLength + 1, action, actionLength);

    hotkeyActions = realloc(hotkeyActions, (numHotkeys + 1) * sizeof(HotkeyAction));
    hotkeyActions[numHotkeys++] = newHotkey;

    my_sscanf(line, U("%[^=]=%[^\n]"), hotkeyStr, action);
    if (!RegisterHotKey(g_hWnd, numHotkeys, modifier, key)) {
      my_strcat(g_ballonInfo, sizeof(g_ballonInfo), line);
    }

    my_snprintf(line, MAX_BUFF, U("%s=%s\r\n"), hotkeyStr, action);
    appendText(g_hwndEdit, line);
  }

  fclose(fp);
}
