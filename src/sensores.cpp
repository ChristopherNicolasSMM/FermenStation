#include "include/sensores.h"
#include "include/config.h"

OneWire oneWireFermentador(PINO_FERMENTADOR);
OneWire oneWireAmbiente(PINO_AMBIENTE);
OneWire oneWireDegelo(PINO_DEGELO);

DallasTemperature sensorFermentador(&oneWireFermentador);
DallasTemperature sensorAmbiente(&oneWireAmbiente);
DallasTemperature sensorDegelo(&oneWireDegelo);

DeviceAddress tempFermentadorAddress;
DeviceAddress tempAmbienteAddress;
DeviceAddress tempDegeloAddress; 