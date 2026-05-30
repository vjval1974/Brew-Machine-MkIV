// Barrel for the sensing domain.
// Owns: temperature poller for DS18B20s (HLT, MASH). Rolling sample
//       buffer + stability check used by the HLT controller.
// Owns store section: 'temps'.
export * as tempSensors from './tempSensors';
