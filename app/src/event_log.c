#include "event_log.h"
#include "util/log.h"
#include <string.h>

#define EVENT_BUFFER_SIZE 64
#define FLUSH_THRESHOLD 32

// parse_and_create_event 함수 선언을 파일 상단에 추가
static bool parse_and_create_event(const char *type, int x, int y, 
                                 int code, int modifiers, SDL_Event *event);

static void init_time_scale(double *time_scale) {
    *time_scale = 1.0 / (double)SDL_GetPerformanceFrequency();
}

static Uint64 get_current_time_us(Uint64 start_time, double time_scale) {
    Uint64 now = SDL_GetPerformanceCounter();
    // 마이크로초 단위로 변환하고 반올림
    return (Uint64)((now - start_time) * time_scale * 1000000.0 + 0.5);
}

// 이벤트 중복 체크 함수 추가
static bool is_duplicate_event(struct event_logger *logger, 
                             Uint64 timestamp, 
                             const char *type,
                             int x, int y, 
                             int code) {
    // 20ms 이내의 동일한 위치의 동일한 타입 이벤트는 중복으로 처리
    if (timestamp - logger->last_event.timestamp < 20000 &&  // 20ms
        logger->last_event.x == x &&
        logger->last_event.y == y &&
        logger->last_event.code == code) {
        return true;
    }
    
    // 마지막 이벤트 정보 업데이트
    logger->last_event.timestamp = timestamp;
    logger->last_event.x = x;
    logger->last_event.y = y;
    logger->last_event.code = code;
    
    return false;
}

bool event_logger_init(struct event_logger *logger, const char *filename) {
    // 텍스트 모드로 파일 열기 ("w"가 아닌 "wt" 사용)
    logger->log_file = fopen(filename, "wt");
    if (!logger->log_file) {
        LOGE("Could not open event log file: %s", filename);
        return false;
    }
    
    // 라인 버퍼링 모드 설정
    setvbuf(logger->log_file, NULL, _IOLBF, BUFSIZ);
    
    init_time_scale(&logger->time_scale);
    logger->start_time = SDL_GetPerformanceCounter();
    logger->is_recording = true;
    logger->event_count = 0;
    logger->last_timestamp = 0;
    
    // 헤더 작성 (fprintf 대신 fputs 사용)
    static const char header[] = 
        "# Scrcpy Event Log\n"
        "# Timestamp Type X Y KeyCode Modifiers\n"
        "# ----------------------------------------\n";
    fputs(header, logger->log_file);
    fflush(logger->log_file);
    
    // 마지막 이벤트 정보 초기화
    memset(&logger->last_event, 0, sizeof(logger->last_event));
    logger->last_event.x = -1;
    logger->last_event.y = -1;
    logger->last_event.type = -1;
    logger->last_event.code = -1;
    
    return true;
}

void event_logger_record(struct event_logger *logger, const SDL_Event *event) {
    if (!logger->is_recording) return;
    
    Uint64 timestamp = get_current_time_us(logger->start_time, logger->time_scale);
    
    // 이전 타임스탬프보다 작은 경우 조정
    if (timestamp <= logger->last_timestamp) {
        timestamp = logger->last_timestamp + 1;
    }
    
    // 중복 이벤트 필터링 강화
    if (logger->event_count > 0) {
        if (event->type == SDL_MOUSEMOTION) {
            if (event->motion.x == logger->last_event.x &&
                event->motion.y == logger->last_event.y) {
                return;  // 동일한 위치의 마우스 이동은 무시
            }
            // 매우 작은 움직임도 필터링 (1픽셀 이하)
            int dx = abs(event->motion.x - logger->last_event.x);
            int dy = abs(event->motion.y - logger->last_event.y);
            if (dx <= 1 && dy <= 1 && 
                timestamp - logger->last_event.timestamp < 16000) { // 16ms
                return;
            }
        } else if (event->type == SDL_MOUSEBUTTONDOWN ||
                   event->type == SDL_MOUSEBUTTONUP) {
            // 마우스 버튼 이벤트는 중복 필터링 제거
            // 모든 DOWN/UP 이벤트를 반드시 기록해야 함
        } else if (event->type == SDL_KEYDOWN ||
                   event->type == SDL_KEYUP) {
            // 키보드 이벤트는 같은 키의 연속 입력만 필터링
            if (event->key.keysym.sym == logger->last_event.code &&
                timestamp - logger->last_event.timestamp < 16000) { // 16ms
                return;
            }
        }
    }
    
    char entry[256];
    int len = 0;
    
    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            len = snprintf(entry, sizeof(entry), 
                "%llu MOUSE_%s %d %d %d 0x%x\n",
                timestamp,
                event->type == SDL_MOUSEBUTTONDOWN ? "DOWN" : "UP",
                event->button.x, event->button.y,
                event->button.button, 
                event->button.state);  // 상태 정보도 기록
            
            LOGD("Recording mouse button event: type=%s, x=%d, y=%d, button=%d, state=%d",
                 event->type == SDL_MOUSEBUTTONDOWN ? "DOWN" : "UP",
                 event->button.x, event->button.y,
                 event->button.button,
                 event->button.state);
            break;
            
        case SDL_MOUSEMOTION:
            len = snprintf(entry, sizeof(entry), 
                "%llu MOUSE_MOTION %d %d 0 0x%x\n",
                timestamp,
                event->motion.x, event->motion.y, 0);
            break;
            
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            len = snprintf(entry, sizeof(entry), 
                "%llu KEY_%s %d %d %d 0x%x\n",
                timestamp,
                event->type == SDL_KEYDOWN ? "DOWN" : "UP",
                0,
                0,
                event->key.keysym.sym,
                event->key.keysym.mod);
            
            LOGD("Recording keyboard event: type=%s, sym=0x%x, scancode=%d, mod=0x%x",
                 event->type == SDL_KEYDOWN ? "KEY_DOWN" : "KEY_UP",
                 event->key.keysym.sym,
                 event->key.keysym.scancode,
                 event->key.keysym.mod);
            break;
            
        default:
            return;
    }
    
    if (len > 0 && len < sizeof(entry)) {
        fputs(entry, logger->log_file);
        fflush(logger->log_file);
        
        // 마지막 이벤트 정보 업데이트
        logger->last_event.timestamp = timestamp;
        logger->last_event.type = event->type;
        
        // 이벤트 타입에 따라 적절한 정보 저장
        if (event->type == SDL_MOUSEMOTION) {
            logger->last_event.x = event->motion.x;
            logger->last_event.y = event->motion.y;
            logger->last_event.code = 0;
        } else if (event->type == SDL_MOUSEBUTTONDOWN || 
                  event->type == SDL_MOUSEBUTTONUP) {
            logger->last_event.x = event->button.x;
            logger->last_event.y = event->button.y;
            logger->last_event.code = event->button.button;
        } else if (event->type == SDL_KEYDOWN || 
                  event->type == SDL_KEYUP) {
            logger->last_event.x = 0;
            logger->last_event.y = 0;
            logger->last_event.code = event->key.keysym.sym;
        }
        
        logger->last_timestamp = timestamp;
        logger->event_count++;
    }
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
    init_time_scale(&replayer->time_scale);
    replayer->start_time = SDL_GetPerformanceCounter();
    replayer->is_replaying = true;
    
    // 이벤트 큐 초기화
    replayer->queue.head = 0;
    replayer->queue.tail = 0;
    replayer->queue.count = 0;
    
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

static bool queue_is_full(struct event_queue *queue) {
    return queue->count >= MAX_QUEUED_EVENTS;
}

static bool queue_is_empty(struct event_queue *queue) {
    return queue->count == 0;
}

static void queue_push(struct event_queue *queue, SDL_Event *event, Uint64 timestamp) {
    if (queue_is_full(queue)) return;
    
    queue->events[queue->tail].event = *event;
    queue->events[queue->tail].timestamp = timestamp;
    queue->tail = (queue->tail + 1) % MAX_QUEUED_EVENTS;
    queue->count++;
}

static bool queue_pop(struct event_queue *queue, SDL_Event *event, Uint64 *timestamp) {
    if (queue_is_empty(queue)) return false;
    
    *event = queue->events[queue->head].event;
    *timestamp = queue->events[queue->head].timestamp;
    queue->head = (queue->head + 1) % MAX_QUEUED_EVENTS;
    queue->count--;
    return true;
}

bool event_replayer_process(struct event_replayer *replayer) {
    if (!replayer->is_replaying) {
        return false;
    }
    
    // 큐가 비어있을 때만 새 이벤트를 읽음
    if (queue_is_empty(&replayer->queue)) {
        while (!queue_is_full(&replayer->queue)) {
            char line[256];
            if (!fgets(line, sizeof(line), replayer->log_file)) {
                break;
            }
            
            if (line[0] == '#') continue;
            
            Uint64 timestamp;
            char type[32];
            int x, y, code, modifiers;
            
            if (sscanf(line, "%llu %s %d %d %d %x", 
                       &timestamp, type, &x, &y, &code, &modifiers) != 6) {
                LOGW("Invalid log line format: %s", line);
                continue;
            }
            
            SDL_Event event;
            if (parse_and_create_event(type, x, y, code, modifiers, &event)) {
                queue_push(&replayer->queue, &event, timestamp);
            }
        }
    }
    
    // 이벤트 실행
    SDL_Event event;
    Uint64 timestamp;
    if (queue_pop(&replayer->queue, &event, &timestamp)) {
        Uint64 current_time = get_current_time_us(replayer->start_time, replayer->time_scale);
        
        if (current_time < timestamp) {
            Uint64 delay_us = timestamp - current_time;
            if (delay_us > 1000) {
                SDL_Delay((Uint32)(delay_us / 1000));
            }
        }
        
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            event.key.timestamp = SDL_GetTicks();
            SDL_Window *window = replayer->window;
            if (window) {
                event.key.windowID = SDL_GetWindowID(window);
                LOGD("Replaying keyboard event: type=%s, sym=0x%x, scancode=%d, mod=0x%x, window=%u",
                     event.type == SDL_KEYDOWN ? "KEY_DOWN" : "KEY_UP",
                     event.key.keysym.sym,
                     event.key.keysym.scancode,
                     event.key.keysym.mod,
                     event.key.windowID);
                
                if (SDL_PushEvent(&event) < 0) {
                    LOGW("Failed to push keyboard event: %s", SDL_GetError());
                }
            }
        } else {
            if (SDL_PushEvent(&event) < 0) {
                LOGW("Failed to push event: %s", SDL_GetError());
            }
        }
        return true;
    }
    
    if (queue_is_empty(&replayer->queue)) {
        replayer->is_replaying = false;
        return false;
    }
    
    return true;
}

static bool parse_and_create_event(const char *type, int x, int y, 
                                 int code, int modifiers, SDL_Event *event) {
    memset(event, 0, sizeof(SDL_Event));

    if (strcmp(type, "MOUSE_MOTION") == 0) {
        event->type = SDL_MOUSEMOTION;
        event->motion.x = x;
        event->motion.y = y;
        event->motion.xrel = 0;
        event->motion.yrel = 0;
        event->motion.state = 0;
        return true;
    }
    else if (strcmp(type, "MOUSE_DOWN") == 0) {
        event->type = SDL_MOUSEBUTTONDOWN;
        event->button.button = code;
        event->button.x = x;
        event->button.y = y;
        event->button.clicks = 1;
        event->button.state = SDL_PRESSED;
        return true;
    }
    else if (strcmp(type, "MOUSE_UP") == 0) {
        event->type = SDL_MOUSEBUTTONUP;
        event->button.button = code;
        event->button.x = x;
        event->button.y = y;
        event->button.clicks = 1;
        event->button.state = SDL_RELEASED;
        return true;
    }
    else if (strcmp(type, "KEY_DOWN") == 0 || strcmp(type, "KEY_UP") == 0) {
        event->type = (strcmp(type, "KEY_DOWN") == 0) ? SDL_KEYDOWN : SDL_KEYUP;
        event->key.state = (event->type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
        event->key.repeat = 0;
        event->key.keysym.sym = (SDL_Keycode)code;
        event->key.keysym.scancode = SDL_GetScancodeFromKey((SDL_Keycode)code);
        event->key.keysym.mod = modifiers;
        
        LOGD("Creating keyboard event: type=%s, sym=0x%x, scancode=%d, mod=0x%x",
             type, event->key.keysym.sym, event->key.keysym.scancode, modifiers);
        
        return true;
    }
    
    LOGW("Unknown event type: %s", type);
    return false;
}

void event_replayer_close(struct event_replayer *replayer) {
    if (replayer->log_file) {
        fclose(replayer->log_file);
        replayer->log_file = NULL;
    }
    replayer->is_replaying = false;
}