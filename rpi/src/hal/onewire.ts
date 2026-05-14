// 1-Wire HAL — DS18B20 temperature sensors via the Linux kernel `w1-therm`
// module. Replaces the bit-banged STM32 driver in `drivers/ds1820.c`.

import { promises as fs } from 'fs';
import path from 'path';
import log from '../util/logger';
import { oneWire } from '../config/pinmap';

const MOCK = process.env.MOCK_HARDWARE === '1';

const lastTemp   = new Map<string, number>();
const lastReadAt = new Map<string, number>();
const mockTemps  = new Map<string, number>();

export function _mockSetTemp(name: string, value: number): void {
  mockTemps.set(name, value);
}

export async function readSensor(name: string): Promise<number> {
  const rom = oneWire.sensors[name];
  if (!rom) throw new Error(`Unknown 1-Wire sensor: ${name}`);

  if (MOCK) {
    return mockTemps.has(name) ? (mockTemps.get(name) as number) : 20.0;
  }

  const file = path.join(oneWire.busPath, rom, 'w1_slave');
  try {
    const data = await fs.readFile(file, 'utf8');
    if (!data.includes('YES')) {
      log.warn(`1-wire ${name}: CRC NO, returning last value`);
      return lastTemp.has(name) ? (lastTemp.get(name) as number) : NaN;
    }
    const match = data.match(/t=(-?\d+)/);
    if (!match) {
      log.warn(`1-wire ${name}: malformed payload`);
      return lastTemp.has(name) ? (lastTemp.get(name) as number) : NaN;
    }
    const tC = parseInt(match[1]!, 10) / 1000;
    lastTemp.set(name, tC);
    lastReadAt.set(name, Date.now());
    return tC;
  } catch (err) {
    log.warn(`1-wire ${name}: ${(err as Error).message}`);
    return lastTemp.has(name) ? (lastTemp.get(name) as number) : NaN;
  }
}

export async function readCached(name: string, maxAgeMs = 800): Promise<number> {
  const age = Date.now() - (lastReadAt.get(name) ?? 0);
  if (age < maxAgeMs && lastTemp.has(name)) return lastTemp.get(name) as number;
  return readSensor(name);
}

export default { readSensor, readCached, _mockSetTemp };
