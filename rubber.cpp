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
                        
                        g_is_resizing = true;
                        return 1; // Eat the click! Stops Windows from acting on it
                    }
                }
            }
        }

        // 2. Drive the resizing loop manually on mouse move
        if (g_is_resizing && wParam == WM_MOUSEMOVE) {
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {
                // Calculate distance dragged relative to the starting click point
                int delta_x = mouse_data->pt.x - g_initial_mouse.x;
                int delta_y = mouse_data->pt.y - g_initial_mouse.y;

                // Amplify drag vectors depending on which direction they pull
                int start_width = g_initial_rect.right - g_initial_rect.left;
                int start_height = g_initial_rect.bottom - g_initial_rect.top;

                // Adjust size based on relative displacement from the center pivot
                int current_width = start_width + (delta_x * 2);
                int current_height = start_height + (delta_y * 2);

                // Force strict 1:1 Aspect Ratio constraint
                int target_size = (std::max)({current_width, current_height, 150}); // Min clamp 150px

                int new_left = g_center_x - (target_size / 2);
                int new_top = g_center_y - (target_size / 2);

                // Update size manually. No flickering because Windows isn't fighting us.
                SetWindowPos(g_target_hwnd, NULL, new_left, new_top, target_size, target_size, 
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);

                return 1; // Eat the mouse movement
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