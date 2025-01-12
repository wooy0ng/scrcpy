#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

// 이벤트 타입 열거형 추가
enum event_type {
    EVENT_MOUSE_MOTION,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_FINGER_DOWN,
    EVENT_FINGER_UP,
    EVENT_FINGER_MOTION
};

struct event_log_entry {
    Uint32 timestamp;
    enum event_type type;
    int x;
    int y;
    int code;      // button, key, or finger id
    int modifiers;
};

struct event_logger {
    FILE *log_file;
    Uint32 start_time;
    bool is_recording;
};

struct event_replayer {
    FILE *log_file;
    Uint32 start_time;
    bool is_replaying;
    SDL_Window *window;  // 좌표 변환을 위해 필요
};

// 기존 함수들
bool event_logger_init(struct event_logger *logger, const char *filename);
void event_logger_record(struct event_logger *logger, const SDL_Event *event);
void event_logger_close(struct event_logger *logger);

// 재생 관련 함수들 추가
bool event_replayer_init(struct event_replayer *replayer, const char *filename, SDL_Window *window);
bool event_replayer_process(struct event_replayer *replayer);
void event_replayer_close(struct event_replayer *replayer);

#endif