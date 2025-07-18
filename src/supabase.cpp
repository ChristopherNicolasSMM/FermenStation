#include "supabase.h"
#include "config.h"
#include "log.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "storage.h"
#include "controle.h"

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
    JsonDocument doc;
    doc["p_device_id"] = savedDeviceId;
    String payload;
    serializeJson(doc, payload);
    String response = callSupabaseRpc("rpc_validate_device", payload);
    JsonDocument responseDoc;
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
    JsonDocument responseDoc;
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

void controlFermenstationOnSupabase(float tempFermentador, float tempAmbiente, float tempDegelo, float gravidade) {
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