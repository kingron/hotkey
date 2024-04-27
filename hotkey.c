// #define UNICODE

#pragma comment(linker, "/SUBSYSTEM:windows")
#pragma comment(lib, "user32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "psapi")

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

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
#define Shell_NotifyIcon Shell_NotifyIconW
#define ShellExecute ShellExecuteW
#define my_fopen _wfopen
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
#define Shell_NotifyIcon Shell_NotifyIconA
#define ShellExecute ShellExecuteA
#define my_fopen fopen
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

HWND g_hWnd;
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
    MessageBox(GetForegroundWindow(), U("Too many hide windows"), U("Error"), MB_ICONERROR | MB_OK);
    return FALSE;
  }
}

void* pop(Stack* stack) {
  if (!isEmpty(stack)) {
    void* item = stack->items[stack->top];
    stack->top--;
    return item;
  } else {
    MessageBox(GetForegroundWindow(), U("No hidden window"), U("Error"), MB_ICONERROR | MB_OK);
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

#ifdef UNICODE
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
  HANDLE hMutex = CreateMutex(NULL, TRUE, U("llds_hotkey"));
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(hMutex);
    return 0;
  }

  init(&stack);
  g_hWnd = CreateWindowEx(0, U("STATIC"), U("Hotkey"), WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, NULL, NULL);
  if (g_hWnd == NULL) {
    return 1;
  }

    SetWindowLong(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
    SetWindowLong(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

  SetWindowLong(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  int index = wParam - 1;
  switch (message) {
  case WM_USER + 1:
    if (lParam == WM_LBUTTONDBLCLK) {
      PostQuitMessage(0);
    }
    break;
  case WM_HOTKEY:
    if (index >=0 && index < numHotkeys) {
      if (my_stricmp(hotkeyActions[index].action, U("Minimize")) == 0) {
        ShowWindow(GetForegroundWindow(), SW_MINIMIZE);
      } else if (my_stricmp(hotkeyActions[index].action, U("Close")) == 0) {
        SendMessage(GetForegroundWindow(), WM_CLOSE, 0, 0);
      } else if (my_stricmp(hotkeyActions[index].action, U("Hide")) == 0) {
        HWND hwndActive = GetForegroundWindow();
        if (push(&stack, hwndActive)) {
          ShowWindow(hwndActive, SW_HIDE);
        }
      } else if (my_stricmp(hotkeyActions[index].action, U("Unhide")) == 0) {
        HWND hwndLast = (HWND) pop(&stack);
        if (hwndLast != NULL) {
          ShowWindow(hwndLast, SW_SHOW);
          SetForegroundWindow(hwndLast);
        }
      } else if (my_stricmp(hotkeyActions[index].action, U("Maximize")) == 0) {
        ShowWindow(GetForegroundWindow(), SW_MAXIMIZE);
      } else if (my_stricmp(hotkeyActions[index].action, U("OnTop")) == 0) {
        ToggleTopmost(GetForegroundWindow());
      } else {
        MCHAR cmd[MAX_BUFF] = {0};
        MCHAR parameter[MAX_BUFF] = {0};
        extractCmdAndParameter(hotkeyActions[index].action, cmd, parameter);
        OutputDebugString(cmd);
        OutputDebugString(parameter);
        ShellExecute(NULL, U("open"), cmd, parameter, NULL, SW_SHOWNORMAL);
        EmptyWorkingSet(GetCurrentProcess());
      }
    }
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
  FILE *fp = my_fopen(U("config.txt"), U("r"));
  if (fp == NULL) {
    return;
  }

  MCHAR line[MAX_BUFF];
  MCHAR hotkeyStr[32];
  MCHAR action[MAX_BUFF - 32];
  // while (my_fgets(line, sizeof(line), fp) != NULL) {
  while (fgetws(line, sizeof(line), fp) != NULL) {
    MCHAR *p = line;
    while ((p < line + sizeof(line) - 1) && (*p == ' ' || *p == '\t')) {
      p++;
    }
    if (*p == '#') {
      continue;
    }

    if (my_sscanf(line, U("%[^=]=%[^\n]"), hotkeyStr, action) != 2) {
      continue;
    }

    UINT modifier = 0;
    UINT key = 0;
    MCHAR *token;
    MCHAR *nextToken;
    token = my_strtok(hotkeyStr, U("+"), &nextToken);
    while (token != NULL) {
      if (my_stricmp(token, U("ctrl")) == 0) {
        modifier |= MOD_CONTROL;
      } else if (my_stricmp(token, U("alt")) == 0) {
        modifier |= MOD_ALT;
      } else if (my_stricmp(token, U("shift")) == 0) {
        modifier |= MOD_SHIFT;
      } else if (_stricmp(token, U("win")) == 0) {
        modifier |= MOD_WIN;
      } else {
        key = GetKeyCodeFromMapping(token);
      }
      token = my_strtok(NULL, U("+"), &nextToken);
    }

    HotkeyAction newHotkey;
    newHotkey.modifier = modifier;
    newHotkey.key = key;
    size_t actionLength = my_strlen(action);
    newHotkey.action = (MCHAR *)malloc((actionLength + 1) * sizeof(MCHAR));
    my_strncpy(newHotkey.action, actionLength + 1, action, actionLength);
    OutputDebugString(line);

    hotkeyActions = realloc(hotkeyActions, (numHotkeys + 1) * sizeof(HotkeyAction));
    hotkeyActions[numHotkeys++] = newHotkey;

    if (!RegisterHotKey(g_hWnd, numHotkeys, modifier, key)) {
      my_sscanf(line, U("%[^=]=%[^\n]"), hotkeyStr, action);
      my_strcat(g_ballonInfo, sizeof(g_ballonInfo), line);
    }
  }

  fclose(fp);
}
