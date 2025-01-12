#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

struct event_log_entry {
    Uint32 timestamp;  // 이벤트 발생 시간
    SDL_Event event;   // SDL 이벤트
};

struct event_logger {
    FILE *log_file;
    Uint32 start_time;
    bool is_recording;
};

bool event_logger_init(struct event_logger *logger, const char *filename);
void event_logger_record(struct event_logger *logger, const SDL_Event *event);
void event_logger_close(struct event_logger *logger);

bool event_replay_start(const char *filename);
void event_replay_process(void);

#endif 