#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WebServer.h>

// --- DEFINIÇÕES GLOBAIS E CONSTANTES ---
#define DEBUG_MODE true
#define MAX_LOGS 30 // Número máximo de logs armazenados

IPAddress apIP(192, 168, 0, 1);        // IP do ESP32 no modo AP
IPAddress apGateway(192, 168, 0, 1);   // Gateway (normalmente o mesmo do AP)
IPAddress apSubnet(255, 255, 255, 0);  // Máscara de sub-rede


// --- CONFIGURAÇÕES DO SUPABASE ---
const char *SUPABASE_URL = "https://ftycjulkovsffzdxpcwf.supabase.co/rest/v1";
const char *SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImZ0eWNqdWxrb3ZzZmZ6ZHhwY3dmIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTE0NTU4NDEsImV4cCI6MjA2NzAzMTg0MX0.vreBc6iAqfgampJMrbFx_6AUO6PtjuOMjJVGCvuyu-Q";

// --- DEFINIÇÕES DOS PINOS ---
const int ONE_WIRE_BUS = 4;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress tempFermentadorAddress;
DeviceAddress tempAmbienteAddress;
DeviceAddress tempDegeloAddress;

const int RELAY_PIN_AQUECIMENTO = 27;
const int RELAY_PIN_RESFRIAMENTO = 26;
const int RELAY_PIN_DEGELO = 25;
const int RESET_BUTTON_PIN = 0;
const unsigned long RESET_BUTTON_HOLD_TIME_MS = 5000;

// --- VARIÁVEIS DE ESTADO E CONFIGURAÇÃO PERSISTENTE ---
Preferences preferences;

// Configurações WiFi
String savedSsid = "";
String savedPassword = "";
String savedDeviceId = "";
String savedProcessId = "";

// Parâmetros de segurança/iniciais
String savedDegeloModo = "desativado";
String savedDegeloTempo = "00:00";
float savedDegeloTemperatura = 5.0;
float savedTemperaturaMinSeguranca = 0.0;
float savedTemperaturaMaxSeguranca = 35.0;
float savedTemperaturaAlvoLocal = 20.0;
float savedVariacaoTemperaturaLocal = 0.5;

// Status da conexão WiFi
bool wifiConnected = false;
bool apModeActive = false;
int wifiErrorCount = 0;
const int MAX_WIFI_ERRORS = 5;

// Status do processo ativo
bool processFound = false;

// Estados atuais dos relés
bool currentRelayAquecimentoState = false;
bool currentRelayResfriamentoState = false;
bool currentRelayDegeloState = false;

// Armazenamento de logs
String logs[MAX_LOGS];
int currentLogIndex = 0;
bool logBufferFull = false;

// Timers
unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_READ_INTERVAL_MS = 30000;
const unsigned long OFFLINE_RETRY_INTERVAL_MS = 60000;
unsigned long lastOfflineRetryTime = 0;
unsigned long buttonPressStartTime = 0;

// Servidor Web para API
WebServer server(80);

// --- FUNÇÕES AUXILIARES ---
void addLog(String message) {
  logs[currentLogIndex] = message;
  currentLogIndex = (currentLogIndex + 1) % MAX_LOGS;
  if (currentLogIndex == 0) logBufferFull = true;
  
  if (DEBUG_MODE) {
    Serial.println(message);
  }
}

void saveConfigurations() {
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

void loadConfigurations() {
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
  addLog("SSID: " + savedSsid + ", Device ID: " + savedDeviceId + ", Process ID: " + savedProcessId);
}

void clearConfigurations() {
  preferences.begin("fermenstation", false);
  preferences.clear();
  preferences.end();
  addLog("Todas as configurações foram limpas.");
}

void setRelayState(int relayPin, bool state) {
  digitalWrite(relayPin, state ? HIGH : LOW);
}

float readDSTemperature(DeviceAddress sensorAddress) {
  sensors.requestTemperatures();
  float tempC = sensors.getTempC(sensorAddress);
  if (tempC == DEVICE_DISCONNECTED_C) {
    addLog("Erro ao ler sensor de temperatura.");
    return -127.0;
  }
  return tempC;
}

// --- FUNÇÕES DE COMUNICAÇÃO COM SUPABASE ---
String callSupabaseRpc(const String &rpcName, const String &payload) {
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rpc/" + rpcName;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));

  addLog("Chamando RPC: " + rpcName + " com payload: " + payload);
  int httpResponseCode = http.POST(payload);
  String response = "";

  if (httpResponseCode > 0) {
    response = http.getString();
    addLog("Resposta RPC (" + String(httpResponseCode) + "): " + response);
  } else {
    addLog("Erro na chamada RPC (" + String(httpResponseCode) + "): " + http.errorToString(httpResponseCode));
  }
  http.end();
  return response;
}

bool validateDeviceOnSupabase() {
  if (savedDeviceId.length() == 0) {
    addLog("Device ID não configurado. Não é possível validar no Supabase.");
    return false;
  }
  
  JsonDocument  doc;
  doc["p_device_id"] = savedDeviceId;
  String payload;
  serializeJson(doc, payload);

  String response = callSupabaseRpc("rpc_validate_device", payload);

  JsonDocument  responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);

  if (error) {
    addLog("Erro ao parsear resposta de validação do dispositivo: " + String(error.f_str()));
    return false;
  }

  if (responseDoc["status"] == "success") {
    addLog("Dispositivo validado com sucesso no Supabase.");
    return true;
  } else {
    addLog("Falha na validação do dispositivo no Supabase: " + responseDoc["message"].as<String>());
    return false;
  }
}

bool getActiveProcessOnSupabase() {
  if (savedDeviceId.length() == 0) {
    addLog("Device ID não configurado. Não é possível buscar processo ativo.");
    return false;
  }
  
  JsonDocument doc;
  doc["p_device_id"] = savedDeviceId;
  String payload;
  serializeJson(doc, payload);

  String response = callSupabaseRpc("rpc_get_active_process", payload);

  JsonDocument  responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);

  if (error) {
    addLog("Erro ao parsear resposta de processo ativo: " + String(error.f_str()));
    return false;
  }

  if (responseDoc["process_found"] == true) {
    savedProcessId = responseDoc["process_id"].as<String>();
    savedTemperaturaAlvoLocal = responseDoc["temperatura_alvo_receita"] | 20.0;
    savedVariacaoTemperaturaLocal = responseDoc["variacao_aceitavel_receita"] | 0.5;

    addLog("Processo ativo encontrado: " + savedProcessId);
    saveConfigurations();
    return true;
  } else {
    addLog("Nenhum processo ativo encontrado: " + responseDoc["message"].as<String>());
    savedProcessId = "";
    saveConfigurations();
    return false;
  }
}

void controlFermenstationOnSupabase(float tempFermentador, float tempAmbiente, float tempDegelo, float gravidade = -1.0) {
  if (!processFound || savedProcessId.length() == 0) {
    addLog("Nenhum processo ativo ou ID do processo. Não é possível controlar via Supabase.");
    return;
  }

  JsonDocument doc;
  doc["p_device_id"] = savedDeviceId;
  doc["p_processo_id"] = savedProcessId;
  doc["p_temp_fermentador"] = tempFermentador;
  doc["p_temp_ambiente"] = tempAmbiente;
  doc["p_temp_degelo"] = tempDegelo;
  if (gravidade != -1.0) {
    doc["p_gravidade"] = gravidade;
  }
  String payload;
  serializeJson(doc, payload);

  String response = callSupabaseRpc("rpc_controlar_fermentacao", payload);

  JsonDocument responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);

  if (error) {
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

// --- LÓGICA DE CONTROLE LOCAL (OFFLINE / FALLBACK) ---
void localControlLogic(float tempFermentador, float tempAmbiente, float tempDegelo) {
  addLog("Executando lógica de controle LOCAL (offline/fallback).");

  currentRelayAquecimentoState = false;
  currentRelayResfriamentoState = false;
  currentRelayDegeloState = false;
  String acaoLocal = "Nenhuma ação local necessária.";

  if (savedDegeloModo == "por_temperatura") {
    if (tempDegelo < savedDegeloTemperatura) {
      currentRelayDegeloState = true;
      acaoLocal = "Degelo por temperatura ativado (local).";
    }
  }

  if (currentRelayDegeloState) {
    currentRelayAquecimentoState = false;
    currentRelayResfriamentoState = false;
  } else {
    if (tempFermentador < savedTemperaturaMinSeguranca) {
      currentRelayAquecimentoState = true;
      currentRelayResfriamentoState = false;
      acaoLocal = "Aquecimento de SEGURANÇA (temp muito baixa - local).";
    } else if (tempFermentador > savedTemperaturaMaxSeguranca) {
      currentRelayAquecimentoState = false;
      currentRelayResfriamentoState = true;
      acaoLocal = "Resfriamento de SEGURANÇA (temp muito alta - local).";
    } else {
      if (tempFermentador < (savedTemperaturaAlvoLocal - savedVariacaoTemperaturaLocal)) {
        currentRelayAquecimentoState = true;
        currentRelayResfriamentoState = false;
        acaoLocal = "Aquecimento (temp abaixo do alvo - local).";
      } else if (tempFermentador > (savedTemperaturaAlvoLocal + savedVariacaoTemperaturaLocal)) {
        currentRelayAquecimentoState = false;
        currentRelayResfriamentoState = true;
        acaoLocal = "Resfriamento (temp acima do alvo - local).";
      } else {
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
void handleGetConfig() {
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
  server.send(200, "application/json", response);
}

void handleGetLogs() {
  JsonDocument doc;
  JsonArray logArray = doc.to<JsonArray>();
  
  if (logBufferFull) {
    // Se o buffer está cheio, começa do currentLogIndex até o final e depois do início até currentLogIndex-1
    for (int i = currentLogIndex; i < MAX_LOGS; i++) {
      logArray.add(logs[i]);
    }
    for (int i = 0; i < currentLogIndex; i++) {
      logArray.add(logs[i]);
    }
  } else {
    // Se o buffer não está cheio, apenas do início até currentLogIndex-1
    for (int i = 0; i < currentLogIndex; i++) {
      logArray.add(logs[i]);
    }
  }
  
  String response;
  serializeJson(logArray, response);
  server.send(200, "application/json", response);
}

void handleGetCurrentReadings() {
  float tempFermentador = readDSTemperature(tempFermentadorAddress);
  float tempAmbiente = readDSTemperature(tempAmbienteAddress);
  float tempDegelo = readDSTemperature(tempDegeloAddress);
  
  JsonDocument doc;
  doc["tempFermentador"] = tempFermentador;
  doc["tempAmbiente"] = tempAmbiente;
  doc["tempDegelo"] = tempDegelo;
  doc["releAquecimento"] = currentRelayAquecimentoState;
  doc["releResfriamento"] = currentRelayResfriamentoState;
  doc["releDegelo"] = currentRelayDegeloState;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSaveConfig() {

  addLog(server.hasHeader("Content-Type") ? "Recebendo configuração via POST." : "Recebendo configuração sem Content-Type.");
  addLog(server.header("Content-Type").c_str());

  addLog("Todos os headers recebidos:");
  for (int i = 0; i < server.headers(); i++) {
  addLog((String(i) + ": " + server.headerName(i) + "=" + server.header(i)).c_str());
  }

  if (server.hasHeader("Content-Type") && server.header("Content-Type") == "application/json") {
    String body = server.arg("plain");
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
      server.send(400, "text/plain", "Erro ao parsear JSON: " + String(error.c_str()));
      return;
    }
    
    addLog(doc["ssid"].as<String>());

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
    server.send(200, "application/json", "{\"status\":\"success\", \"message\":\"Configurações salvas com sucesso\"}");
  } else {
    server.send(400, "text/plain", "Content-Type deve ser application/json");
  }
}

void handleResetConfig() {
  clearConfigurations();
  server.send(200, "application/json", "{\"status\":\"success\", \"message\":\"Configurações resetadas. Reinicie o dispositivo.\"}");
}

void handleRestartDevice() {
  server.send(200, "application/json", "{\"status\":\"success\", \"message\":\"Dispositivo reiniciando...\"}");
  delay(1000);
  ESP.restart();
}

void handleNotFound() {
  String message = "Endpoint não encontrado\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\n";
  
  server.send(404, "text/plain", message);
}


void setupAPIEndpoints() {
  // Endpoints GET
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/logs", HTTP_GET, handleGetLogs);
  server.on("/api/readings", HTTP_GET, handleGetCurrentReadings);
  
  // Endpoints POST
  server.on("/api/config", HTTP_POST, handleSaveConfig);
  server.on("/api/reset", HTTP_POST, handleResetConfig);
  server.on("/api/restart", HTTP_POST, handleRestartDevice);
  
  server.onNotFound(handleNotFound);
}

void startAPMode() {
  apModeActive = true;
  WiFi.mode(WIFI_AP);

  // Configurar o AP com IP específico *********
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  
  // Gera um SSID único baseado no endereço MAC
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String apSSID = "FermenStation_" + String(mac[4], HEX) + String(mac[5], HEX);
  
  // Configurar o servidor web
  WiFi.softAP(apSSID.c_str());
  addLog("Modo AP iniciado. SSID: " + apSSID + " IP: " + WiFi.softAPIP().toString());
  addLog("Aguardando configuração do WiFi...");

  // Definir faixa de IP (opcional)************
  // Isso define o intervalo de IPs que serão atribuídos aos clientes  
  //IPAddress myIP = WiFi.softAPIP();
  //addLog("Modo AP iniciado. SSID: " + apSSID + " IP: " + myIP.toString());
  
  // Configura os endpoints da API
  setupAPIEndpoints();
  server.begin();
}

// --- FUNÇÕES DE CONEXÃO WIFI ---
void connectToWiFi() {
  if (apModeActive) {
    addLog("Já está no modo AP. Não é possível conectar ao WiFi.");
    return;
  }
  addLog("Tentando conectar ao WiFi...");
  if (savedSsid.length() == 0 || savedPassword.length() == 0) {
    addLog("Nenhuma credencial WiFi salva. Iniciando modo AP para configuração...");
    startAPMode();
    return;
  }
  
  addLog("Conectando ao WiFi: " + savedSsid);
  WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    apModeActive = false;
    addLog("\nWiFi conectado! IP: " + WiFi.localIP().toString());
    
    if (validateDeviceOnSupabase()) {
      processFound = getActiveProcessOnSupabase();
    } else {
      savedDeviceId = "";
      saveConfigurations();
      addLog("Falha na validação do dispositivo. Modo offline.");
    }
  } else {
    wifiConnected = false;
    addLog("\nFalha ao conectar ao WiFi. Iniciando modo AP para configuração...");
    startAPMode();
  }
}
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    addLog("WiFi desconectado. Tentando reconectar...");
    connectToWiFi();
  } else {
    wifiConnected = true;
  }
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(100);
  addLog("Iniciando FermenStation...");

  // Inicializa array de logs
  for (int i = 0; i < MAX_LOGS; i++) {
    logs[i] = "";
  }

  // Configura pinos dos relés
  pinMode(RELAY_PIN_AQUECIMENTO, OUTPUT);
  pinMode(RELAY_PIN_RESFRIAMENTO, OUTPUT);
  pinMode(RELAY_PIN_DEGELO, OUTPUT);
  setRelayState(RELAY_PIN_AQUECIMENTO, false);
  setRelayState(RELAY_PIN_RESFRIAMENTO, false);
  setRelayState(RELAY_PIN_DEGELO, false);

  // Configura o pino do botão de reset
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Inicializa sensores de temperatura
  sensors.begin();
  if (!sensors.getAddress(tempFermentadorAddress, 0)) addLog("Não encontrou sensor de fermentador.");
  if (!sensors.getAddress(tempAmbienteAddress, 1)) addLog("Não encontrou sensor de ambiente.");
  if (!sensors.getAddress(tempDegeloAddress, 2)) addLog("Não encontrou sensor de degelo.");

  // Carrega configurações persistentes
  loadConfigurations();

  // Tenta conectar ao WiFi
  connectToWiFi();
}

// --- LOOP PRINCIPAL ---
void loop() {
  // Verifica botão de reset físico
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
      addLog("Botão de reset pressionado...");
    } else if (millis() - buttonPressStartTime >= RESET_BUTTON_HOLD_TIME_MS) {
      addLog("Botão de reset segurado por " + String(RESET_BUTTON_HOLD_TIME_MS / 1000) + " segundos. Limpando configurações...");
      clearConfigurations();
    }
  } else {
    buttonPressStartTime = 0;
  }

  // Se estiver no modo AP, trata as requisições da API
  if (apModeActive) {
    server.handleClient();
  }

  // Verifica reconexão WiFi se estiver offline
  if (!wifiConnected && millis() - lastOfflineRetryTime >= OFFLINE_RETRY_INTERVAL_MS) {
    lastOfflineRetryTime = millis();
    if (!apModeActive) {
      addLog("Tentando reconectar ao WiFi...");
      connectToWiFi();
    }
  }

  // Leitura dos sensores e controle
  float tempFermentador = readDSTemperature(tempFermentadorAddress);
  float tempAmbiente = readDSTemperature(tempAmbienteAddress);
  float tempDegelo = readDSTemperature(tempDegeloAddress);
  float gravidade = -1.0;

  // Simulação de leituras se os sensores não estiverem conectados
  if (tempFermentador == -127.0) tempFermentador = 25.0 + sin(millis() / 10000.0) * 2.0;
  if (tempAmbiente == -127.0) tempAmbiente = 26.5 + cos(millis() / 15000.0) * 1.5;
  if (tempDegelo == -127.0) tempDegelo = 4.0 + sin(millis() / 8000.0) * 1.0;

  if (millis() - lastSensorReadTime >= SENSOR_READ_INTERVAL_MS) {
    lastSensorReadTime = millis();

    addLog("Temperaturas lidas: Fermentador=" + String(tempFermentador) + "°C, Ambiente=" + String(tempAmbiente) + "°C, Degelo=" + String(tempDegelo) + "°C");

    if (wifiConnected && processFound && savedProcessId.length() > 0) {
      controlFermenstationOnSupabase(tempFermentador, tempAmbiente, tempDegelo, gravidade);
    } else {
      addLog("Modo Offline/Fallback: Sem WiFi ou processo ativo. Usando controle local.");
      localControlLogic(tempFermentador, tempAmbiente, tempDegelo);
    }
  }
}