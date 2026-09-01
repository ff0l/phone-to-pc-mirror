#include "window.h"

#include "firewall.h"
#include "icon.h"
#include "receiver.h"
#include "resource.h"
#include "tokens.h"

#include <dwmapi.h>
#include <string.h>
#include <strsafe.h>
#include <windowsx.h>

enum AppPhase {
    PhaseStarting = 0,
    PhaseWaiting = 1,
    PhaseConnected = 2
};

static const DWORD kFrameStyle = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;

struct App {
    HWND frame;
    HWND video;
    HWND close;
    Receiver *rx;
    AppPhase phase;
    FirewallStatus firewall;
    bool fullscreen;
    RECT windowed;
    wchar_t airplay_name[256];
    wchar_t client_name[256];
    wchar_t status[256];
    HFONT font_title;
    HFONT font_body;
    HBRUSH brush_bg;
    int stream_w;
    int stream_h;
};

static App g_app;

static void apply_chrome(HWND hwnd) {
    DWM_WINDOW_CORNER_PREFERENCE corner;
    COLORREF chrome;
    BOOL dark;
    DWM_SYSTEMBACKDROP_TYPE backdrop;
    corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    chrome = tokens::background;
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &chrome, sizeof(chrome));
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &chrome, sizeof(chrome));
    dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    backdrop = DWMSBT_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}

static LRESULT hit_frame(HWND hwnd, LPARAM lparam) {
    POINT pt;
    RECT rc;
    int x;
    int y;
    pt.x = GET_X_LPARAM(lparam);
    pt.y = GET_Y_LPARAM(lparam);
    ScreenToClient(hwnd, &pt);
    GetClientRect(hwnd, &rc);
    x = pt.x;
    y = pt.y;
    if (g_app.fullscreen) {
        return HTCLIENT;
    }
    if (x < tokens::edge && y < tokens::edge) {
        return HTTOPLEFT;
    }
    if (x >= rc.right - tokens::edge && y < tokens::edge) {
        return HTTOPRIGHT;
    }
    if (x < tokens::edge && y >= rc.bottom - tokens::edge) {
        return HTBOTTOMLEFT;
    }
    if (x >= rc.right - tokens::edge && y >= rc.bottom - tokens::edge) {
        return HTBOTTOMRIGHT;
    }
    if (x < tokens::edge) {
        return HTLEFT;
    }
    if (x >= rc.right - tokens::edge) {
        return HTRIGHT;
    }
    if (y < tokens::edge) {
        return HTTOP;
    }
    if (y >= rc.bottom - tokens::edge) {
        return HTBOTTOM;
    }
    if (y < tokens::drag) {
        return HTCAPTION;
    }
    return HTCLIENT;
}

static void set_status(const wchar_t *text) {
    wcsncpy(g_app.status, text, sizeof(g_app.status) / sizeof(g_app.status[0]) - 1);
    g_app.status[sizeof(g_app.status) / sizeof(g_app.status[0]) - 1] = 0;
    if (g_app.video && g_app.phase != PhaseConnected) {
        InvalidateRect(g_app.video, NULL, TRUE);
    }
}

static bool g_close_hot = false;

static void paint_close(HWND hwnd) {
    RECT rc;
    BITMAPINFO bmi;
    void *bits;
    HDC screen;
    HDC mem;
    HBITMAP dib;
    HGDIOBJ old;
    HBITMAP mark;
    SIZE size;
    POINT src;
    BLENDFUNCTION blend;
    int w;
    int h;
    if (!hwnd) {
        return;
    }
    GetClientRect(hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        return;
    }
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    screen = GetDC(NULL);
    bits = NULL;
    dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    mem = CreateCompatibleDC(screen);
    old = SelectObject(mem, dib);
    if (bits) {
        memset(bits, 0, (size_t)w * (size_t)h * 4);
    }
    mark = g_close_hot ? IconXmarkOn() : IconXmark();
    if (mark) {
        HDC srcdc = CreateCompatibleDC(mem);
        HGDIOBJ olds = SelectObject(srcdc, mark);
        blend.BlendOp = AC_SRC_OVER;
        blend.BlendFlags = 0;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        AlphaBlend(mem, (w - tokens::close_icon) / 2, (h - tokens::close_icon) / 2, tokens::close_icon,
                   tokens::close_icon, srcdc, 0, 0, tokens::close_icon, tokens::close_icon, blend);
        SelectObject(srcdc, olds);
        DeleteDC(srcdc);
    }
    size.cx = w;
    size.cy = h;
    src.x = 0;
    src.y = 0;
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, screen, NULL, &size, mem, &src, 0, &blend, ULW_ALPHA);
    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
}

static void layout_chrome() {
    RECT rc;
    if (!g_app.frame || !g_app.video) {
        return;
    }
    GetClientRect(g_app.frame, &rc);
    MoveWindow(g_app.video, 0, 0, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    if (g_app.close) {
        MoveWindow(g_app.close, rc.right - tokens::close, 0, tokens::close, tokens::drag, TRUE);
        ShowWindow(g_app.close, g_app.fullscreen ? SW_HIDE : SW_SHOW);
        SetWindowPos(g_app.close, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        paint_close(g_app.close);
    }
}

static void fit_window_to_stream(int sw, int sh) {
    RECT wr;
    MONITORINFO mi;
    int cur_w;
    int cur_h;
    int long_edge;
    int nw;
    int nh;
    int work_w;
    int work_h;
    if (!g_app.frame || g_app.fullscreen || sw <= 0 || sh <= 0) {
        return;
    }
    if (sw == g_app.stream_w && sh == g_app.stream_h) {
        return;
    }
    g_app.stream_w = sw;
    g_app.stream_h = sh;
    GetWindowRect(g_app.frame, &wr);
    cur_w = wr.right - wr.left;
    cur_h = wr.bottom - wr.top;
    long_edge = cur_w > cur_h ? cur_w : cur_h;
    if (long_edge < 240) {
        long_edge = 880;
    }
    if (sh >= sw) {
        nh = long_edge;
        nw = (int)((long long)nh * sw / sh);
    } else {
        nw = long_edge;
        nh = (int)((long long)nw * sh / sw);
    }
    if (nw < 160) {
        nw = 160;
        nh = (int)((long long)nw * sh / sw);
    }
    if (nh < 160) {
        nh = 160;
        nw = (int)((long long)nh * sw / sh);
    }
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(g_app.frame, MONITOR_DEFAULTTONEAREST), &mi);
    work_w = mi.rcWork.right - mi.rcWork.left;
    work_h = mi.rcWork.bottom - mi.rcWork.top;
    if (nw > work_w || nh > work_h) {
        if ((long long)work_w * sh > (long long)work_h * sw) {
            nh = work_h;
            nw = (int)((long long)nh * sw / sh);
        } else {
            nw = work_w;
            nh = (int)((long long)nw * sh / sw);
        }
    }
    SetWindowPos(g_app.frame, NULL, 0, 0, nw, nh, SWP_NOMOVE | SWP_NOZORDER);
}

static void enter_fullscreen() {
    MONITORINFO mi;
    if (g_app.fullscreen) {
        return;
    }
    GetWindowRect(g_app.frame, &g_app.windowed);
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(g_app.frame, MONITOR_DEFAULTTONEAREST), &mi);
    SetWindowLongW(g_app.frame, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
    SetWindowPos(g_app.frame, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_FRAMECHANGED);
    g_app.fullscreen = true;
    layout_chrome();
}

static void leave_fullscreen() {
    int w;
    int h;
    if (!g_app.fullscreen) {
        return;
    }
    w = g_app.windowed.right - g_app.windowed.left;
    h = g_app.windowed.bottom - g_app.windowed.top;
    SetWindowLongW(g_app.frame, GWL_STYLE, kFrameStyle | WS_VISIBLE);
    SetWindowPos(g_app.frame, HWND_TOP, g_app.windowed.left, g_app.windowed.top, w, h, SWP_FRAMECHANGED);
    apply_chrome(g_app.frame);
    g_app.fullscreen = false;
    layout_chrome();
}

static void paint_idle(HDC hdc, RECT rc) {
    const wchar_t *title = L"Waiting for iPhone";
    const wchar_t *hint = L"Control Center  ·  Screen Mirroring";
    RECT r;
    HFONT old;
    SetBkMode(hdc, TRANSPARENT);
    FillRect(hdc, &rc, g_app.brush_bg);
    r = rc;
    r.top = rc.bottom / 2 - 48;
    old = (HFONT)SelectObject(hdc, g_app.font_title);
    SetTextColor(hdc, tokens::text_primary);
    DrawTextW(hdc, title, -1, &r, DT_CENTER | DT_SINGLELINE);
    r.top += 36;
    SelectObject(hdc, g_app.font_body);
    SetTextColor(hdc, tokens::accent);
    DrawTextW(hdc, g_app.airplay_name, -1, &r, DT_CENTER | DT_SINGLELINE);
    r.top += 28;
    SetTextColor(hdc, tokens::text_secondary);
    DrawTextW(hdc, hint, -1, &r, DT_CENTER | DT_SINGLELINE);
    r.top += 22;
    DrawTextW(hdc, g_app.status, -1, &r, DT_CENTER | DT_SINGLELINE);
    if (g_app.firewall == FirewallDenied) {
        r.top += 22;
        SetTextColor(hdc, tokens::error);
        DrawTextW(hdc, L"Firewall blocked inbound traffic. Allow Mirror, then retry.", -1, &r,
                  DT_CENTER | DT_SINGLELINE);
    }
    SelectObject(hdc, old);
}

static LRESULT CALLBACK close_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_MOUSEMOVE) {
        if (!g_close_hot) {
            TRACKMOUSEEVENT tme;
            g_close_hot = true;
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            tme.dwHoverTime = 0;
            TrackMouseEvent(&tme);
            paint_close(hwnd);
        }
        return 0;
    }
    if (msg == WM_MOUSELEAVE) {
        g_close_hot = false;
        paint_close(hwnd);
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_LBUTTONUP) {
        DestroyWindow(g_app.frame);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static LRESULT CALLBACK video_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCHITTEST) {
        POINT pt;
        RECT rc;
        pt.x = GET_X_LPARAM(lparam);
        pt.y = GET_Y_LPARAM(lparam);
        ScreenToClient(hwnd, &pt);
        GetClientRect(hwnd, &rc);
        if (!g_app.fullscreen) {
            if (pt.y < tokens::drag) {
                return HTTRANSPARENT;
            }
            if (pt.x < tokens::edge || pt.y < tokens::edge || pt.x >= rc.right - tokens::edge ||
                pt.y >= rc.bottom - tokens::edge) {
                return HTTRANSPARENT;
            }
        }
        return HTCLIENT;
    }
    if (msg == WM_LBUTTONDOWN && !g_app.fullscreen) {
        SendMessageW(g_app.frame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        if (g_app.phase == PhaseConnected) {
            return 1;
        }
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wparam, &rc, g_app.brush_bg);
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_app.phase != PhaseConnected) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            paint_idle(hdc, rc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static LRESULT CALLBACK frame_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        apply_chrome(hwnd);
        return 0;
    case WM_NCCALCSIZE:
        if (wparam) {
            return 0;
        }
        break;
    case WM_NCHITTEST:
        return hit_frame(hwnd, lparam);
    case WM_SIZE:
        layout_chrome();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wparam == VK_F11) {
            if (g_app.fullscreen) {
                leave_fullscreen();
            } else {
                enter_fullscreen();
            }
            return 0;
        }
        if (wparam == VK_ESCAPE && g_app.fullscreen) {
            leave_fullscreen();
            return 0;
        }
        return 0;
    case WM_MIRROR_READY:
        g_app.phase = PhaseWaiting;
        if (lparam) {
            wcsncpy(g_app.airplay_name, (const wchar_t *)lparam,
                    sizeof(g_app.airplay_name) / sizeof(g_app.airplay_name[0]) - 1);
            g_app.airplay_name[sizeof(g_app.airplay_name) / sizeof(g_app.airplay_name[0]) - 1] = 0;
            LocalFree((HLOCAL)lparam);
        }
        set_status(L"Ready on this Wi-Fi");
        ShowWindow(g_app.video, SW_SHOW);
        return 0;
    case WM_MIRROR_CLIENT:
        g_app.phase = PhaseConnected;
        if (lparam) {
            wcsncpy(g_app.client_name, (const wchar_t *)lparam,
                    sizeof(g_app.client_name) / sizeof(g_app.client_name[0]) - 1);
            g_app.client_name[sizeof(g_app.client_name) / sizeof(g_app.client_name[0]) - 1] = 0;
            LocalFree((HLOCAL)lparam);
        }
        return 0;
    case WM_MIRROR_SIZE:
        fit_window_to_stream((int)wparam, (int)lparam);
        return 0;
    case WM_MIRROR_IDLE:
        if (lparam) {
            LocalFree((HLOCAL)lparam);
        }
        g_app.phase = PhaseWaiting;
        g_app.client_name[0] = 0;
        g_app.stream_w = 0;
        g_app.stream_h = 0;
        set_status(L"Ready on this Wi-Fi");
        return 0;
    case WM_DESTROY:
        if (g_app.rx) {
            ReceiverStop(g_app.rx);
            ReceiverDestroy(g_app.rx);
            g_app.rx = NULL;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void setup_runtime_paths() {
    wchar_t exe[MAX_PATH];
    wchar_t dir[MAX_PATH];
    wchar_t plugins[MAX_PATH];
    wchar_t path[4096];
    wchar_t *slash;
    DWORD n;
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wcsncpy(dir, exe, MAX_PATH);
    dir[MAX_PATH - 1] = 0;
    slash = wcsrchr(dir, L'\\');
    if (slash) {
        *slash = 0;
    }
    StringCchPrintfW(plugins, MAX_PATH, L"%s\\lib\\gstreamer-1.0", dir);
    if (GetFileAttributesW(plugins) != INVALID_FILE_ATTRIBUTES) {
        SetEnvironmentVariableW(L"GST_PLUGIN_PATH", plugins);
        SetEnvironmentVariableW(L"GST_PLUGIN_SYSTEM_PATH_1_0", L"");
    }
    n = GetEnvironmentVariableW(L"PATH", path, 4096);
    if (n > 0 && n < 4000) {
        wchar_t combined[4096];
        StringCchPrintfW(combined, 4096, L"%s;%s", dir, path);
        SetEnvironmentVariableW(L"PATH", combined);
    } else {
        SetEnvironmentVariableW(L"PATH", dir);
    }
}

int AppRun(HINSTANCE instance) {
    WNDCLASSEXW frame_class;
    WNDCLASSEXW video_class;
    WNDCLASSEXW close_class;
    DWORD n;
    MSG msg;
    wchar_t exe[MAX_PATH];
    setup_runtime_paths();
    SetEnvironmentVariableA("LANG", "en-US");
    IconInit();

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ZeroMemory(&g_app, sizeof(g_app));
    n = (DWORD)(sizeof(g_app.airplay_name) / sizeof(g_app.airplay_name[0]));
    if (!GetComputerNameW(g_app.airplay_name, &n) || !g_app.airplay_name[0]) {
        wcscpy(g_app.airplay_name, L"Mirror");
    }
    g_app.phase = PhaseStarting;
    g_app.font_title = CreateFontW(-tokens::type_title, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_app.font_body = CreateFontW(-tokens::type_body, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_app.brush_bg = CreateSolidBrush(tokens::background);

    ZeroMemory(&frame_class, sizeof(frame_class));
    frame_class.cbSize = sizeof(frame_class);
    frame_class.style = CS_HREDRAW | CS_VREDRAW;
    frame_class.lpfnWndProc = frame_proc;
    frame_class.hInstance = instance;
    frame_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    frame_class.hIconSm = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    frame_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    frame_class.hbrBackground = g_app.brush_bg;
    frame_class.lpszClassName = L"MirrorFrame";
    RegisterClassExW(&frame_class);

    ZeroMemory(&video_class, sizeof(video_class));
    video_class.cbSize = sizeof(video_class);
    video_class.style = CS_HREDRAW | CS_VREDRAW;
    video_class.lpfnWndProc = video_proc;
    video_class.hInstance = instance;
    video_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    video_class.hbrBackground = g_app.brush_bg;
    video_class.lpszClassName = L"MirrorVideo";
    RegisterClassExW(&video_class);

    ZeroMemory(&close_class, sizeof(close_class));
    close_class.cbSize = sizeof(close_class);
    close_class.lpfnWndProc = close_proc;
    close_class.hInstance = instance;
    close_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    close_class.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    close_class.lpszClassName = L"MirrorClose";
    RegisterClassExW(&close_class);

    g_app.frame = CreateWindowExW(WS_EX_APPWINDOW, L"MirrorFrame", L"Mirror", kFrameStyle, CW_USEDEFAULT,
                                  CW_USEDEFAULT, 406, 880, NULL, NULL, instance, NULL);
    g_app.video = CreateWindowExW(0, L"MirrorVideo", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 406, 880,
                                  g_app.frame, NULL, instance, NULL);
    g_app.close = CreateWindowExW(WS_EX_LAYERED, L"MirrorClose", L"", WS_CHILD | WS_VISIBLE, 370, 0, tokens::close,
                                  tokens::drag, g_app.frame, NULL, instance, NULL);
    apply_chrome(g_app.frame);
    layout_chrome();
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    g_app.firewall = FirewallEnsureInbound(exe);
    g_app.rx = ReceiverCreate(g_app.frame);
    set_status(L"Starting receiver");
    if (!g_app.rx || !ReceiverStart(g_app.rx, g_app.video, g_app.airplay_name)) {
        set_status(L"Failed to start receiver");
    }
    ShowWindow(g_app.frame, SW_SHOW);
    UpdateWindow(g_app.frame);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_app.font_title);
    DeleteObject(g_app.font_body);
    DeleteObject(g_app.brush_bg);
    IconShutdown();
    return (int)msg.wParam;
}
