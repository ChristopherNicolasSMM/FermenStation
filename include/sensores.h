#ifndef SENSORES_H
#define SENSORES_H

#include <OneWire.h>
#include <DallasTemperature.h>

extern OneWire oneWireFermentador;
extern OneWire oneWireAmbiente;
extern OneWire oneWireDegelo;
extern DallasTemperature sensorFermentador;
extern DallasTemperature sensorAmbiente;
extern DallasTemperature sensorDegelo;
extern DeviceAddress tempFermentadorAddress;
extern DeviceAddress tempAmbienteAddress;
extern DeviceAddress tempDegeloAddress;

#endif // SENSORES_H 