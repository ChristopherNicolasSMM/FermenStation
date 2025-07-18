#ifndef SUPABASE_H
#define SUPABASE_H

#include <Arduino.h>

String callSupabaseRpc(const String &rpcName, const String &payload);
bool validateDeviceOnSupabase();
bool getActiveProcessOnSupabase();
void controlFermenstationOnSupabase(float tempFermentador, float tempAmbiente, float tempDegelo, float gravidade = -1.0);

#endif // SUPABASE_H 