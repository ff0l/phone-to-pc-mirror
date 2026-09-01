#include "receiver.h"

#include <glib.h>
#include "uxplay_api.h"
#include "video_renderer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct Receiver {
    HWND notify;
    HANDLE thread;
    HWND video;
    char name_utf8[256];
    volatile LONG stop_requested;
};

static wchar_t *utf8_to_wide(const char *text) {
    if (!text || !text[0]) {
        return NULL;
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (n <= 0) {
        return NULL;
    }
    wchar_t *out = (wchar_t *)LocalAlloc(LMEM_FIXED, (SIZE_T)n * sizeof(wchar_t));
    if (!out) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, out, n);
    return out;
}

static void on_uxplay_event(int event, const char *text, void *cls) {
    Receiver *rx = (Receiver *)cls;
    UINT msg = 0;
    if (event == UXPLAY_EVENT_READY) {
        msg = WM_MIRROR_READY;
    } else if (event == UXPLAY_EVENT_CLIENT) {
        msg = WM_MIRROR_CLIENT;
    } else if (event == UXPLAY_EVENT_IDLE) {
        msg = WM_MIRROR_IDLE;
    } else if (event == UXPLAY_EVENT_SIZE) {
        int w = 0;
        int h = 0;
        if (text && sscanf(text, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
            PostMessageW(rx->notify, WM_MIRROR_SIZE, (WPARAM)w, (LPARAM)h);
        }
        return;
    } else {
        return;
    }
    PostMessageW(rx->notify, msg, 0, (LPARAM)utf8_to_wide(text));
}

static DWORD WINAPI receiver_thread(void *param) {
    Receiver *rx = (Receiver *)param;
    char *argv[16];
    argv[0] = (char *)"Mirror";
    argv[1] = (char *)"-n";
    argv[2] = rx->name_utf8;
    argv[3] = (char *)"-nh";
    argv[4] = (char *)"-s";
    argv[5] = (char *)"1080x1920";
    argv[6] = (char *)"-fps";
    argv[7] = (char *)"60";
    argv[8] = (char *)"-vsync";
    argv[9] = (char *)"no";
    argv[10] = (char *)"-al";
    argv[11] = (char *)"0.04";
    argv[12] = (char *)"-vs";
    argv[13] = (char *)"d3d11videosink";
    argv[14] = (char *)"-as";
    argv[15] = (char *)"wasapi2sink low-latency=true provide-clock=false";
    uxplay_set_event_callback(on_uxplay_event, rx);
    if (rx->video) {
        video_renderer_set_window_handle((guintptr)rx->video);
    }
    start_uxplay(16, argv);
    return 0;
}

Receiver *ReceiverCreate(HWND notify) {
    Receiver *rx = (Receiver *)calloc(1, sizeof(Receiver));
    if (!rx) {
        return NULL;
    }
    rx->notify = notify;
    rx->thread = NULL;
    return rx;
}

void ReceiverDestroy(Receiver *rx) {
    if (!rx) {
        return;
    }
    ReceiverStop(rx);
    free(rx);
}

bool ReceiverStart(Receiver *rx, HWND video, const wchar_t *airplay_name) {
    if (!rx || rx->thread) {
        return false;
    }
    rx->video = video;
    rx->name_utf8[0] = 0;
    if (airplay_name && airplay_name[0]) {
        WideCharToMultiByte(CP_UTF8, 0, airplay_name, -1, rx->name_utf8, (int)sizeof(rx->name_utf8), NULL, NULL);
    }
    if (!rx->name_utf8[0]) {
        strncpy(rx->name_utf8, "Mirror", sizeof(rx->name_utf8) - 1);
    }
    rx->stop_requested = 0;
    rx->thread = CreateThread(NULL, 0, receiver_thread, rx, 0, NULL);
    return rx->thread != NULL;
}

void ReceiverStop(Receiver *rx) {
    if (!rx || !rx->thread) {
        return;
    }
    InterlockedExchange(&rx->stop_requested, 1);
    stop_uxplay();
    WaitForSingleObject(rx->thread, 8000);
    CloseHandle(rx->thread);
    rx->thread = NULL;
}
