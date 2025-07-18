#include "config.h"
#include "secrets.h"
// ... existing code ...

// ... existing code ...

const char *SUPABASE_URL = SECRET_SUPABASE_URL;
const char *SUPABASE_ANON_KEY = SECRET_SUPABASE_ANON_KEY;

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