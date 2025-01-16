#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX_QUEUED_EVENTS 128
#define EVENT_BUFFER_SIZE 64

struct queued_event {
    SDL_Event event;
    Uint64 timestamp;  // 고해상도 타임스탬프
};

struct event_queue {
    struct queued_event events[MAX_QUEUED_EVENTS];
    int head;
    int tail;
    int count;
};

struct event_entry {
    Uint64 timestamp;
    char text[256];
};

struct event_buffer {
    struct event_entry entries[EVENT_BUFFER_SIZE];
    int count;
};

struct event_logger {
    FILE *log_file;
    Uint64 start_time;  // 고해상도 타임스탬프로 변경
    double time_scale;  // 성능 카운터를 초 단위로 변환하기 위한 스케일
    bool is_recording;
    struct event_buffer *buffer;
    int event_count;
    // 마지막 이벤트 정보 저장
    struct {
        Uint64 timestamp;
        int x;
        int y;
        int type;
        int code;
    } last_event;
    Uint64 last_timestamp;  // 추가: 마지막으로 기록된 타임스탬프
};

struct event_replayer {
    FILE *log_file;
    SDL_Window *window;
    Uint64 start_time;  // 고해상도 타임스탬프로 변경
    double time_scale;  // 성능 카운터를 초 단위로 변환하기 위한 스케일
    bool is_replaying;
    struct event_queue queue;  // 이벤트 큐 추가
};

bool event_logger_init(struct event_logger *logger, const char *filename);
void event_logger_record(struct event_logger *logger, const SDL_Event *event);
void event_logger_close(struct event_logger *logger);

bool event_replayer_init(struct event_replayer *replayer, const char *filename, SDL_Window *window);
bool event_replayer_process(struct event_replayer *replayer);
void event_replayer_close(struct event_replayer *replayer);

#endif