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
int g_scale_dir_x = 1;
int g_scale_dir_y = 1;
bool g_is_resizing = false;

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* mouse_data = (MSLLHOOKSTRUCT*)lParam;

        // 1. Intercept left-clicks anywhere on the screen
        if (wParam == WM_LBUTTONDOWN) {
            // Only trigger if the ALT key is held down
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {
                // Find exactly what window is under the mouse cursor right now
                g_target_hwnd = WindowFromPoint(mouse_data->pt);
                
                if (g_target_hwnd) {
                    // Get the top-level window if we accidentally grabbed an internal UI button
                    HWND parent = GetAncestor(g_target_hwnd, GA_ROOT);
                    if (parent) g_target_hwnd = parent;

                    if (GetWindowRect(g_target_hwnd, &g_initial_rect)) {
                        g_initial_mouse = mouse_data->pt;
                        
                        int width = g_initial_rect.right - g_initial_rect.left;
                        int height = g_initial_rect.bottom - g_initial_rect.top;
                        
                        // Lock down our absolute center-pivot point
                        g_center_x = g_initial_rect.left + (width / 2);
                        g_center_y = g_initial_rect.top + (height / 2);

                        // If clicking left of center, negate X delta. If right (or exactly center), keep it positive.
                        g_scale_dir_x = (g_initial_mouse.x >= g_center_x) ? 1 : -1;

                        // If clicking above center, negate Y delta. If below, keep it positive.
                        g_scale_dir_y = (g_initial_mouse.y >= g_center_y) ? 1 : -1;
                        
                        g_is_resizing = true;
                        return 1; // Eat the click! Stops Windows from acting on it
                    }
                }
            }
        }

        // 2. Drive the resizing loop manually on mouse move
        // 2. Drive the resizing loop manually on mouse move
        if (g_is_resizing && wParam == WM_MOUSEMOVE) {
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {
                // Calculate total displacement from the original click point
                int delta_x = mouse_data->pt.x - g_initial_mouse.x;
                int delta_y = mouse_data->pt.y - g_initial_mouse.y;

                // Use the STATIC original size cached during WM_LBUTTONDOWN
                int initial_width = g_initial_rect.right - g_initial_rect.left;
                int initial_height = g_initial_rect.bottom - g_initial_rect.top;

                // Calculate independent dimensions from the original base
                int current_width = initial_width + (delta_x * 2 * g_scale_dir_x);
                int current_height = initial_height + (delta_y * 2 * g_scale_dir_y);

                // Clamp to a safe minimum size so the window doesn't flip inside out
                if (current_width < 100)  current_width = 100;
                if (current_height < 100) current_height = 100;

                // Calculate center-pivoted placement
                int new_left = g_center_x - (current_width / 2);
                int new_top = g_center_y - (current_height / 2);

                // SWP_NOSENDCHANGING bypasses internal framework layout resistance
                SetWindowPos(g_target_hwnd, NULL,
                             new_left, new_top,
                             current_width, current_height,
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

                std::cout << "Current Left: " << new_left << " | Current Top: " << new_top << " | Current Width: " << current_width << " | Current Height: " << current_height << std::endl;

                // REMOVED: "return 1;"
                // Allowing this message to pass through lets the cursor move naturally on screen.
            } else {
                g_is_resizing = false;
            }
        }

        // 3. Clean up when the mouse releases
        if (wParam == WM_LBUTTONUP) {
            if (g_is_resizing) {
                g_is_resizing = false;
                g_target_hwnd = NULL;
                return 1; // Prevent the click release from clicking something random
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
    std::cout << " Seamless Center-Pivot Resizer Online    " << std::endl;
    std::cout << " -> Hold ALT and drag ANYWHERE inside a window." << std::endl;
    std::cout << " -> Released from the native Windows loop constraint." << std::endl;
    std::cout << "=========================================" << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(g_mouse_hook);
    return 0;
}