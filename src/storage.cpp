#include "storage.h"
#include "config.h"
#include "log.h"

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
}

void clearConfigurations() {
    preferences.begin("fermenstation", false);
    preferences.clear();
    preferences.end();
    addLog("Todas as configurações foram limpas.");
} 