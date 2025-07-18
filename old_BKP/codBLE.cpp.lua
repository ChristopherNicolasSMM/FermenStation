// Inclui as bibliotecas necessárias para Wi-Fi, Bluetooth Low Energy (BLE), HTTP e armazenamento persistente.
#include <WiFi.h>
#include <HTTPClient.h>        // Para comunicação com o Supabase via HTTP
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>       // Para lidar com JSON nas requisições e respostas
#include <Preferences.h>       // Para armazenar configurações de forma persistente
#include <OneWire.h>           // Para sensores DS18B20
#include <DallasTemperature.h> // Para sensores DS18B20

// --- DEFINIÇÕES GLOBAIS E CONSTANTES ---
#define DEBUG_MODE true // Define se os logs BLE serão enviados apenas em debug

// --- CONFIGURAÇÕES DO SUPABASE ---
// Substitua pelo URL do seu projeto Supabase e sua Anon Key.
// Exemplo: https://<SEU_PROJETO_ID>.supabase.co
const char *SUPABASE_URL = "https://ftycjulkovsffzdxpcwf.supabase.co/rest/v1";
const char *SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImZ0eWNqdWxrb3ZzZmZ6ZHhwY3dmIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTE0NTU4NDEsImV4cCI6MjA2NzAzMTg0MX0.vreBc6iAqfgampJMrbFx_6AUO6PtjuOMjJVGCvuyu-Q";

// --- DEFINIÇÕES BLE ---
// UUIDs para o serviço e características BLE
// Você pode gerar UUIDs únicos em https://www.uuidgenerator.net/
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"      // UUID do Serviço
#define CONFIG_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Característica para receber configurações (SSID, Senha, Device ID, Recipe ID)
#define STATUS_CHAR_UUID "ae06e121-657c-473d-9861-59424e6005f7"  // Característica para enviar status e erros
#define RESET_CHAR_UUID "a1b2c3d4-e5f6-7890-1234-567890abcdef"  // Característica para comando de reset

BLEServer *pServer = NULL;
BLECharacteristic *pConfigCharacteristic = NULL;
BLECharacteristic *pStatusCharacteristic = NULL;
BLECharacteristic *pResetCharacteristic = NULL;
bool deviceConnected = false;    // Indica se um cliente BLE está conectado
bool oldDeviceConnected = false; // Usado para detectar mudanças no estado de conexão BLE

// --- DEFINIÇÕES DOS PINOS ---
// Pinos dos sensores de temperatura (DS18B20 - OneWire)
const int ONE_WIRE_BUS = 4; // Pino GPIO para o barramento OneWire
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Endereços dos sensores DS18B20 (você precisará descobrir os seus)
// Se você não souber, pode usar o exemplo "DallasTemperature/OneWireSearch" para encontrá-los.
DeviceAddress tempFermentadorAddress;
DeviceAddress tempAmbienteAddress;
DeviceAddress tempDegeloAddress;

// Pinos dos relés
const int RELAY_PIN_AQUECIMENTO = 27; // Exemplo de pino GPIO
const int RELAY_PIN_RESFRIAMENTO = 26;
const int RELAY_PIN_DEGELO = 25;

// Pino do botão de reset físico
const int RESET_BUTTON_PIN = 0;                       // GPIO 0 é um pino comum para o botão BOOT em muitas placas ESP32
const unsigned long RESET_BUTTON_HOLD_TIME_MS = 5000; // 5 segundos para reset

// --- VARIÁVEIS DE ESTADO E CONFIGURAÇÃO PERSISTENTE ---
Preferences preferences; // Objeto para gerenciar o armazenamento persistente

// Configurações Wi-Fi e IDs
String savedSsid = "";
String savedPassword = "";
String savedDeviceId = "";  // UUID do dispositivo no Supabase
String savedProcessId = ""; // UUID do processo ativo no Supabase (se houver)

// Parâmetros de segurança/iniciais (para controle local offline)
String savedDegeloModo = "desativado";     // 'desativado', 'agendado', 'por_temperatura'
String savedDegeloTempo = "00:00";         // Formato HH:MM
float savedDegeloTemperatura = 5.0;        // Temperatura para degelo por temperatura
float savedTemperaturaMinSeguranca = 0.0;  // Limite mínimo de segurança
float savedTemperaturaMaxSeguranca = 35.0; // Limite máximo de segurança
float savedTemperaturaAlvoLocal = 20.0;    // Temperatura alvo padrão para offline
float savedVariacaoTemperaturaLocal = 0.5; // Variação aceitável padrão para offline

// Status da conexão Wi-Fi
bool wifiConnected = false;
int wifiErrorCount = 0;
const int MAX_WIFI_ERRORS = 5; // Número máximo de tentativas de erro antes de habilitar BLE

// Status do processo ativo
bool processFound = false;

// Estados atuais dos relés
bool currentRelayAquecimentoState = false;
bool currentRelayResfriamentoState = false;
bool currentRelayDegeloState = false;

// Timers
unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_READ_INTERVAL_MS = 30000;   // Intervalo de leitura e envio para Supabase (30 segundos)
const unsigned long OFFLINE_RETRY_INTERVAL_MS = 60000; // Intervalo de re-tentativa no modo offline (1 minuto)
unsigned long lastOfflineRetryTime = 0;
unsigned long buttonPressStartTime = 0; // Para detectar o tempo de pressionamento do botão de reset

// --- FUNÇÕES AUXILIARES ---

// Função para enviar status via Serial e BLE (se conectado e em modo debug)
void sendLog(String message, bool isBLE = false)
{
  Serial.println(message);
  if (isBLE && deviceConnected && DEBUG_MODE)
  {
    pStatusCharacteristic->setValue(message.c_str());
    pStatusCharacteristic->notify();
  }
}

// Função para salvar configurações na memória persistente
void saveConfigurations()
{
  preferences.begin("fermenstation", false); // 'fermenstation' é o namespace
  preferences.putString("ssid", savedSsid);
  preferences.putString("password", savedPassword);
  preferences.putString("deviceId", savedDeviceId);
  preferences.putString("processId", savedProcessId);

  // Salvar parâmetros de controle local
  preferences.putString("degeloModo", savedDegeloModo);
  preferences.putString("degeloTempo", savedDegeloTempo);
  preferences.putFloat("degeloTemp", savedDegeloTemperatura);
  preferences.putFloat("tempMinSeg", savedTemperaturaMinSeguranca);
  preferences.putFloat("tempMaxSeg", savedTemperaturaMaxSeguranca);
  preferences.putFloat("tempAlvoLocal", savedTemperaturaAlvoLocal);
  preferences.putFloat("variacaoTempLocal", savedVariacaoTemperaturaLocal);

  preferences.end();
  sendLog("Configurações salvas na memória persistente.");
}

// Função para carregar configurações da memória persistente
void loadConfigurations()
{
  preferences.begin("fermenstation", false);
  savedSsid = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");
  savedDeviceId = preferences.getString("deviceId", "");
  savedProcessId = preferences.getString("processId", "");

  // Carregar parâmetros de controle local
  savedDegeloModo = preferences.getString("degeloModo", "desativado");
  savedDegeloTempo = preferences.getString("degeloTempo", "00:00");
  savedDegeloTemperatura = preferences.getFloat("degeloTemp", 5.0);
  savedTemperaturaMinSeguranca = preferences.getFloat("tempMinSeg", 0.0);
  savedTemperaturaMaxSeguranca = preferences.getFloat("tempMaxSeg", 35.0);
  savedTemperaturaAlvoLocal = preferences.getFloat("tempAlvoLocal", 20.0);
  savedVariacaoTemperaturaLocal = preferences.getFloat("variacaoTempLocal", 0.5);

  preferences.end();
  sendLog("Configurações carregadas da memória persistente.");
  sendLog("SSID: " + savedSsid + ", Device ID: " + savedDeviceId + ", Process ID: " + savedProcessId);
  sendLog("Modo Degelo Local: " + savedDegeloModo + ", Temp Min Seg: " + String(savedTemperaturaMinSeguranca));
}

// Função para limpar todas as configurações salvas
void clearConfigurations()
{
  preferences.begin("fermenstation", false);
  preferences.clear(); // Limpa todas as chaves no namespace
  preferences.end();
  sendLog("Todas as configurações foram limpas. Reiniciando para modo de provisionamento BLE.");
  ESP.restart(); // Reinicia o ESP32
}

// Função para controlar o estado físico dos relés
void setRelayState(int relayPin, bool state)
{
  digitalWrite(relayPin, state ? HIGH : LOW); // HIGH para ligar, LOW para desligar (assumindo relé ativo em HIGH)
  // Se o seu relé for ativo em LOW, inverta a lógica: digitalWrite(relayPin, state ? LOW : HIGH);
}

// Função para ler temperatura de um sensor DS18B20 específico
float readDSTemperature(DeviceAddress sensorAddress)
{
  sensors.requestTemperatures(); // Envia o comando para todos os sensores no barramento
  float tempC = sensors.getTempC(sensorAddress);
  if (tempC == DEVICE_DISCONNECTED_C)
  {
    sendLog("Erro ao ler sensor de temperatura.");
    return -127.0; // Valor de erro padrão para DS18B20
  }
  return tempC;
}

// --- FUNÇÕES DE COMUNICAÇÃO COM SUPABASE ---

// Função para chamar uma função RPC do Supabase
String callSupabaseRpc(const String &rpcName, const String &payload)
{
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/rpc/" + rpcName;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY)); // Adiciona o token de autenticação

  sendLog("Chamando RPC: " + rpcName + " com payload: " + payload);
  int httpResponseCode = http.POST(payload);
  String response = "";

  if (httpResponseCode > 0)
  {
    response = http.getString();
    sendLog("Resposta RPC (" + String(httpResponseCode) + "): " + response);
  }
  else
  {
    sendLog("Erro na chamada RPC (" + String(httpResponseCode) + "): " + http.errorToString(httpResponseCode));
  }
  http.end();
  return response;
}

// Função para validar o dispositivo no Supabase
bool validateDeviceOnSupabase()
{
  if (savedDeviceId.length() == 0)
  {
    sendLog("Device ID não configurado. Não é possível validar no Supabase.");
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
    sendLog("Erro ao parsear resposta de validação do dispositivo: " + String(error.f_str()));
    return false;
  }

  if (responseDoc["status"] == "success")
  {
    sendLog("Dispositivo validado com sucesso no Supabase.");
    return true;
  }
  else
  {
    sendLog("Falha na validação do dispositivo no Supabase: " + responseDoc["message"].as<String>());
    return false;
  }
}

// Função para buscar o processo ativo no Supabase
bool getActiveProcessOnSupabase()
{
  if (savedDeviceId.length() == 0)
  {
    sendLog("Device ID não configurado. Não é possível buscar processo ativo.");
    return false;
  }
  JsonDocument doc; // Ou tamanho apropriado; 
  doc["p_device_id"] = savedDeviceId;
  String payload;
  serializeJson(doc, payload);

  String response = callSupabaseRpc("rpc_get_active_process", payload);

  JsonDocument responseDoc; // Maior para incluir dados da receita
  DeserializationError error = deserializeJson(responseDoc, response);

  if (error)
  {
    sendLog("Erro ao parsear resposta de processo ativo: " + String(error.f_str()));
    return false;
  }

  if (responseDoc["process_found"] == true)
  {
    savedProcessId = responseDoc["process_id"].as<String>();
    // Opcional: Salvar parâmetros da receita localmente para fallback
    savedTemperaturaAlvoLocal = responseDoc["temperatura_alvo_receita"] | 20.0;
    savedVariacaoTemperaturaLocal = responseDoc["variacao_aceitavel_receita"] | 0.5;
    // ... carregar outros parâmetros da receita se necessário para o modo offline

    sendLog("Processo ativo encontrado: " + savedProcessId);
    saveConfigurations(); // Salva o processId e os parâmetros da receita para fallback
    return true;
  }
  else
  {
    sendLog("Nenhum processo ativo encontrado: " + responseDoc["message"].as<String>());
    savedProcessId = ""; // Limpa o ID do processo se não houver um ativo
    saveConfigurations();
    return false;
  }
}

// Função principal de controle de fermentação no Supabase
void controlFermenstationOnSupabase(float tempFermentador, float tempAmbiente, float tempDegelo, float gravidade = -1.0)
{
  if (!processFound || savedProcessId.length() == 0)
  {
    sendLog("Nenhum processo ativo ou ID do processo. Não é possível controlar via Supabase.");
    return;
  }

  StaticJsonDocument<512> doc;
  doc["p_device_id"] = savedDeviceId;
  doc["p_processo_id"] = savedProcessId;
  doc["p_temp_fermentador"] = tempFermentador;
  doc["p_temp_ambiente"] = tempAmbiente;
  doc["p_temp_degelo"] = tempDegelo;
  if (gravidade != -1.0)
  { // Apenas inclui gravidade se for um valor válido
    doc["p_gravidade"] = gravidade;
  }
  String payload;
  serializeJson(doc, payload);

  String response = callSupabaseRpc("rpc_controlar_fermentacao", payload);

  StaticJsonDocument<256> responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);

  if (error)
  {
    sendLog("Erro ao parsear resposta de controle de fermentação: " + String(error.f_str()));
    return;
  }

  // Atualiza o estado dos relés com base na resposta do Supabase
  currentRelayAquecimentoState = responseDoc["releAquecimento"] | false;
  currentRelayResfriamentoState = responseDoc["releResfriamento"] | false;
  currentRelayDegeloState = responseDoc["releDegelo"] | false;

  setRelayState(RELAY_PIN_AQUECIMENTO, currentRelayAquecimentoState);
  setRelayState(RELAY_PIN_RESFRIAMENTO, currentRelayResfriamentoState);
  setRelayState(RELAY_PIN_DEGELO, currentRelayDegeloState);

  sendLog("Relés atualizados pelo Supabase: Aquecimento=" + String(currentRelayAquecimentoState) +
          ", Resfriamento=" + String(currentRelayResfriamentoState) +
          ", Degelo=" + String(currentRelayDegeloState));
  sendLog("Ação tomada pelo Supabase: " + responseDoc["acaoTomada"].as<String>());
}

// --- LÓGICA DE CONTROLE LOCAL (OFFLINE / FALLBACK) ---
void localControlLogic(float tempFermentador, float tempAmbiente, float tempDegelo)
{
  sendLog("Executando lógica de controle LOCAL (offline/fallback).");

  // Resetar estados dos relés para o início da lógica local
  currentRelayAquecimentoState = false;
  currentRelayResfriamentoState = false;
  currentRelayDegeloState = false;
  String acaoLocal = "Nenhuma ação local necessária.";

  // 1. Lógica de Controle de Degelo (Prioritária)
  if (savedDegeloModo == "agendado")
  {
    // Exemplo simplificado: Degelo baseado na hora do dia.
    // Para uma lógica mais robusta, seria necessário um RTC e controle de duração.
    // Usaremos millis() para simular uma duração simples se o degelo for ativado.
    // ATENÇÃO: Para degelo agendado preciso de um RTC ou NTP para a hora exata.
    // Este é um placeholder.
    // if (currentTime.hour() == degeloHora && currentTime.minute() == degeloMinuto) {
    //   currentRelayDegeloState = true;
    //   acaoLocal = "Degelo agendado ativado (local).";
    // }
  }
  else if (savedDegeloModo == "por_temperatura")
  {
    if (tempDegelo < savedDegeloTemperatura)
    {
      currentRelayDegeloState = true;
      acaoLocal = "Degelo por temperatura ativado (local).";
    }
  }

  // Se o degelo estiver ativo, desliga aquecimento e resfriamento.
  if (currentRelayDegeloState)
  {
    currentRelayAquecimentoState = false;
    currentRelayResfriamentoState = false;
  }
  else
  {
    // 2. Lógica de Controle de Temperatura (Fermentador) - Se degelo NÃO estiver ativo.

    // Limites de Segurança (Prioritários sobre Alvo Local)
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
      // Controle por Temperatura Alvo Local (se não estiver em segurança)
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
        // Temperatura dentro do range normal, relés desligados por padrão.
        currentRelayAquecimentoState = false;
        currentRelayResfriamentoState = false;
        acaoLocal = "Temperatura no alvo - relés desligados (local).";
      }
      // A lógica de "Range Especial de Aquecimento" não é implementada no modo offline
      // pois requer dados da receita do Supabase. O modo offline é para segurança básica.
    }
  }

  // Aplica os estados calculados aos pinos dos relés
  setRelayState(RELAY_PIN_AQUECIMENTO, currentRelayAquecimentoState);
  setRelayState(RELAY_PIN_RESFRIAMENTO, currentRelayResfriamentoState);
  setRelayState(RELAY_PIN_DEGELO, currentRelayDegeloState);

  sendLog("Relés atualizados LOCALMENTE: Aquecimento=" + String(currentRelayAquecimentoState) +
          ", Resfriamento=" + String(currentRelayResfriamentoState) +
          ", Degelo=" + String(currentRelayDegeloState));
  sendLog("Ação tomada localmente: " + acaoLocal);
}

// --- CALLBACKS DO SERVIDOR BLE ---
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    sendLog("Dispositivo BLE conectado.", true);
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    sendLog("Dispositivo BLE desconectado.", true);
    // Reinicia a publicidade para permitir novas conexões
    BLEDevice::startAdvertising();
    sendLog("Reiniciando publicidade BLE.", true);
  }
};

// --- CALLBACK PARA RECEBER DADOS NA CARACTERÍSTICA DE CONFIGURAÇÃO (BLE) ---
class MyConfigCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0)
    {
      sendLog("Recebido via BLE: " + String(rxValue.c_str()), true);

      StaticJsonDocument<512> doc; // Tamanho maior para JSON de configuração
      DeserializationError error = deserializeJson(doc, rxValue);

      if (error)
      {
        sendLog("Erro JSON ao parsear config BLE: " + String(error.f_str()), true);
        return;
      }

      // Extrai e salva os parâmetros do JSON
      if (doc.containsKey("ssid"))
      {
        savedSsid = doc["ssid"].as<String>();
      }
      if (doc.containsKey("password"))
      {
        savedPassword = doc["password"].as<String>();
      }
      if (doc.containsKey("deviceId"))
      {
        savedDeviceId = doc["deviceId"].as<String>();
      }
      if (doc.containsKey("recipeId"))
      { // O recipeId pode ser usado para buscar o processo ativo inicial
        // Não salvamos recipeId diretamente, ele é usado para buscar o processo ativo
        // e o processId é que é salvo.
      }

      // Salva os parâmetros de segurança/iniciais recebidos via BLE
      if (doc.containsKey("degeloModo"))
        savedDegeloModo = doc["degeloModo"].as<String>();
      if (doc.containsKey("degeloTempo"))
        savedDegeloTempo = doc["degeloTempo"].as<String>();
      if (doc.containsKey("degeloTemperatura"))
        savedDegeloTemperatura = doc["degeloTemperatura"].as<float>();
      if (doc.containsKey("temperaturaMin"))
        savedTemperaturaMinSeguranca = doc["temperaturaMin"].as<float>();
      if (doc.containsKey("temperaturaMax"))
        savedTemperaturaMaxSeguranca = doc["temperaturaMax"].as<float>();
      if (doc.containsKey("temperaturaAlvo"))
        savedTemperaturaAlvoLocal = doc["temperaturaAlvo"].as<float>();
      if (doc.containsKey("variacaoTemperatura"))
        savedVariacaoTemperaturaLocal = doc["variacaoTemperatura"].as<float>();

      saveConfigurations(); // Salva todas as configurações recebidas

      // Tenta conectar ao Wi-Fi após receber as credenciais
      if (savedSsid.length() > 0 && savedPassword.length() > 0)
      {
        sendLog("Tentando conectar ao Wi-Fi: " + savedSsid, true);
        WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 40)
        { // Tenta por 20 segundos (40 * 500ms)
          delay(500);
          sendLog(".", true);
          retries++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
          wifiConnected = true;
          sendLog("\nWi-Fi conectado! IP: " + WiFi.localIP().toString(), true);
          // Após conectar Wi-Fi, desativa BLE para economizar energia e foca na operação Wi-Fi
          // BLEDevice::deinit(); // Descomente para desativar BLE completamente
          // sendLog("BLE desativado para economizar energia.", true);
        }
        else
        {
          wifiConnected = false;
          sendLog("\nFalha ao conectar ao Wi-Fi. Verifique as credenciais.", true);
        }
      }
      else
      {
        sendLog("SSID e/ou Senha não fornecidos via BLE.", true);
      }
    }
  }
};

// --- CALLBACK PARA RECEBER COMANDO DE RESET (BLE) ---
class MyResetCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue == "RESET")
    {
      sendLog("Comando de RESET recebido via BLE. Limpando configurações...", true);
      clearConfigurations(); // Limpa as configurações e reinicia o ESP32
    }
  }
};

// --- SETUP ---
void setup()
{
  Serial.begin(115200);
  delay(100); // Pequeno delay para estabilizar a serial
  sendLog("Iniciando FermenStation...");

  // Configura pinos dos relés como OUTPUT e os desliga inicialmente
  pinMode(RELAY_PIN_AQUECIMENTO, OUTPUT);
  pinMode(RELAY_PIN_RESFRIAMENTO, OUTPUT);
  pinMode(RELAY_PIN_DEGELO, OUTPUT);
  setRelayState(RELAY_PIN_AQUECIMENTO, false);
  setRelayState(RELAY_PIN_RESFRIAMENTO, false);
  setRelayState(RELAY_PIN_DEGELO, false);

  // Configura o pino do botão de reset como INPUT_PULLUP
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Inicializa sensores de temperatura
  sensors.begin();
  // Se você tiver os endereços dos seus sensores, pode atribuí-los aqui.
  // Caso contrário, use o exemplo OneWireSearch para encontrá-los e substitua.
  // Exemplo:
   if (!sensors.getAddress(tempFermentadorAddress, 0)) sendLog("Não encontrou sensor de fermentador.");
   if (!sensors.getAddress(tempAmbienteAddress, 1)) sendLog("Não encontrou sensor de ambiente.");
   if (!sensors.getAddress(tempDegeloAddress, 2)) sendLog("Não encontrou sensor de degelo.");
  // Para simulação, não usaremos os endereços reais por enquanto.

  // Carrega configurações persistentes
  loadConfigurations();

  // --- LÓGICA DE INICIALIZAÇÃO: Prioriza Wi-Fi, Fallback para BLE ---
  if (savedSsid.length() > 0 && savedPassword.length() > 0)
  {
    sendLog("Credenciais Wi-Fi salvas encontradas. Tentando conectar ao Wi-Fi...");
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 40)
    { // Tenta por 20 segundos
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      wifiConnected = true;
      sendLog("\nWi-Fi conectado! IP: " + WiFi.localIP().toString());
      // Tenta validar dispositivo e buscar processo ativo imediatamente
      if (validateDeviceOnSupabase())
      {
        processFound = getActiveProcessOnSupabase();
      }
      else
      {
        // Se a validação falhar, limpa o deviceId salvo para forçar re-provisionamento
        savedDeviceId = "";
        saveConfigurations();
        sendLog("Falha na validação do dispositivo. Entrando em modo offline/BLE.");
      }
    }
    else
    {
      wifiConnected = false;
      sendLog("\nFalha ao conectar ao Wi-Fi com credenciais salvas. Entrando em modo offline/BLE.");
    }
  }

  // Se Wi-Fi não conectou ou não há credenciais salvas, inicia BLE para provisionamento
  if (!wifiConnected || savedDeviceId.length() == 0)
  {
    sendLog("Iniciando servidor BLE para provisionamento...");
    char bleName[30];
    uint8_t mac[6];
    WiFi.macAddress(mac);                                       // Obtém o endereço MAC do Wi-Fi
    sprintf(bleName, "FermenStation_%02X%02X", mac[4], mac[5]); // Usa os últimos 2 bytes do MAC
    BLEDevice::init(bleName);                                   // Nome do dispositivo BLE

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pConfigCharacteristic = pService->createCharacteristic(
        CONFIG_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pConfigCharacteristic->setCallbacks(new MyConfigCharacteristicCallbacks());

    pStatusCharacteristic = pService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY);
    pStatusCharacteristic->addDescriptor(new BLE2902());

    pResetCharacteristic = pService->createCharacteristic(
        RESET_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pResetCharacteristic->setCallbacks(new MyResetCharacteristicCallbacks());

    pService->start();

    // BLEAdvertising *pAdvertising = BLEDevice::pAdvertising;
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    sendLog("Servidor BLE iniciado. Aguardando conexão para provisionamento...");
    sendLog("Aguardando conexão BLE...", true);
  }
}


// --- LOOP PRINCIPAL ---
void loop()
{
  // Lógica para detecção do botão de reset físico
  if (digitalRead(RESET_BUTTON_PIN) == LOW)
  { // Botão pressionado (LOW se INPUT_PULLUP)
    if (buttonPressStartTime == 0)
    {
      buttonPressStartTime = millis();
      sendLog("Botão de reset pressionado...");
    }
    else if (millis() - buttonPressStartTime >= RESET_BUTTON_HOLD_TIME_MS)
    {
      sendLog("Botão de reset segurado por " + String(RESET_BUTTON_HOLD_TIME_MS / 1000) + " segundos. Limpando configurações...");
      clearConfigurations(); // Limpa e reinicia o ESP32
    }
  }
  else
  {                           // Botão liberado
    buttonPressStartTime = 0; // Reseta o timer
  }

  // Lógica para lidar com a conexão/desconexão BLE
  // NOTA: Estas variáveis 'deviceConnected' e 'oldDeviceConnected' são atualizadas pelos callbacks BLE.
  // A lógica abaixo é para reagir a essas mudanças de estado.
  if (deviceConnected)
  {
    if (!oldDeviceConnected)
    {
      // Ações ao conectar via BLE
      sendLog("Dispositivo BLE conectado. (Loop)", true);
      oldDeviceConnected = true;
    }
  }
  else
  {
    if (oldDeviceConnected)
    {
      // Ações ao desconectar via BLE
      sendLog("Dispositivo BLE desconectado. (Loop)", true);
      oldDeviceConnected = false;
    }
    // Reinicia a publicidade se não estiver ativa e Wi-Fi falhou,
    // mas isso já está dentro do bloco de controle de Wi-Fi abaixo.
    // Pode ser redundante aqui, mas não causa problema.
    // pServer->startAdvertising() é chamado no MyServerCallbacks::onDisconnect
  }

  // Verifica o status da conexão Wi-Fi
  // ... (o restante da sua lógica de Wi-Fi e fallback)

  // --- LÓGICA DE OPERAÇÃO PRINCIPAL ---
  float tempFermentador = readDSTemperature(tempFermentadorAddress); // Leitura real do sensor
  float tempAmbiente = readDSTemperature(tempAmbienteAddress);
  float tempDegelo = readDSTemperature(tempDegeloAddress);
  float gravidade = -1.0; // Placeholder para leitura de gravidade, se houver

  // Simulação de leituras de temperatura se os sensores não estiverem conectados
  if (tempFermentador == -127.0)
    tempFermentador = 25.0 + sin(millis() / 10000.0) * 2.0;
  if (tempAmbiente == -127.0)
    tempAmbiente = 26.5 + cos(millis() / 15000.0) * 1.5;
  if (tempDegelo == -127.0)
    tempDegelo = 4.0 + sin(millis() / 8000.0) * 1.0;

  if (millis() - lastSensorReadTime >= SENSOR_READ_INTERVAL_MS)
  {
    lastSensorReadTime = millis();

    sendLog("Temperaturas lidas: Fermentador=" + String(tempFermentador) + "°C, Ambiente=" + String(tempAmbiente) + "°C, Degelo=" + String(tempDegelo) + "°C");

    if (wifiConnected && processFound && savedProcessId.length() > 0)
    {
      // MODO ONLINE: Controla via Supabase
      controlFermenstationOnSupabase(tempFermentador, tempAmbiente, tempDegelo, gravidade);
    }
    else
    {
      // MODO OFFLINE / FALLBACK: Controla localmente
      sendLog("Modo Offline/Fallback: Sem Wi-Fi ou processo ativo. Usando controle local.");
      localControlLogic(tempFermentador, tempAmbiente, tempDegelo);
    }
  }
}