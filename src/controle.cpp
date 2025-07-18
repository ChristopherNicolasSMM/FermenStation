#include "controle.h"
#include "config.h"
#include "sensores.h"
#include "log.h"
#include <DallasTemperature.h>

void setRelayState(int relayPin, bool state) {
    digitalWrite(relayPin, state ? HIGH : LOW);
    addLog("Relé " + String(relayPin) + " definido como " + (state ? "LIGADO" : "DESLIGADO"));
}

float readDSTemperature(DeviceAddress sensorAddress, DallasTemperature &sensorInstance) {
    sensorInstance.requestTemperatures();
    float tempC = sensorInstance.getTempC(sensorAddress);
    if (tempC == DEVICE_DISCONNECTED_C) {
        addLog("Erro ao ler sensor de temperatura.");
        return -127.0;
    }
    return tempC;
}

void debugAllSensors() {
    addLog("[DEBUG] Temperaturas:");
    addLog("Fermentador: " + String(readDSTemperature(tempFermentadorAddress, sensorFermentador)) + "°C");
    addLog("Ambiente: " + String(readDSTemperature(tempAmbienteAddress, sensorAmbiente)) + "°C");
    addLog("Degelo: " + String(readDSTemperature(tempDegeloAddress, sensorDegelo)) + "°C");
}

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