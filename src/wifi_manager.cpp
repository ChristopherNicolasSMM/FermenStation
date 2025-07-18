#include "wifi_manager.h"
#include "config.h"
#include "log.h"
#include "api.h"
#include <WiFi.h>
#include <WiFiAP.h>

IPAddress apIP(192, 168, 0, 1);
IPAddress apGateway(192, 168, 0, 1);
IPAddress apSubnet(255, 255, 255, 0);

void startAPMode() {
    apModeActive = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apGateway, apSubnet);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String apSSID = "FermenStation_" + String(mac[4], HEX) + String(mac[5], HEX);
    WiFi.softAP(apSSID.c_str());
    addLog("Modo AP iniciado. SSID: " + apSSID + " IP: " + WiFi.softAPIP().toString());
    WiFi.setSleep(false);
    setupAPIEndpoints();
    extern AsyncWebServer server;
    server.begin();
    addLog("Servidor HTTP iniciado no modo AP");
}

String getWiFiStatusString(int status) {
    switch (status) {
        case WL_NO_SHIELD: return "WL_NO_SHIELD";
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED: return "WL_DISCONNECTED";
        default: return "UNKNOWN (" + String(status) + ")";
    }
}

void scanNetworks() {
    addLog("Scanning networks...");
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        addLog(WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dB) " + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured"));
    }
}

void connectToWiFi() {
    if (apModeActive) {
        addLog("Modo AP ativo - Conexão WiFi ignorada");
        return;
    }
    scanNetworks();
    if (savedSsid.length() == 0 || savedPassword.length() == 0) {
        addLog("Credenciais WiFi não configuradas");
        startAPMode();
        return;
    }
    addLog("Conectando a: " + savedSsid);
    addLog("Tamanho senha: " + String(savedPassword.length()));
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
        int status = WiFi.status();
        addLog("Status: " + getWiFiStatusString(status) + " | Tentativa: " + String((millis() - startTime) / 1000) + "s");
        if (status == WL_NO_SSID_AVAIL) {
            addLog("Rede não encontrada - Verifique o nome");
            break;
        }
        delay(1000);
    }
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        addLog("Conectado! IP: " + WiFi.localIP().toString() + " | RSSI: " + WiFi.RSSI() + "dBm");
        extern bool validateDeviceOnSupabase();
        extern bool getActiveProcessOnSupabase();
        if (validateDeviceOnSupabase()) {
            processFound = getActiveProcessOnSupabase();
        }
    } else {
        addLog("Falha na conexão - Último status: " + getWiFiStatusString(WiFi.status()));
        startAPMode();
    }
}

void checkWiFiConnection() {
    static unsigned long lastCheck = 0;
    const unsigned long checkInterval = 15000;
    if (millis() - lastCheck >= checkInterval) {
        lastCheck = millis();
        int status = WiFi.status();
        addLog("Verificação WiFi - Status: " + getWiFiStatusString(status));
        if (status != WL_CONNECTED) {
            wifiConnected = false;
            wifiErrorCount++;
            addLog("WiFi desconectado. Tentativas: " + String(wifiErrorCount));
            if (wifiErrorCount >= MAX_WIFI_ERRORS && !apModeActive) {
                addLog("Máximo de tentativas alcançado - Ativando modo AP");
                startAPMode();
            } else {
                connectToWiFi();
            }
        } else {
            wifiErrorCount = 0;
        }
    }
} 