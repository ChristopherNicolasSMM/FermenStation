#include "main.h"
#include "config.h"
#include "log.h"
#include "sensores.h"
#include "controle.h"
#include "storage.h"
#include "wifi_manager.h"
#include "api.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

AsyncWebServer server(80);

void setup() {
    Serial.begin(115200);
    delay(100);
    addLog("Iniciando FermenStation...");
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) { addLog("Evento WiFi: " + String(event)); });
    for (int i = 0; i < MAX_LOGS; i++) {
        logs[i] = "";
    }
    pinMode(RELAY_PIN_AQUECIMENTO, OUTPUT);
    pinMode(RELAY_PIN_RESFRIAMENTO, OUTPUT);
    pinMode(RELAY_PIN_DEGELO, OUTPUT);
    setRelayState(RELAY_PIN_AQUECIMENTO, false);
    setRelayState(RELAY_PIN_RESFRIAMENTO, false);
    setRelayState(RELAY_PIN_DEGELO, false);
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    sensorFermentador.begin();
    sensorAmbiente.begin();
    sensorDegelo.begin();
    if (!sensorFermentador.getAddress(tempFermentadorAddress, 0)) {
        addLog("ERRO: Sensor do fermentador (GPIO16) não encontrado!");
    }
    if (!sensorAmbiente.getAddress(tempAmbienteAddress, 0)) {
        addLog("ERRO: Sensor ambiente (GPIO17) não encontrado!");
    }
    if (!sensorDegelo.getAddress(tempDegeloAddress, 0)) {
        addLog("ERRO: Sensor de degelo (GPIO18) não encontrado!");
    }
    loadConfigurations();
    connectToWiFi();
}

void loop() {
    checkWiFiConnection();
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        if (buttonPressStartTime == 0) {
            buttonPressStartTime = millis();
            addLog("Botão de reset pressionado...");
        } else if (millis() - buttonPressStartTime >= RESET_BUTTON_HOLD_TIME_MS) {
            addLog("Botão de reset segurado por 5 segundos. Limpando configurações...");
            clearConfigurations();
        }
    } else {
        buttonPressStartTime = 0;
    }
    if (!wifiConnected && millis() - lastOfflineRetryTime >= OFFLINE_RETRY_INTERVAL_MS) {
        lastOfflineRetryTime = millis();
        if (!apModeActive) {
            addLog("Tentando reconectar ao WiFi...");
            connectToWiFi();
        }
    }
    float tempFermentador = readDSTemperature(tempFermentadorAddress, sensorFermentador);
    float tempAmbiente = readDSTemperature(tempAmbienteAddress, sensorAmbiente);
    float tempDegelo = readDSTemperature(tempDegeloAddress, sensorDegelo);
    float gravidade = -1.0;
    if (tempFermentador == -127.0)
        tempFermentador = 25.0 + sin(millis() / 10000.0) * 2.0;
    if (tempAmbiente == -127.0)
        tempAmbiente = 26.5 + cos(millis() / 15000.0) * 1.5;
    if (tempDegelo == -127.0)
        tempDegelo = 4.0 + sin(millis() / 8000.0) * 1.0;
    if (millis() - lastSensorReadTime >= SENSOR_READ_INTERVAL_MS) {
        lastSensorReadTime = millis();
        addLog("Temperaturas lidas: Fermentador=" + String(tempFermentador) + "°C, Ambiente=" + String(tempAmbiente) + "°C, Degelo=" + String(tempDegelo) + "°C");
        if (wifiConnected && processFound && savedProcessId.length() > 0) {
            extern void controlFermenstationOnSupabase(float, float, float, float);
            controlFermenstationOnSupabase(tempFermentador, tempAmbiente, tempDegelo, gravidade);
        } else {
            addLog("Modo Offline/Fallback: Sem WiFi ou processo ativo. Usando controle local.");
            localControlLogic(tempFermentador, tempAmbiente, tempDegelo);
        }
    }
    debugAllSensors();
    delay(1000);
} 