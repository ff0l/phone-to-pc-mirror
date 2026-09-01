#ifndef UXPLAY_API_H
#define UXPLAY_API_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    UXPLAY_EVENT_READY = 0,
    UXPLAY_EVENT_CLIENT = 1,
    UXPLAY_EVENT_IDLE = 2,
    UXPLAY_EVENT_SIZE = 3
};

typedef void (*uxplay_event_cb)(int event, const char *text, void *cls);

void uxplay_set_event_callback(uxplay_event_cb cb, void *cls);
int start_uxplay(int argc, char *argv[]);
void stop_uxplay(void);

#ifdef __cplusplus
}
#endif

#endif
