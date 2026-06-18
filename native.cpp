#define NOMINMAX
#include <windows.h>
#include <iostream>

HHOOK g_mouse_hook = NULL;
bool g_is_resizing = false;

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* mouse_data = (MSLLHOOKSTRUCT*)lParam;

        if (wParam == WM_LBUTTONDOWN) {
            if (GetAsyncKeyState(VK_MENU) & 0x8000) { // ALT key
                HWND hit_window = WindowFromPoint(mouse_data->pt);
                HWND top_window = GetAncestor(hit_window, GA_ROOT);

                if (top_window && top_window != GetDesktopWindow() && top_window != GetShellWindow()) {
                    g_is_resizing = true;
                    
                    // Bring the window to the foreground
                    SetForegroundWindow(top_window);
                    
                    // Tell the window natively that the user clicked the bottom-right corner 
                    // to start a standard, OS-level resize loop.
                    PostMessageW(top_window, WM_SYSCOMMAND, SC_SIZE + WMSZ_BOTTOMRIGHT, MAKELPARAM(mouse_data->pt.x, mouse_data->pt.y));
                    
                    return 1; // Swallow the click so the app doesn't process a normal click
                }
            }
        }

        if (wParam == WM_LBUTTONUP) {
            if (g_is_resizing) {
                g_is_resizing = false;
                return 1;
            }
        }
    }
    return CallNextHookEx(g_mouse_hook, nCode, wParam, lParam);
}

int main() {
    g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
    if (!g_mouse_hook) {
        std::cerr << "Hook failed." << std::endl;
        return 1;
    }

    std::cout << "Alt + Left Click anywhere on a window to native-resize..." << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(g_mouse_hook);
    return 0;
}