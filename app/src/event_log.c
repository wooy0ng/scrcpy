#include "event_log.h"
#include "util/log.h"
#include <string.h>

bool event_logger_init(struct event_logger *logger, const char *filename) {
    logger->log_file = fopen(filename, "w");
    if (!logger->log_file) {
        LOGE("Could not open event log file: %s", filename);
        return false;
    }
    logger->start_time = SDL_GetTicks();
    logger->is_recording = true;
    
    fprintf(logger->log_file, "# Scrcpy Event Log\n");
    fprintf(logger->log_file, "# Timestamp Type X Y KeyCode Modifiers\n");
    fprintf(logger->log_file, "# ----------------------------------------\n");
    return true;
}

void event_logger_record(struct event_logger *logger, const SDL_Event *event) {
    if (!logger->is_recording) return;
    
    Uint32 timestamp = SDL_GetTicks() - logger->start_time;
    
    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            fprintf(logger->log_file, "%u MOUSE_%s %d %d %d 0x%x\n",
                timestamp,
                event->type == SDL_MOUSEBUTTONDOWN ? "DOWN" : "UP",
                event->button.x,
                event->button.y,
                event->button.button,
                0);
            break;
            
        case SDL_MOUSEMOTION:
            fprintf(logger->log_file, "%u MOUSE_MOTION %d %d 0 0x%x\n",
                timestamp,
                event->motion.x,
                event->motion.y,
                0);
            break;
            
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            fprintf(logger->log_file, "%u KEY_%s %d %d %d 0x%x\n",
                timestamp,
                event->type == SDL_KEYDOWN ? "DOWN" : "UP",
                0,
                0,
                event->key.keysym.sym,
                event->key.keysym.mod);
            break;
            
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION: {
            int window_width, window_height;
            SDL_Window *window = SDL_GetWindowFromID(event->tfinger.windowID);
            SDL_GetWindowSize(window, &window_width, &window_height);
            
            int x = (int)(event->tfinger.x * window_width);
            int y = (int)(event->tfinger.y * window_height);
            
            fprintf(logger->log_file, "%u FINGER_%s %d %d %d 0x%x\n",
                timestamp,
                event->type == SDL_FINGERDOWN ? "DOWN" :
                event->type == SDL_FINGERUP ? "UP" : "MOTION",
                x,
                y,
                (int)event->tfinger.fingerId,
                0);
            break;
        }
    }
    
    fflush(logger->log_file);
}

void event_logger_close(struct event_logger *logger) {
    if (logger->log_file) {
        fprintf(logger->log_file, "# End of log\n");
        fclose(logger->log_file);
        logger->log_file = NULL;
    }
    logger->is_recording = false;
}

bool event_replayer_init(struct event_replayer *replayer, const char *filename, SDL_Window *window) {
    replayer->log_file = fopen(filename, "r");
    if (!replayer->log_file) {
        LOGE("Could not open event log file: %s", filename);
        return false;
    }
    
    replayer->window = window;
    replayer->start_time = SDL_GetTicks();
    replayer->is_replaying = true;

    // 헤더 라인들 건너뛰기
    char line[256];
    for (int i = 0; i < 3; i++) {
        if (!fgets(line, sizeof(line), replayer->log_file)) {
            LOGE("Invalid log file format");
            fclose(replayer->log_file);
            return false;
        }
    }
    
    return true;
}

static void inject_event(SDL_Window *window, const char *type, 
                        int x, int y, int code, int modifiers) {
    SDL_Event event;
    
    // 현재 윈도우 크기 가져오기 (좌표 스케일링을 위해)
    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    
    if (strcmp(type, "MOUSE_MOTION") == 0) {
        event.type = SDL_MOUSEMOTION;
        event.motion.x = x;
        event.motion.y = y;
        event.motion.xrel = 0;
        event.motion.yrel = 0;
        event.motion.state = 0;
    }
    else if (strcmp(type, "MOUSE_DOWN") == 0) {
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = code;
        event.button.x = x;
        event.button.y = y;
        event.button.clicks = 1;
    }
    else if (strcmp(type, "MOUSE_UP") == 0) {
        event.type = SDL_MOUSEBUTTONUP;
        event.button.button = code;
        event.button.x = x;
        event.button.y = y;
        event.button.clicks = 1;
    }
    else if (strcmp(type, "KEY_DOWN") == 0) {
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = code;
        event.key.keysym.mod = modifiers;
    }
    else if (strcmp(type, "KEY_UP") == 0) {
        event.type = SDL_KEYUP;
        event.key.keysym.sym = code;
        event.key.keysym.mod = modifiers;
    }
    
    SDL_PushEvent(&event);
}

bool event_replayer_process(struct event_replayer *replayer) {
    if (!replayer->is_replaying) {
        return false;
    }
    
    char line[256];
    if (!fgets(line, sizeof(line), replayer->log_file)) {
        // 파일 끝에 도달
        replayer->is_replaying = false;
        return false;
    }
    
    // 주석 라인 무시
    if (line[0] == '#') {
        return true;
    }
    
    Uint32 timestamp;
    char type[32];
    int x, y, code, modifiers;
    
    if (sscanf(line, "%u %s %d %d %d %x", 
               &timestamp, type, &x, &y, &code, &modifiers) != 6) {
        LOGW("Invalid log line format: %s", line);
        return true;
    }
    
    // 타임스탬프에 맞춰 대기
    Uint32 current_time = SDL_GetTicks() - replayer->start_time;
    if (current_time < timestamp) {
        SDL_Delay(timestamp - current_time);
    }
    
    // 이벤트 주입
    inject_event(replayer->window, type, x, y, code, modifiers);
    
    return true;
}

void event_replayer_close(struct event_replayer *replayer) {
    if (replayer->log_file) {
        fclose(replayer->log_file);
        replayer->log_file = NULL;
    }
    replayer->is_replaying = false;
}