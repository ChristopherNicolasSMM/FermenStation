#include "include/api.h"
#include "include/config.h"
#include "include/log.h"
#include "include/storage.h"
#include "include/controle.h"
#include "include/sensores.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

void handleGetConfig(AsyncWebServerRequest *request) {
    addLog("GET /api/config solicitado");
    StaticJsonDocument<512> doc;
    doc["ssid"] = savedSsid;
    doc["deviceId"] = savedDeviceId;
    doc["processId"] = savedProcessId;
    doc["degeloModo"] = savedDegeloModo;
    doc["degeloTempo"] = savedDegeloTempo;
    doc["degeloTemperatura"] = savedDegeloTemperatura;
    doc["temperaturaMinSeguranca"] = savedTemperaturaMinSeguranca;
    doc["temperaturaMaxSeguranca"] = savedTemperaturaMaxSeguranca;
    doc["temperaturaAlvoLocal"] = savedTemperaturaAlvoLocal;
    doc["variacaoTemperaturaLocal"] = savedVariacaoTemperaturaLocal;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
    addLog("Configurações enviadas com sucesso");
}

void handleGetLogs(AsyncWebServerRequest *request) {
    addLog("GET /api/logs solicitado");
    StaticJsonDocument<2048> doc;
    JsonArray logArray = doc.to<JsonArray>();
    if (logBufferFull) {
        for (int i = currentLogIndex; i < MAX_LOGS; i++) {
            logArray.add(logs[i]);
        }
        for (int i = 0; i < currentLogIndex; i++) {
            logArray.add(logs[i]);
        }
    } else {
        for (int i = 0; i < currentLogIndex; i++) {
            logArray.add(logs[i]);
        }
    }
    String response;
    serializeJson(logArray, response);
    request->send(200, "application/json", response);
    addLog("Logs enviados com sucesso");
}

void handleGetCurrentReadings(AsyncWebServerRequest *request) {
    addLog("GET /api/readings solicitado");
    float tempFermentador = readDSTemperature(tempFermentadorAddress, sensorFermentador);
    float tempAmbiente = readDSTemperature(tempAmbienteAddress, sensorAmbiente);
    float tempDegelo = readDSTemperature(tempDegeloAddress, sensorDegelo);
    StaticJsonDocument<256> doc;
    doc["tempFermentador"] = tempFermentador;
    doc["tempAmbiente"] = tempAmbiente;
    doc["tempDegelo"] = tempDegelo;
    doc["releAquecimento"] = currentRelayAquecimentoState;
    doc["releResfriamento"] = currentRelayResfriamentoState;
    doc["releDegelo"] = currentRelayDegeloState;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
    addLog("Leituras enviadas com sucesso");
}

void handleSaveConfig(AsyncWebServerRequest *request) {
    addLog("POST /api/config recebido");
    if (request->hasHeader("Content-Type")) {
        String contentType = request->getHeader("Content-Type")->value();
        addLog("Content-Type recebido: " + contentType);
        if (contentType.indexOf("application/json") != -1) {
            if (request->_tempObject != NULL) {
                String body = *((String *)(request->_tempObject));
                addLog("Body recebido: " + body);
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, body);
                if (error) {
                    String errorMsg = "Erro ao parsear JSON: " + String(error.c_str());
                    addLog(errorMsg);
                    request->send(400, "text/plain", errorMsg);
                    delete (String *)(request->_tempObject);
                    return;
                }
                if (doc["ssid"].is<String>()) savedSsid = doc["ssid"].as<String>();
                if (doc["password"].is<String>()) savedPassword = doc["password"].as<String>();
                if (doc["deviceId"].is<String>()) savedDeviceId = doc["deviceId"].as<String>();
                if (doc["degeloModo"].is<String>()) savedDegeloModo = doc["degeloModo"].as<String>();
                if (doc["degeloTempo"].is<String>()) savedDegeloTempo = doc["degeloTempo"].as<String>();
                if (doc["degeloTemperatura"].is<float>()) savedDegeloTemperatura = doc["degeloTemperatura"].as<float>();
                if (doc["temperaturaMinSeguranca"].is<float>()) savedTemperaturaMinSeguranca = doc["temperaturaMinSeguranca"].as<float>();
                if (doc["temperaturaMaxSeguranca"].is<float>()) savedTemperaturaMaxSeguranca = doc["temperaturaMaxSeguranca"].as<float>();
                if (doc["temperaturaAlvoLocal"].is<float>()) savedTemperaturaAlvoLocal = doc["temperaturaAlvoLocal"].as<float>();
                if (doc["variacaoTemperaturaLocal"].is<float>()) savedVariacaoTemperaturaLocal = doc["variacaoTemperaturaLocal"].as<float>();
                saveConfigurations();
                request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Configurações salvas com sucesso\"}");
                addLog("Configurações salvas via POST");
                delete (String *)(request->_tempObject);
            } else {
                addLog("Nenhum dado recebido no body");
                request->send(400, "text/plain", "Nenhum dado JSON recebido");
            }
        } else {
            addLog("Content-Type inválido ou ausente");
            request->send(400, "text/plain", "Content-Type deve ser application/json");
        }
    } else {
        addLog("Nenhum Content-Type recebido");
        request->send(400, "text/plain", "Content-Type ausente");
    }
}

void handleResetConfig(AsyncWebServerRequest *request) {
    addLog("POST /api/reset solicitado");
    clearConfigurations();
    request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Configurações resetadas. Reinicie o dispositivo.\"}");
    addLog("Configurações resetadas");
}

void handleRestartDevice(AsyncWebServerRequest *request) {
    addLog("POST /api/restart solicitado - Reiniciando dispositivo");
    request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Dispositivo reiniciando...\"}");
    delay(1000);
    ESP.restart();
}

void handleNotFound(AsyncWebServerRequest *request) {
    String message = "Endpoint não encontrado\n\n";
    message += "URI: ";
    message += request->url();
    message += "\nMethod: ";
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    addLog("Endpoint não encontrado: " + request->url());
    request->send(404, "text/plain", message);
}

void setupAPIEndpoints() {
    server.on("/api/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        addLog("Recebido comando de reset via API");
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"ESP32 reiniciando em 1 segundo\"}");
        addLog("Reiniciando ESP32 em 1 segundo...");
        delay(1000);
        ESP.restart();
    });
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) { handleGetConfig(request); });
    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request) { handleGetLogs(request); });
    server.on("/api/readings", HTTP_GET, [](AsyncWebServerRequest *request) { handleGetCurrentReadings(request); });
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) { handleSaveConfig(request); }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if(request->_tempObject == NULL){
            request->_tempObject = new String();
        }
        ((String*)(request->_tempObject))->concat((const char*)data, len);
    });
    server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request) { handleResetConfig(request); });
    server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) { handleRestartDevice(request); });
    server.onNotFound([](AsyncWebServerRequest *request) { handleNotFound(request); });
    addLog("Endpoints da API configurados");
} 