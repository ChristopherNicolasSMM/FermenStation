#include <WiFi.h>
#include <WiFiAP.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// --- DEFINIÇÕES GLOBAIS E CONSTANTES ---
#define DEBUG_MODE true
#define MAX_LOGS 60
#define WIFI_TIMEOUT 30000 // 30 segundos

IPAddress apIP(192, 168, 0, 1);
IPAddress apGateway(192, 168, 0, 1);
IPAddress apSubnet(255, 255, 255, 0);

// --- CONFIGURAÇÕES DO SUPABASE ---
const char *SUPABASE_URL = "https://ftycjulkovsffzdxpcwf.supabase.co/rest/v1";
const char *SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImZ0eWNqdWxrb3ZzZmZ6ZHhwY3dmIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTE0NTU4NDEsImV4cCI6MjA2NzAzMTg0MX0.vreBc6iAqfgampJMrbFx_6AUO6PtjuOMjJVGCvuyu-Q";

// --- DEFINIÇÕES DOS PINOS ---
// --- DEFINIÇÕES DOS PINOS ---
const int PINO_FERMENTADOR = 16; // GPIO16 para sensor do fermentador
const int PINO_AMBIENTE = 17;    // GPIO17 para sensor ambiente
const int PINO_DEGELO = 18;      // GPIO18 para sensor de degelo

// Remove estas linhas (não serão usadas):
// const int ONE_WIRE_BUS = 4;
// OneWire oneWire(ONE_WIRE_BUS);
// DallasTemperature sensors(&oneWire);

// Barramentos OneWire independentes para cada sensor:
OneWire oneWireFermentador(PINO_FERMENTADOR);
OneWire oneWireAmbiente(PINO_AMBIENTE);
OneWire oneWireDegelo(PINO_DEGELO);

// Instâncias DallasTemperature para cada sensor:
DallasTemperature sensorFermentador(&oneWireFermentador);
DallasTemperature sensorAmbiente(&oneWireAmbiente);
DallasTemperature sensorDegelo(&oneWireDegelo);

// Endereços dos sensores (mantidos):
DeviceAddress tempFermentadorAddress;
DeviceAddress tempAmbienteAddress;
DeviceAddress tempDegeloAddress;

const int RELAY_PIN_AQUECIMENTO = 27;
const int RELAY_PIN_RESFRIAMENTO = 26;
const int RELAY_PIN_DEGELO = 25;
const int RESET_BUTTON_PIN = 0;
const unsigned long RESET_BUTTON_HOLD_TIME_MS = 5000;

// --- VARIÁVEIS GLOBAIS ---
Preferences preferences;
AsyncWebServer server(80);

String savedSsid = "";
String savedPassword = "";
String savedDeviceId = "";
String savedProcessId = "";

String savedDegeloModo = "desativado";
String savedDegeloTempo = "00:00";
float savedDegeloTemperatura = 5.0;
float savedTemperaturaMinSeguranca = 0.0;
float savedTemperaturaMaxSeguranca = 35.0;
float savedTemperaturaAlvoLocal = 20.0;
float savedVariacaoTemperaturaLocal = 0.5;

bool wifiConnected = false;
bool apModeActive = false;
bool processFound = false;
int wifiErrorCount = 0;
const int MAX_WIFI_ERRORS = 5;

bool currentRelayAquecimentoState = false;
bool currentRelayResfriamentoState = false;
bool currentRelayDegeloState = false;

String logs[MAX_LOGS];
int currentLogIndex = 0;
bool logBufferFull = false;

unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_READ_INTERVAL_MS = 30000;
const unsigned long OFFLINE_RETRY_INTERVAL_MS = 60000;
unsigned long lastOfflineRetryTime = 0;
unsigned long buttonPressStartTime = 0;

// --- FUNÇÕES AUXILIARES ---
void addLog(String message)
{
    logs[currentLogIndex] = message;
    currentLogIndex = (currentLogIndex + 1) % MAX_LOGS;
    if (currentLogIndex == 0)
        logBufferFull = true;

    if (DEBUG_MODE)
    {
        Serial.println(message);
    }
}

void saveConfigurations()
{
    preferences.begin("fermenstation", false);
    preferences.putString("ssid", savedSsid);
    preferences.putString("password", savedPassword);
    preferences.putString("deviceId", savedDeviceId);
    preferences.putString("processId", savedProcessId);

    preferences.putString("degeloModo", savedDegeloModo);
    preferences.putString("degeloTempo", savedDegeloTempo);
    preferences.putFloat("degeloTemp", savedDegeloTemperatura);
    preferences.putFloat("tempMinSeg", savedTemperaturaMinSeguranca);
    preferences.putFloat("tempMaxSeg", savedTemperaturaMaxSeguranca);
    preferences.putFloat("tempAlvoLocal", savedTemperaturaAlvoLocal);
    preferences.putFloat("variacaoTempLocal", savedVariacaoTemperaturaLocal);

    preferences.end();
    addLog("Configurações salvas na memória persistente.");
}

void loadConfigurations()
{
    preferences.begin("fermenstation", false);
    savedSsid = preferences.getString("ssid", "");
    savedPassword = preferences.getString("password", "");
    savedDeviceId = preferences.getString("deviceId", "");
    savedProcessId = preferences.getString("processId", "");

    savedDegeloModo = preferences.getString("degeloModo", "desativado");
    savedDegeloTempo = preferences.getString("degeloTempo", "00:00");
    savedDegeloTemperatura = preferences.getFloat("degeloTemp", 5.0);
    savedTemperaturaMinSeguranca = preferences.getFloat("tempMinSeg", 0.0);
    savedTemperaturaMaxSeguranca = preferences.getFloat("tempMaxSeg", 35.0);
    savedTemperaturaAlvoLocal = preferences.getFloat("tempAlvoLocal", 20.0);
    savedVariacaoTemperaturaLocal = preferences.getFloat("variacaoTempLocal", 0.5);

    preferences.end();
    addLog("Configurações carregadas da memória persistente.");
}

void clearConfigurations()
{
    preferences.begin("fermenstation", false);
    preferences.clear();
    preferences.end();
    addLog("Todas as configurações foram limpas.");
}

void setRelayState(int relayPin, bool state)
{
    digitalWrite(relayPin, state ? HIGH : LOW);
    addLog("Relé " + String(relayPin) + " definido como " + (state ? "LIGADO" : "DESLIGADO"));
}


float readDSTemperature(DeviceAddress sensorAddress, DallasTemperature &sensorInstance)
{
    sensorInstance.requestTemperatures();
    float tempC = sensorInstance.getTempC(sensorAddress);
    if (tempC == DEVICE_DISCONNECTED_C)
    {
        addLog("Erro ao ler sensor de temperatura.");
        return -127.0;
    }
    return tempC;
}

void debugAllSensors()
{
    addLog("[DEBUG] Temperaturas:");
    addLog("Fermentador: " + String(readDSTemperature(tempFermentadorAddress, sensorFermentador)) + "°C");
    addLog("Ambiente: " + String(readDSTemperature(tempAmbienteAddress, sensorAmbiente)) + "°C");
    addLog("Degelo: " + String(readDSTemperature(tempDegeloAddress, sensorDegelo)) + "°C");
}

// --- FUNÇÕES DE COMUNICAÇÃO COM SUPABASE ---
String callSupabaseRpc(const String &rpcName, const String &payload)
{
    HTTPClient http;
    String url = String(SUPABASE_URL) + "/rpc/" + rpcName;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));

    addLog("Chamando RPC: " + rpcName + " com payload: " + payload);
    int httpResponseCode = http.POST(payload);
    String response = "";

    if (httpResponseCode > 0)
    {
        response = http.getString();
        addLog("Resposta RPC (" + String(httpResponseCode) + "): " + response);
    }
    else
    {
        addLog("Erro na chamada RPC (" + String(httpResponseCode) + "): " + http.errorToString(httpResponseCode));
    }
    http.end();
    return response;
}

bool validateDeviceOnSupabase()
{
    if (savedDeviceId.length() == 0)
    {
        addLog("Device ID não configurado. Não é possível validar no Supabase.");
        return false;
    }

    JsonDocument doc;
    doc["p_device_id"] = savedDeviceId;
    String payload;
    serializeJson(doc, payload);

    String response = callSupabaseRpc("rpc_validate_device", payload);

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error)
    {
        addLog("Erro ao parsear resposta de validação do dispositivo: " + String(error.f_str()));
        return false;
    }

    if (responseDoc["status"] == "success")
    {
        addLog("Dispositivo validado com sucesso no Supabase.");
        return true;
    }
    else
    {
        addLog("Falha na validação do dispositivo no Supabase: " + responseDoc["message"].as<String>());
        return false;
    }
}

bool getActiveProcessOnSupabase()
{
    if (savedDeviceId.length() == 0)
    {
        addLog("Device ID não configurado. Não é possível buscar processo ativo.");
        return false;
    }

    JsonDocument doc;
    doc["p_device_id"] = savedDeviceId;
    String payload;
    serializeJson(doc, payload);

    String response = callSupabaseRpc("rpc_get_active_process", payload);

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error)
    {
        addLog("Erro ao parsear resposta de processo ativo: " + String(error.f_str()));
        return false;
    }

    if (responseDoc["process_found"] == true)
    {
        savedProcessId = responseDoc["process_id"].as<String>();
        savedTemperaturaAlvoLocal = responseDoc["temperatura_alvo_receita"] | 20.0;
        savedVariacaoTemperaturaLocal = responseDoc["variacao_aceitavel_receita"] | 0.5;

        addLog("Processo ativo encontrado: " + savedProcessId);
        saveConfigurations();
        return true;
    }
    else
    {
        addLog("Nenhum processo ativo encontrado: " + responseDoc["message"].as<String>());
        savedProcessId = "";
        saveConfigurations();
        return false;
    }
}

void controlFermenstationOnSupabase(float tempFermentador, float tempAmbiente, float tempDegelo, float gravidade = -1.0)
{
    if (!processFound || savedProcessId.length() == 0)
    {
        addLog("Nenhum processo ativo ou ID do processo. Não é possível controlar via Supabase.");
        return;
    }

    JsonDocument doc;
    doc["p_device_id"] = savedDeviceId;
    doc["p_processo_id"] = savedProcessId;
    doc["p_temp_fermentador"] = tempFermentador;
    doc["p_temp_ambiente"] = tempAmbiente;
    doc["p_temp_degelo"] = tempDegelo;
    if (gravidade != -1.0)
    {
        doc["p_gravidade"] = gravidade;
    }
    String payload;
    serializeJson(doc, payload);

    String response = callSupabaseRpc("rpc_controlar_fermentacao", payload);

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error)
    {
        addLog("Erro ao parsear resposta de controle de fermentação: " + String(error.f_str()));
        return;
    }

    currentRelayAquecimentoState = responseDoc["releAquecimento"] | false;
    currentRelayResfriamentoState = responseDoc["releResfriamento"] | false;
    currentRelayDegeloState = responseDoc["releDegelo"] | false;

    setRelayState(RELAY_PIN_AQUECIMENTO, currentRelayAquecimentoState);
    setRelayState(RELAY_PIN_RESFRIAMENTO, currentRelayResfriamentoState);
    setRelayState(RELAY_PIN_DEGELO, currentRelayDegeloState);

    addLog("Relés atualizados pelo Supabase: Aquecimento=" + String(currentRelayAquecimentoState) +
           ", Resfriamento=" + String(currentRelayResfriamentoState) +
           ", Degelo=" + String(currentRelayDegeloState));
    addLog("Ação tomada pelo Supabase: " + responseDoc["acaoTomada"].as<String>());
}

// --- LÓGICA DE CONTROLE LOCAL ---
void localControlLogic(float tempFermentador, float tempAmbiente, float tempDegelo)
{
    addLog("Executando lógica de controle LOCAL (offline/fallback).");

    currentRelayAquecimentoState = false;
    currentRelayResfriamentoState = false;
    currentRelayDegeloState = false;
    String acaoLocal = "Nenhuma ação local necessária.";

    if (savedDegeloModo == "por_temperatura")
    {
        if (tempDegelo < savedDegeloTemperatura)
        {
            currentRelayDegeloState = true;
            acaoLocal = "Degelo por temperatura ativado (local).";
        }
    }

    if (currentRelayDegeloState)
    {
        currentRelayAquecimentoState = false;
        currentRelayResfriamentoState = false;
    }
    else
    {
        if (tempFermentador < savedTemperaturaMinSeguranca)
        {
            currentRelayAquecimentoState = true;
            currentRelayResfriamentoState = false;
            acaoLocal = "Aquecimento de SEGURANÇA (temp muito baixa - local).";
        }
        else if (tempFermentador > savedTemperaturaMaxSeguranca)
        {
            currentRelayAquecimentoState = false;
            currentRelayResfriamentoState = true;
            acaoLocal = "Resfriamento de SEGURANÇA (temp muito alta - local).";
        }
        else
        {
            if (tempFermentador < (savedTemperaturaAlvoLocal - savedVariacaoTemperaturaLocal))
            {
                currentRelayAquecimentoState = true;
                currentRelayResfriamentoState = false;
                acaoLocal = "Aquecimento (temp abaixo do alvo - local).";
            }
            else if (tempFermentador > (savedTemperaturaAlvoLocal + savedVariacaoTemperaturaLocal))
            {
                currentRelayAquecimentoState = false;
                currentRelayResfriamentoState = true;
                acaoLocal = "Resfriamento (temp acima do alvo - local).";
            }
            else
            {
                currentRelayAquecimentoState = false;
                currentRelayResfriamentoState = false;
                acaoLocal = "Temperatura no alvo - relés desligados (local).";
            }
        }
    }

    setRelayState(RELAY_PIN_AQUECIMENTO, currentRelayAquecimentoState);
    setRelayState(RELAY_PIN_RESFRIAMENTO, currentRelayResfriamentoState);
    setRelayState(RELAY_PIN_DEGELO, currentRelayDegeloState);

    addLog("Relés atualizados LOCALMENTE: Aquecimento=" + String(currentRelayAquecimentoState) +
           ", Resfriamento=" + String(currentRelayResfriamentoState) +
           ", Degelo=" + String(currentRelayDegeloState));
    addLog("Ação tomada localmente: " + acaoLocal);
}

// --- HANDLERS DA API REST ---
void handleGetConfig(AsyncWebServerRequest *request)
{
    addLog("GET /api/config solicitado");

    JsonDocument doc;
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

void handleGetLogs(AsyncWebServerRequest *request)
{
    addLog("GET /api/logs solicitado");

    JsonDocument doc;
    JsonArray logArray = doc.to<JsonArray>();

    if (logBufferFull)
    {
        for (int i = currentLogIndex; i < MAX_LOGS; i++)
        {
            logArray.add(logs[i]);
        }
        for (int i = 0; i < currentLogIndex; i++)
        {
            logArray.add(logs[i]);
        }
    }
    else
    {
        for (int i = 0; i < currentLogIndex; i++)
        {
            logArray.add(logs[i]);
        }
    }

    String response;
    serializeJson(logArray, response);
    request->send(200, "application/json", response);
    addLog("Logs enviados com sucesso");
}

void handleGetCurrentReadings(AsyncWebServerRequest *request)
{
    addLog("GET /api/readings solicitado");

    float tempFermentador = readDSTemperature(tempFermentadorAddress, sensorFermentador);
    float tempAmbiente = readDSTemperature(tempAmbienteAddress, sensorAmbiente);
    float tempDegelo = readDSTemperature(tempDegeloAddress, sensorDegelo);

    JsonDocument doc;
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

void handleSaveConfig(AsyncWebServerRequest *request)
{
    addLog("POST /api/config recebido");

    if (request->hasHeader("Content-Type"))
    {
        String contentType = request->getHeader("Content-Type")->value();
        addLog("Content-Type recebido: " + contentType);
        // contentType.toLowerCase();

        // if(contentType.startsWith("application/json") || contentType.("application/json")){
        if (contentType.indexOf("application/json") != -1)
        {
            // Content-Type válido (contém application/json)
            addLog("Content-Type válido detectado");

            if (request->_tempObject != NULL)
            {
                String body = *((String *)(request->_tempObject));
                addLog("Body recebido: " + body);

                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, body);

                if (error)
                {
                    String errorMsg = "Erro ao parsear JSON: " + String(error.c_str());
                    addLog(errorMsg);
                    request->send(400, "text/plain", errorMsg);
                    delete (String *)(request->_tempObject);
                    return;
                }

                if (doc["ssid"].is<String>())
                    savedSsid = doc["ssid"].as<String>();
                if (doc["password"].is<String>())
                    savedPassword = doc["password"].as<String>();
                if (doc["deviceId"].is<String>())
                    savedDeviceId = doc["deviceId"].as<String>();
                if (doc["degeloModo"].is<String>())
                    savedDegeloModo = doc["degeloModo"].as<String>();
                if (doc["degeloTempo"].is<String>())
                    savedDegeloTempo = doc["degeloTempo"].as<String>();
                if (doc["degeloTemperatura"].is<float>())
                    savedDegeloTemperatura = doc["degeloTemperatura"].as<float>();
                if (doc["temperaturaMinSeguranca"].is<float>())
                    savedTemperaturaMinSeguranca = doc["temperaturaMinSeguranca"].as<float>();
                if (doc["temperaturaMaxSeguranca"].is<float>())
                    savedTemperaturaMaxSeguranca = doc["temperaturaMaxSeguranca"].as<float>();
                if (doc["temperaturaAlvoLocal"].is<float>())
                    savedTemperaturaAlvoLocal = doc["temperaturaAlvoLocal"].as<float>();
                if (doc["variacaoTemperaturaLocal"].is<float>())
                    savedVariacaoTemperaturaLocal = doc["variacaoTemperaturaLocal"].as<float>();

                saveConfigurations();
                request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Configurações salvas com sucesso\"}");
                addLog("Configurações salvas via POST");
                delete (String *)(request->_tempObject);
            }
            else
            {
                addLog("Nenhum dado recebido no body");
                request->send(400, "text/plain", "Nenhum dado JSON recebido");
            }
        }
        else
        {
            addLog("Content-Type inválido ou ausente");
            request->send(400, "text/plain", "Content-Type deve ser application/json");
        }
    }
    else
    {
        addLog("Nenhum Content-Type recebido");
        request->send(400, "text/plain", "Content-Type ausente");
    }
}

void handleResetConfig(AsyncWebServerRequest *request)
{
    addLog("POST /api/reset solicitado");
    clearConfigurations();
    request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Configurações resetadas. Reinicie o dispositivo.\"}");
    addLog("Configurações resetadas");
}

void handleRestartDevice(AsyncWebServerRequest *request)
{
    addLog("POST /api/restart solicitado - Reiniciando dispositivo");
    request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"Dispositivo reiniciando...\"}");
    delay(1000);
    ESP.restart();
}

void handleNotFound(AsyncWebServerRequest *request)
{
    String message = "Endpoint não encontrado\n\n";
    message += "URI: ";
    message += request->url();
    message += "\nMethod: ";
    message += (request->method() == HTTP_GET) ? "GET" : "POST";

    addLog("Endpoint não encontrado: " + request->url());
    request->send(404, "text/plain", message);
}

void setupAPIEndpoints()
{

    // Rota para resetar o ESP32
    server.on("/api/reset", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        addLog("Recebido comando de reset via API");
        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"ESP32 reiniciando em 1 segundo\"}");
        addLog("Reiniciando ESP32 em 1 segundo...");
        // Aguardar 1 segundo antes de reiniciar    
        delay(1000);
        ESP.restart(); });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleGetConfig(request); });

    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleGetLogs(request); });

    server.on("/api/readings", HTTP_GET, [](AsyncWebServerRequest *request)
              { handleGetCurrentReadings(request); });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request)
              { handleSaveConfig(request); }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
    if(request->_tempObject == NULL){
      request->_tempObject = new String();
    }
    ((String*)(request->_tempObject))->concat((const char*)data, len); });

    server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request)
              { handleResetConfig(request); });

    server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request)
              { handleRestartDevice(request); });

    server.onNotFound([](AsyncWebServerRequest *request)
                      { handleNotFound(request); });

    addLog("Endpoints da API configurados");
}

// --- FUNÇÕES WIFI ---
void startAPMode()
{
    apModeActive = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apGateway, apSubnet);

    uint8_t mac[6];
    WiFi.macAddress(mac);
    String apSSID = "FermenStation_" + String(mac[4], HEX) + String(mac[5], HEX);

    WiFi.softAP(apSSID.c_str());
    addLog("Modo AP iniciado. SSID: " + apSSID + " IP: " + WiFi.softAPIP().toString());

    WiFi.setSleep(false); // Desabilita sleep para conexão mais estável

    setupAPIEndpoints();
    server.begin();
    addLog("Servidor HTTP iniciado no modo AP");
}

/*
void connectToWiFi()
{
    if (apModeActive)
    {
        addLog("Já está no modo AP. Não é possível conectar ao WiFi.");
        return;
    }

    addLog("Tentando conectar ao WiFi...");
    if (savedSsid.length() == 0 || savedPassword.length() == 0)
    {
        addLog("Nenhuma credencial WiFi salva. Iniciando modo AP...");
        startAPMode();
        return;
    }

    addLog("Conectando ao WiFi: " + savedSsid);
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20)
    {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        wifiConnected = true;
        apModeActive = false;
        addLog("\nWiFi conectado! IP: " + WiFi.localIP().toString());

        if (validateDeviceOnSupabase())
        {
            processFound = getActiveProcessOnSupabase();
        }
        else
        {
            savedDeviceId = "";
            saveConfigurations();
            addLog("Falha na validação do dispositivo. Modo offline.");
        }
    }
    else
    {
        wifiConnected = false;
        addLog("\nFalha ao conectar ao WiFi. Iniciando modo AP...");
        startAPMode();
    }
}


// --- FUNÇÃO DE VERIFICAÇÃO DE CONEXÃO WIFI ---
void checkWiFiConnection()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        wifiConnected = false;
        addLog("WiFi desconectado. Tentando reconectar...");
        connectToWiFi();
    }
    else
    {
        wifiConnected = true;
    }
}

*/
String getWiFiStatusString(int status)
{
    switch (status)
    {
    case WL_NO_SHIELD:
        return "WL_NO_SHIELD";
    case WL_IDLE_STATUS:
        return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:
        return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
        return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:
        return "WL_CONNECTED";
    case WL_CONNECT_FAILED:
        return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST:
        return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:
        return "WL_DISCONNECTED";
    default:
        return "UNKNOWN (" + String(status) + ")";
    }
}

void scanNetworks() {
    addLog("Scanning networks...");
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        addLog(WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dB) " + 
              (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured"));
    }
}

void connectToWiFi()
{

    if (apModeActive)
    {
        addLog("Modo AP ativo - Conexão WiFi ignorada");
        return;
    }

    scanNetworks();

    // Verifica credenciais
    if (savedSsid.length() == 0 || savedPassword.length() == 0)
    {
        addLog("Credenciais WiFi não configuradas");
        startAPMode();
        return;
    }

    addLog("Conectando a: " + savedSsid);
    addLog("Tamanho senha: " + String(savedPassword.length()));

    // Configuração avançada do WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Desativa sleep para melhor estabilidade
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

    // Tentativa com timeout maior
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < WIFI_TIMEOUT)
    {

        int status = WiFi.status();
        addLog("Status: " + getWiFiStatusString(status) +
               " | Tentativa: " + String((millis() - startTime) / 1000) + "s");

        if (status == WL_NO_SSID_AVAIL)
        {
            addLog("Rede não encontrada - Verifique o nome");
            break;
        }
        delay(1000);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        wifiConnected = true;
        addLog("Conectado! IP: " + WiFi.localIP().toString() +
               " | RSSI: " + WiFi.RSSI() + "dBm");

        // Verifica conexão com a internet
        if (validateDeviceOnSupabase())
        {
            processFound = getActiveProcessOnSupabase();
        }
    }
    else
    {
        addLog("Falha na conexão - Último status: " +
               getWiFiStatusString(WiFi.status()));
        startAPMode();
    }
}

void checkWiFiConnection()
{
    static unsigned long lastCheck = 0;
    const unsigned long checkInterval = 15000; // 15 segundos

    if (millis() - lastCheck >= checkInterval)
    {
        lastCheck = millis();

        int status = WiFi.status();
        addLog("Verificação WiFi - Status: " + getWiFiStatusString(status));

        if (status != WL_CONNECTED)
        {
            wifiConnected = false;
            wifiErrorCount++;
            addLog("WiFi desconectado. Tentativas: " + String(wifiErrorCount));

            if (wifiErrorCount >= MAX_WIFI_ERRORS && !apModeActive)
            {
                addLog("Máximo de tentativas alcançado - Ativando modo AP");
                startAPMode();
            }
            else
            {
                connectToWiFi(); // Tentar reconectar
            }
        }
        else
        {
            wifiErrorCount = 0; // Resetar contador se conectado
        }
    }
}

// --- SETUP E LOOP ---
void setup()
{
    Serial.begin(115200);
    delay(100);
    addLog("Iniciando FermenStation...");

    // Adicione no setup():
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
                 { addLog("Evento WiFi: " + String(event)); });

    for (int i = 0; i < MAX_LOGS; i++)
    {
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

    // Verifica endereços dos sensores
    if (!sensorFermentador.getAddress(tempFermentadorAddress, 0))
    {
        addLog("ERRO: Sensor do fermentador (GPIO16) não encontrado!");
    }
    if (!sensorAmbiente.getAddress(tempAmbienteAddress, 0))
    {
        addLog("ERRO: Sensor ambiente (GPIO17) não encontrado!");
    }
    if (!sensorDegelo.getAddress(tempDegeloAddress, 0))
    {
        addLog("ERRO: Sensor de degelo (GPIO18) não encontrado!");
    }

    loadConfigurations();
    connectToWiFi();
}

void loop()
{
    checkWiFiConnection();

    if (digitalRead(RESET_BUTTON_PIN) == LOW)
    {
        if (buttonPressStartTime == 0)
        {
            buttonPressStartTime = millis();
            addLog("Botão de reset pressionado...");
        }
        else if (millis() - buttonPressStartTime >= RESET_BUTTON_HOLD_TIME_MS)
        {
            addLog("Botão de reset segurado por 5 segundos. Limpando configurações...");
            clearConfigurations();
        }
    }
    else
    {
        buttonPressStartTime = 0;
    }

    if (!wifiConnected && millis() - lastOfflineRetryTime >= OFFLINE_RETRY_INTERVAL_MS)
    {
        lastOfflineRetryTime = millis();
        if (!apModeActive)
        {
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

    if (millis() - lastSensorReadTime >= SENSOR_READ_INTERVAL_MS)
    {
        lastSensorReadTime = millis();

        addLog("Temperaturas lidas: Fermentador=" + String(tempFermentador) + "°C, Ambiente=" + String(tempAmbiente) + "°C, Degelo=" + String(tempDegelo) + "°C");

        if (wifiConnected && processFound && savedProcessId.length() > 0)
        {
            controlFermenstationOnSupabase(tempFermentador, tempAmbiente, tempDegelo, gravidade);
        }
        else
        {
            addLog("Modo Offline/Fallback: Sem WiFi ou processo ativo. Usando controle local.");
            localControlLogic(tempFermentador, tempAmbiente, tempDegelo);
        }
    }

    debugAllSensors();
    delay(1000); // Delay para evitar sobrecarga do loop
}