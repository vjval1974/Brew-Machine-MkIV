'use strict';

// 1-Wire HAL — DS18B20 temperature sensors via the Linux kernel `w1-therm`
// module. Replaces the bit-banged STM32 driver in `drivers/ds1820.c`.
//
// Each sensor exposes /sys/bus/w1/devices/28-xxxxx/w1_slave which contains:
//   76 01 4b 46 7f ff 0c 10 e1 : crc=e1 YES
//   76 01 4b 46 7f ff 0c 10 e1 t=23375
// We read it on demand and parse the `t=` value (millidegrees Celsius).

const fs = require('fs').promises;
const path = require('path');
const log = require('../util/logger');
const { oneWire } = require('../../config/pinmap');

const MOCK = process.env.MOCK_HARDWARE === '1';

const lastTemp = new Map();       // name -> last good temp
const lastReadAt = new Map();     // name -> timestamp
const mockTemps = new Map();      // name -> mock value

function _mockSetTemp(name, value) {
  mockTemps.set(name, value);
}

async function readSensor(name) {
  const rom = oneWire.sensors[name];
  if (!rom) throw new Error(`Unknown 1-Wire sensor: ${name}`);

  if (MOCK) {
    return mockTemps.has(name) ? mockTemps.get(name) : 20.0;
  }

  const file = path.join(oneWire.busPath, rom, 'w1_slave');
  try {
    const data = await fs.readFile(file, 'utf8');
    if (!data.includes('YES')) {
      log.warn(`1-wire ${name}: CRC NO, returning last value`);
      return lastTemp.has(name) ? lastTemp.get(name) : NaN;
    }
    const match = data.match(/t=(-?\d+)/);
    if (!match) {
      log.warn(`1-wire ${name}: malformed payload`);
      return lastTemp.has(name) ? lastTemp.get(name) : NaN;
    }
    const tC = parseInt(match[1], 10) / 1000;
    lastTemp.set(name, tC);
    lastReadAt.set(name, Date.now());
    return tC;
  } catch (err) {
    log.warn(`1-wire ${name}: ${err.message}`);
    return lastTemp.has(name) ? lastTemp.get(name) : NaN;
  }
}

// Helper that returns the last cached value if it's <maxAgeMs old, otherwise
// re-reads. Cuts I/O when many UI clients poll concurrently.
async function readCached(name, maxAgeMs = 800) {
  const age = Date.now() - (lastReadAt.get(name) || 0);
  if (age < maxAgeMs && lastTemp.has(name)) return lastTemp.get(name);
  return readSensor(name);
}

module.exports = { readSensor, readCached, _mockSetTemp };
