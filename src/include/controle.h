#ifndef CONTROLE_H
#define CONTROLE_H

void setRelayState(int relayPin, bool state);
float readDSTemperature(DeviceAddress sensorAddress, DallasTemperature &sensorInstance);
void debugAllSensors();
void localControlLogic(float tempFermentador, float tempAmbiente, float tempDegelo);

#endif // CONTROLE_H 