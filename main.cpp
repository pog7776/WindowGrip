#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>

// --- Global State Registry ---
HHOOK g_mouse_hook          = NULL;
HWND  g_target_hwnd         = NULL;
RECT  g_initial_rect        = {0};
POINT g_initial_mouse       = {0};
int   g_center_x            = 0;
int   g_center_y            = 0;
int   g_offset_x            = 0;
int   g_offset_y            = 0;
bool  g_is_resizing         = false;

// --- Feature Configuration Toggles ---
bool g_restrict_to_monitor  = false; // Set to true to prevent windows from growing larger than their monitor
const int minimum_width     = 200;
const int minimum_height    = 200;

// --- Event Handlers ---

/**
 * Handles the initial Alt + Left-Click action.
 * Rejects core system shell frames and prepares seamless center-pivot metrics.
 */
bool OnMouseButtonDown(MSLLHOOKSTRUCT* mouse_data) {
    // Only trigger if the ALT key is held down
    if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) {
        return false;
    }

    HWND hit_hwnd = WindowFromPoint(mouse_data->pt);
    if (!hit_hwnd) return false;

    // --- SYSTEM SHELL SAFETY GUARD ---
    // Traverse up the window chain to verify this element doesn't belong to the taskbar or desktop layers
    HWND check_hwnd = hit_hwnd;
    while (check_hwnd != NULL) {
        wchar_t class_name[256];
        if (GetClassNameW(check_hwnd, class_name, 256)) {
            std::wstring cls(class_name);
            if (cls == L"Shell_TrayWnd"   || // Main Taskbar
                cls == L"SecondaryTrayWnd" || // Multi-monitor Taskbars
                cls == L"Progman"          || // Desktop Manager Frame
                cls == L"WorkerW"          || // Wallpaper / Shell Window Layer
                cls == L"SysListView32"    || // Desktop Folder/Icons view
                cls == L"DesktopWClass")      // Alternate Desktop Container
            {
                return false; // Core system window element detected! Ignore click.
            }
        }
        check_hwnd = GetParent(check_hwnd);
    }
    // ----------------------------------

    // Now safely resolve the top-level window frame for user interactions
    g_target_hwnd = GetAncestor(hit_hwnd, GA_ROOT);
    if (!g_target_hwnd) return false;

    // Seamless Fix for Maximized Windows (No sudden visual jumping/snapping)
    if (IsZoomed(g_target_hwnd)) {
        LONG_PTR style = GetWindowLongPtrW(g_target_hwnd, GWL_STYLE);
        SetWindowLongPtrW(g_target_hwnd, GWL_STYLE, style & ~WS_MAXIMIZE);
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

    // Dynamic offset calibration to prevent window jumping instantly to the cursor position
    g_offset_x = (initial_width / 2) - std::abs(g_initial_mouse.x - g_center_x);
    g_offset_y = (initial_height / 2) - std::abs(g_initial_mouse.y - g_center_y);

    // Focus Management: Bring target window to the foreground instantly
    SetForegroundWindow(g_target_hwnd);

    g_is_resizing = true;
    return true; // Eat the click
}

/**
 * Manages the live drag-resize loop using radial calculation.
 * Updates system cursor placement to smoothly match manual layout adjustments.
 */
bool OnMouseMove(MSLLHOOKSTRUCT* mouse_data) {
    if (!g_is_resizing) return false;

    // Gracefully drop resizing if the user releases the ALT key mid-drag
    if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) {
        g_is_resizing = false;
        return false;
    }

    // Radial Dimension Formulation
    int current_width  = (std::abs(mouse_data->pt.x - g_center_x) + g_offset_x) * 2;
    int current_height = (std::abs(mouse_data->pt.y - g_center_y) + g_offset_y) * 2;

    // Monitor Constraint Boundary Guard
    if (g_restrict_to_monitor) {
        HMONITOR h_monitor = MonitorFromWindow(g_target_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitor_info = { sizeof(MONITORINFO) };

        if (GetMonitorInfoW(h_monitor, &monitor_info)) {
            int monitor_width  = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
            int monitor_height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;

            if (current_width > monitor_width)   current_width  = monitor_width;
            if (current_height > monitor_height) current_height = monitor_height;
        }
    }

    // Safety Minimum Thresholds
    if (current_width < minimum_width)   current_width  = minimum_width;
    if (current_height < minimum_height) current_height = minimum_height;

    int new_left = g_center_x - (current_width / 2);
    int new_top  = g_center_y - (current_height / 2);

    SetWindowPos(g_target_hwnd, NULL,
                 new_left, new_top,
                 current_width, current_height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

    SetCursorPos(mouse_data->pt.x, mouse_data->pt.y);
    return true; // Keep mouse processing exclusive to avoid underlying background highlights
}

/**
 * Tends to clean up allocations when the mouse button is released.
 */
bool OnMouseButtonUp(MSLLHOOKSTRUCT* mouse_data) {
    if (g_is_resizing) {
        g_is_resizing = false;
        g_target_hwnd = NULL;
        return true;
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

    std::cout << "=====================================================" << std::endl;
    std::cout << " Seamless Center-Pivot Resizer Online                " << std::endl;
    std::cout << " -> Hold ALT and drag ANYWHERE inside a window.      " << std::endl;
    std::cout << " -> Taskbar & Desktop Background fully locked.        " << std::endl;
    std::cout << "=====================================================" << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(g_mouse_hook);
    return 0;
}