#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

#define DEBUG_MODE true
#define MAX_LOGS 60
#define WIFI_TIMEOUT 30000 // 30 segundos

extern const char *SUPABASE_URL;
extern const char *SUPABASE_ANON_KEY;

extern const int PINO_FERMENTADOR;
extern const int PINO_AMBIENTE;
extern const int PINO_DEGELO;
extern const int RELAY_PIN_AQUECIMENTO;
extern const int RELAY_PIN_RESFRIAMENTO;
extern const int RELAY_PIN_DEGELO;
extern const int RESET_BUTTON_PIN;
extern const unsigned long RESET_BUTTON_HOLD_TIME_MS;

extern Preferences preferences;
extern String savedSsid;
extern String savedPassword;
extern String savedDeviceId;
extern String savedProcessId;
extern String savedDegeloModo;
extern String savedDegeloTempo;
extern float savedDegeloTemperatura;
extern float savedTemperaturaMinSeguranca;
extern float savedTemperaturaMaxSeguranca;
extern float savedTemperaturaAlvoLocal;
extern float savedVariacaoTemperaturaLocal;
extern bool wifiConnected;
extern bool apModeActive;
extern bool processFound;
extern int wifiErrorCount;
extern const int MAX_WIFI_ERRORS;
extern bool currentRelayAquecimentoState;
extern bool currentRelayResfriamentoState;
extern bool currentRelayDegeloState;
extern String logs[MAX_LOGS];
extern int currentLogIndex;
extern bool logBufferFull;
extern unsigned long lastSensorReadTime;
extern const unsigned long SENSOR_READ_INTERVAL_MS;
extern const unsigned long OFFLINE_RETRY_INTERVAL_MS;
extern unsigned long lastOfflineRetryTime;
extern unsigned long buttonPressStartTime;

#endif // CONFIG_H 