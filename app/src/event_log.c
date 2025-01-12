#include "event_log.h"
#include "util/log.h"

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