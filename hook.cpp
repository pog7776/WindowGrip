#define NOMINMAX
#include <windows.h>
#include <algorithm>

// Shared memory across processes to track center coordinates
#pragma comment(linker, "/SECTION:.shared,RWS")
#pragma data_seg(".shared")
    HHOOK g_hHook = NULL;
    bool is_center_locked = false;
    int center_x = 0;
    int center_y = 0;
#pragma data_seg()

// Our custom window loop that intercepts the target window's private messages
LRESULT CALLBACK SubclassWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // Retrieve the original window procedure we tucked away inside the window's user data
    WNDPROC oldWndProc = (WNDPROC)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_SIZING) {
        if (GetAsyncKeyState(VK_MENU) & 0x8000) {
            LPRECT rect = (LPRECT)lparam;
            int width = rect->right - rect->left;
            int height = rect->bottom - rect->top;

            if (!is_center_locked) {
                center_x = rect->left + (width / 2);
                center_y = rect->top + (height / 2);
                is_center_locked = true;
            }

            int target_size = (std::max)(width, height);
            rect->left   = center_x - (target_size / 2);
            rect->right  = center_x + (target_size / 2);
            rect->top    = center_y - (target_size / 2);
            rect->bottom = center_y + (target_size / 2);

            return TRUE; // Tell Windows we handled the sizing constraints
        } else {
            is_center_locked = false;
        }
    }
    else if (msg == WM_EXITSIZEMOVE) {
        is_center_locked = false;
    }
    else if (msg == WM_NCDESTROY) {
        // Clean up and restore original window settings when the window closes
        if (oldWndProc) {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)oldWndProc);
        }
    }

    // Fall back to the app's original logic for everything else
    if (oldWndProc) {
        return CallWindowProcW(oldWndProc, hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// The core hook: Every time any window receives a message, our DLL hooks into its loop
extern "C" __declspec(dllexport) LRESULT CALLBACK MessageProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code >= 0) {
        MSG* msg = (MSG*)lparam;
        if (msg && msg->hwnd) {
            LONG_PTR currentWndProc = GetWindowLongPtrW(msg->hwnd, GWLP_WNDPROC);

            // If the window isn't already using our custom loop, swap it out
            if (currentWndProc != (LONG_PTR)SubclassWndProc) {
                // Save the old procedure inside GWLP_USERDATA so our Subclass can read it
                SetWindowLongPtrW(msg->hwnd, GWLP_USERDATA, currentWndProc);
                SetWindowLongPtrW(msg->hwnd, GWLP_WNDPROC, (LONG_PTR)SubclassWndProc);
            }
        }
    }
    return CallNextHookEx(g_hHook, code, wparam, lparam);
}