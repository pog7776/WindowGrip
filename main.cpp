#define NOMINMAX
#include <windows.h>
#include <iostream>

// --- Global State Registry ---
HHOOK g_mouse_hook    = NULL;
HWND  g_target_hwnd   = NULL;
RECT  g_initial_rect  = {0};
POINT g_initial_mouse = {0};
int   g_center_x      = 0;
int   g_center_y      = 0;
int   g_offset_x      = 0;
int   g_offset_y      = 0;
bool  g_is_resizing   = false;
bool  g_restrict_to_monitor = true;

const int minimum_width  = 200;
const int minimum_height = 200;

// --- Event Handlers ---

/**
 * Handles the initial Alt + Left-Click action.
 * Resolves the window under the cursor and caches the radial offset constraints.
 */
bool OnMouseButtonDown(MSLLHOOKSTRUCT* mouse_data) {
    // Only trigger if the ALT key is held down
    if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) {
        return false;
    }

    g_target_hwnd = WindowFromPoint(mouse_data->pt);
    if (!g_target_hwnd) return false;

    // Grab the root window frame if we accidentally targeted an internal UI element
    HWND parent = GetAncestor(g_target_hwnd, GA_ROOT);
    if (parent) g_target_hwnd = parent;

    // Instead of forcing a hard SW_RESTORE jump, we strip the maximized style flag in place.
    if (IsZoomed(g_target_hwnd)) {
        // 1. Get the current active style flags
        LONG_PTR style = GetWindowLongPtrW(g_target_hwnd, GWL_STYLE);

        // 2. Clear the WS_MAXIMIZE flag using a bitwise NOT mask
        SetWindowLongPtrW(g_target_hwnd, GWL_STYLE, style & ~WS_MAXIMIZE);

        // 3. Force Windows to re-evaluate frame caching metrics without moving it
        SetWindowPos(g_target_hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }

    if (!GetWindowRect(g_target_hwnd, &g_initial_rect)) {
        g_target_hwnd = NULL;
        return false;
    }

    g_initial_mouse = mouse_data->pt;

    int initial_width  = g_initial_rect.right - g_initial_rect.left;
    int initial_height = g_initial_rect.bottom - g_initial_rect.top;

    g_center_x = g_initial_rect.left + (initial_width / 2);
    g_center_y = g_initial_rect.top + (initial_height / 2);

    // Calculate the delta between the mouse and the current half-size.
    g_offset_x = (initial_width / 2) - std::abs(g_initial_mouse.x - g_center_x);
    g_offset_y = (initial_height / 2) - std::abs(g_initial_mouse.y - g_center_y);

    // Focus Management: Bring the target window to the foreground immediately
    SetForegroundWindow(g_target_hwnd);

    g_is_resizing = true;
    return true; // Eat the click event
}

/**
 * Manages the live drag-resize loop using radial calculation.
 * Forces mouse positioning to sync perfectly with window boundaries.
 */
bool OnMouseMove(MSLLHOOKSTRUCT* mouse_data) {
    if (!g_is_resizing) return false;

    if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) {
        g_is_resizing = false;
        return false;
    }

    // 1. Calculate base layout dimensions derived from mouse distance to center pivot
    int current_width  = (std::abs(mouse_data->pt.x - g_center_x) + g_offset_x) * 2;
    int current_height = (std::abs(mouse_data->pt.y - g_center_y) + g_offset_y) * 2;

    // 2. Apply monitor-boundary clamping if the toggle feature is active
    if (g_restrict_to_monitor) {
        // Fetch the handle of the monitor closest to our target window
        HMONITOR h_monitor = MonitorFromWindow(g_target_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitor_info = { sizeof(NOTIFYICONDATA) };
        monitor_info.cbSize = sizeof(MONITORINFO);

        if (GetMonitorInfoW(h_monitor, &monitor_info)) {
            // Calculate the total width/height of the physical monitor display panel
            int monitor_width  = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
            int monitor_height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;

            // Clamp the sizing limits to the monitor dimensions
            if (current_width > monitor_width)   current_width  = monitor_width;
            if (current_height > monitor_height) current_height = monitor_height;
        }
    }

    // 3. Apply safety minimum bounds
    if (current_width < minimum_width)   current_width  = minimum_width;
    if (current_height < minimum_height) current_height = minimum_height;

    int new_left = g_center_x - (current_width / 2);
    int new_top  = g_center_y - (current_height / 2);

    SetWindowPos(g_target_hwnd, NULL,
                 new_left, new_top,
                 current_width, current_height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

    SetCursorPos(mouse_data->pt.x, mouse_data->pt.y);
    return true;
}

/**
 * Cleans up tracking allocations when the mouse button is released.
 */
bool OnMouseButtonUp(MSLLHOOKSTRUCT* mouse_data) {
    if (g_is_resizing) {
        g_is_resizing = false;
        g_target_hwnd = NULL;
        return true; // Prevent the click release from falling through to background apps
    }
    return false;
}

// --- Windows Hook Message Router ---

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* mouse_data = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        switch (wParam) {
            case WM_LBUTTONDOWN:
                if (OnMouseButtonDown(mouse_data)) return 1;
                break;

            case WM_MOUSEMOVE:
                if (OnMouseMove(mouse_data)) return 1;
                break;

            case WM_LBUTTONUP:
                if (OnMouseButtonUp(mouse_data)) return 1;
                break;
        }
    }
    return CallNextHookEx(g_mouse_hook, nCode, wParam, lParam);
}

// --- Entry Point ---

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