#ifndef API_H
#define API_H

#include <ESPAsyncWebServer.h>

void handleGetConfig(AsyncWebServerRequest *request);
void handleGetLogs(AsyncWebServerRequest *request);
void handleGetCurrentReadings(AsyncWebServerRequest *request);
void handleSaveConfig(AsyncWebServerRequest *request);
void handleResetConfig(AsyncWebServerRequest *request);
void handleRestartDevice(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
void setupAPIEndpoints();

#endif // API_H 