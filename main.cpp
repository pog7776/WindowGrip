#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <algorithm>

HHOOK g_mouse_hook = NULL;
HWND g_target_hwnd = NULL;
RECT g_initial_rect = {0};
POINT g_initial_mouse = {0};
int g_center_x = 0;
int g_center_y = 0;
bool g_is_resizing = false;

// Gracefully terminates the layout loop and tells the target window to commit its state
void EndResizeLoop() {
    if (g_is_resizing && g_target_hwnd) {
        // Inform the target window that the sizing loop has finished
        SendMessageW(g_target_hwnd, WM_EXITSIZEMOVE, 0, 0);
    }
    g_is_resizing = false;
    g_target_hwnd = NULL;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* mouse_data = (MSLLHOOKSTRUCT*)lParam;

        // 1. Intercept left-clicks
        if (wParam == WM_LBUTTONDOWN) {
            if (GetAsyncKeyState(VK_MENU) & 0x8000) { // ALT key
                HWND hit_window = WindowFromPoint(mouse_data->pt);
                if (hit_window) {
                    HWND parent = GetAncestor(hit_window, GA_ROOT);

                    // Filter out the desktop background and system tray/taskbar
                    if (parent && parent != GetDesktopWindow() && parent != GetShellWindow()) {
                        RECT rect;
                        if (GetWindowRect(parent, &rect)) {

                            // OPTIONAL EDGE-ONLY CHECK:
                            // If you want to limit the trigger to a 30px border edge, uncomment this:
                            /*
                            int edge_pad = 30;
                            bool near_edge = (mouse_data->pt.x < rect.left + edge_pad || mouse_data->pt.x > rect.right - edge_pad ||
                                              mouse_data->pt.y < rect.top + edge_pad || mouse_data->pt.y > rect.bottom - edge_pad);
                            if (!near_edge) return CallNextHookEx(g_mouse_hook, nCode, wParam, lParam);
                            */

                            g_target_hwnd = parent;
                            g_initial_rect = rect;
                            g_initial_mouse = mouse_data->pt;

                            int width = rect.right - rect.left;
                            int height = rect.bottom - rect.top;

                            g_center_x = rect.left + (width / 2);
                            g_center_y = rect.top + (height / 2);

                            g_is_resizing = true;

                            // CRITICAL FIX: Tell the application layout engine that a sizing session has started.
                            // This signals frameworks like Electron to cooperate with geometry changes.
                            SendMessageW(g_target_hwnd, WM_ENTERSIZEMOVE, 0, 0);

                            return 1; // Eat the click completely
                        }
                    }
                }
            }
        }

        // 2. Custom loop execution on mouse movement
        if (g_is_resizing && wParam == WM_MOUSEMOVE) {
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {
                int delta_x = mouse_data->pt.x - g_initial_mouse.x;
                int delta_y = mouse_data->pt.y - g_initial_mouse.y;

                int start_width = g_initial_rect.right - g_initial_rect.left;
                int start_height = g_initial_rect.bottom - g_initial_rect.top;

                int current_width = start_width + (delta_x * 2);
                int current_height = start_height + (delta_y * 2);

                // Enforce uniform square behavior with a safe minimum bounds clamp
                int target_size = (std::max)({current_width, current_height, 150});

                // Compute bounding box centered cleanly around our static center-pivot point
                RECT target_rect;
                target_rect.left = g_center_x - (target_size / 2);
                target_rect.top = g_center_y - (target_size / 2);
                target_rect.right = target_rect.left + target_size;
                target_rect.bottom = target_rect.top + target_size;

                // CRITICAL FIX: Feed the structural dimensions directly into the target app's procedure.
                // Windows marshals this system message natively so the target window caches the new layout size.
                SendMessageW(g_target_hwnd, WM_SIZING, WMSZ_BOTTOMRIGHT, (LPARAM)&target_rect);

                // SWP_FRAMECHANGED forces the target window frame/chrome to recalculate cleanly
                SetWindowPos(g_target_hwnd, NULL,
                             target_rect.left,
                             target_rect.top,
                             target_rect.right - target_rect.left,
                             target_rect.bottom - target_rect.top,
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

                return 1; // Eat the mouse movement
            } else {
                EndResizeLoop();
            }
        }

        // 3. Loop termination
        if (wParam == WM_LBUTTONUP) {
            if (g_is_resizing) {
                EndResizeLoop();
                return 1;
            }
        }
    }
    return CallNextHookEx(g_mouse_hook, nCode, wParam, lParam);
}

int main() {
    g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);

    if (!g_mouse_hook) {
        std::cerr << "Failed to install low-level mouse hook." << std::endl;
        return 1;
    }

    std::cout << "=========================================" << std::endl;
    std::cout << " Corrected Center-Pivot Resizer Online   " << std::endl;
    std::cout << " -> Sizing lifecycle synchronization active." << std::endl;
    std::cout << "=========================================" << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(g_mouse_hook);
    return 0;
}