#include <Arduino.h>
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

void startAPMode();
void connectToWiFi();
void checkWiFiConnection();
String getWiFiStatusString(int status);
void scanNetworks();

#endif // WIFI_MANAGER_H 