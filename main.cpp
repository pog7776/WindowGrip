#define NOMINMAX
#include <windows.h>
#include <iostream>

HHOOK g_mouse_hook = NULL;
// Update your global variables
HWND g_target_hwnd = NULL;
RECT g_initial_rect = {0};
POINT g_initial_mouse = {0};
int g_center_x = 0;
int g_center_y = 0;
int g_offset_x = 0; // Caches original edge offset
int g_offset_y = 0; // Caches original edge offset
bool g_is_resizing = false;

const int minimum_width = 200;
const int minimum_height = 200;

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* mouse_data = (MSLLHOOKSTRUCT*)lParam;

        if (wParam == WM_LBUTTONDOWN) {
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {
                g_target_hwnd = WindowFromPoint(mouse_data->pt);
                if (g_target_hwnd) {
                    HWND parent = GetAncestor(g_target_hwnd, GA_ROOT);
                    if (parent) g_target_hwnd = parent;

                    if (GetWindowRect(g_target_hwnd, &g_initial_rect)) {
                        g_initial_mouse = mouse_data->pt;

                        int initial_width = g_initial_rect.right - g_initial_rect.left;
                        int initial_height = g_initial_rect.bottom - g_initial_rect.top;

                        g_center_x = g_initial_rect.left + (initial_width / 2);
                        g_center_y = g_initial_rect.top + (initial_height / 2);

                        // Calculate the delta between the mouse and the current half-size.
                        // This stops the window from snapping to the cursor position instantly.
                        g_offset_x = (initial_width / 2) - std::abs(g_initial_mouse.x - g_center_x);
                        g_offset_y = (initial_height / 2) - std::abs(g_initial_mouse.y - g_center_y);

                        // Fix focus: Bring the target window to the foreground immediately
                        SetForegroundWindow(g_target_hwnd);

                        g_is_resizing = true;
                        return 1; // Eat the click
                    }
                }
            }
        }

        if (g_is_resizing && wParam == WM_MOUSEMOVE) {
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {
                // Radial Calculation: Size is determined by absolute distance to the center pivot
                int current_width = (std::abs(mouse_data->pt.x - g_center_x) + g_offset_x) * 2;
                int current_height = (std::abs(mouse_data->pt.y - g_center_y) + g_offset_y) * 2;

                if (current_width < minimum_width)  current_width = minimum_width;
                if (current_height < minimum_height) current_height = minimum_height;

                int new_left = g_center_x - (current_width / 2);
                int new_top = g_center_y - (current_height / 2);

                SetWindowPos(g_target_hwnd, NULL,
                             new_left, new_top,
                             current_width, current_height,
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

                // CRITICAL FIX: Update the system cursor position manually so it updates visually,
                // but return 1 to block background windows from getting ghost drag-and-drop actions.
                SetCursorPos(mouse_data->pt.x, mouse_data->pt.y);
                return 1;
            } else {
                g_is_resizing = false;
            }
        }

        if (wParam == WM_LBUTTONUP) {
            if (g_is_resizing) {
                g_is_resizing = false;
                g_target_hwnd = NULL;
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