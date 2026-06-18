#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <algorithm>

// Global tracking variables
HWND g_tracked_hwnd = NULL;
int g_center_x = 0;
int g_center_y = 0;
bool g_is_tracking = false;

// The callback function that intercepts window adjustments safely
void CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime
) {
    // Only care about valid top-level windows dragging
    if (idObject != OBJID_WINDOW || hwnd == NULL) return;

    // Check if the user is holding the ALT key
    if ((GetAsyncKeyState(VK_MENU) & 0x8000) == 0) {
        g_is_tracking = false;
        g_tracked_hwnd = NULL;
        return;
    }

    if (event == EVENT_SYSTEM_MOVESIZESTART) {
        RECT rect;
        if (GetWindowRect(hwnd, &rect)) {
            g_tracked_hwnd = hwnd;
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            g_center_x = rect.left + (width / 2);
            g_center_y = rect.top + (height / 2);
            g_is_tracking = true;
        }
    }
    else if (event == EVENT_OBJECT_LOCATIONCHANGE && g_is_tracking && hwnd == g_tracked_hwnd) {
        RECT rect;
        if (GetWindowRect(hwnd, &rect)) {
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            int target_size = (std::max)(width, height);
            int new_left = g_center_x - (target_size / 2);
            int new_top = g_center_y - (target_size / 2);

            // Avoid updating if the window is already in the correct state
            if (rect.left == new_left && rect.top == new_top && width == target_size && height == target_size) {
                return;
            }

            // DeferWindowPos updates the window boundaries atomically in the Win32 kernel,
            // which breaks the recursive event feedback loop and prevents visual stuttering.
            HDWP hdwp = BeginDeferWindowPos(1);
            if (hdwp) {
                hdwp = DeferWindowPos(hdwp, hwnd, NULL, new_left, new_top, target_size, target_size, SWP_NOZORDER | SWP_NOACTIVATE);
                EndDeferWindowPos(hdwp);
            }
        }
    }
    else if (event == EVENT_SYSTEM_MOVESIZEEND) {
        g_is_tracking = false;
        g_tracked_hwnd = NULL;
    }
}

int main() {
    // Register event listeners for window adjustments safely from user space
    HWINEVENTHOOK hStart = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZESTART, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    HWINEVENTHOOK hChange = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    HWINEVENTHOOK hEnd = SetWinEventHook(EVENT_SYSTEM_MOVESIZEEND, EVENT_SYSTEM_MOVESIZEEND, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    if (!hStart || !hChange || !hEnd) {
        std::cerr << "Failed to initialize accessibility hooks." << std::endl;
        return 1;
    }

    std::cout << "=========================================" << std::endl;
    std::cout << " Safe Center-Pivot Resizer Online         " << std::endl;
    std::cout << " -> No DLL Injection, No System Freezes.  " << std::endl;
    std::cout << " -> Hold ALT while dragging a window edge." << std::endl;
    std::cout << "=========================================" << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWinEvent(hStart);
    UnhookWinEvent(hChange);
    UnhookWinEvent(hEnd);
    return 0;
}