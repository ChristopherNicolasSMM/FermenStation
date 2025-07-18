#include "include/log.h"
#include "include/config.h"

void addLog(String message) {
    logs[currentLogIndex] = message;
    currentLogIndex = (currentLogIndex + 1) % MAX_LOGS;
    if (currentLogIndex == 0)
        logBufferFull = true;

    if (DEBUG_MODE) {
        Serial.println(message);
    }
} 