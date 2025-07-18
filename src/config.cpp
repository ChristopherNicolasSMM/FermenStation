#include "include/config.h"

const char *SUPABASE_URL = "https://ftycjulkovsffzdxpcwf.supabase.co/rest/v1";
const char *SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImZ0eWNqdWxrb3ZzZmZ6ZHhwY3dmIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTE0NTU4NDEsImV4cCI6MjA2NzAzMTg0MX0.vreBc6iAqfgampJMrbFx_6AUO6PtjuOMjJVGCvuyu-Q";

const int PINO_FERMENTADOR = 16;
const int PINO_AMBIENTE = 17;
const int PINO_DEGELO = 18;
const int RELAY_PIN_AQUECIMENTO = 27;
const int RELAY_PIN_RESFRIAMENTO = 26;
const int RELAY_PIN_DEGELO = 25;
const int RESET_BUTTON_PIN = 0;
const unsigned long RESET_BUTTON_HOLD_TIME_MS = 5000;

Preferences preferences;
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