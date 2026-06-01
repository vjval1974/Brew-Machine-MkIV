// 1-Wire HAL — DS18B20 temperature sensors via the Linux kernel `w1-therm`
// module. Replaces the bit-banged STM32 driver in `drivers/ds1820.c`.
//
// Sensor ROM assignments (HLT, MASH, ...) come from two sources, merged at
// read time:
//   1. pinmap.oneWire.sensors  — compile-time defaults from src/config/pinmap.ts
//   2. data/onewire-overrides.json — persisted user-set overrides from the
//      Diagnostics tab. Takes precedence.
//
// scan() walks /sys/bus/w1/devices and returns every DS18B20 (28-*) the
// kernel knows about, with its current temperature. The Diagnostics view
// uses this for the "tap to assign" calibration workflow when you wire a
// new probe.

import { promises as fs } from 'fs';
import fsSync from 'fs';
import path from 'path';
import log from '../util/logger';
import { oneWire } from '../../config/pinmap';

const MOCK = process.env.MOCK_HARDWARE === '1';

const lastTemp   = new Map<string, number>();
const lastReadAt = new Map<string, number>();
const mockTemps  = new Map<string, number>();

// Overrides — persisted user assignments. Keyed by logical name (HLT, MASH).
const overrides = new Map<string, string>();

// In MOCK mode we synthesise a couple of "discovered" devices alongside any
// currently-assigned ROMs so the Diagnostics UI's scan/assign flow is
// exercisable without a real bus.
const MOCK_EXTRA_DEVICES = ['28-aaaa111111aa', '28-bbbb222222bb', '28-cccc333333cc'];

const OVERRIDES_PATH = path.join(__dirname, '..', '..', '..', 'data', 'onewire-overrides.json');

function persistOverrides(): void {
  const dir = path.dirname(OVERRIDES_PATH);
  if (!fsSync.existsSync(dir)) fsSync.mkdirSync(dir, { recursive: true });
  fsSync.writeFileSync(OVERRIDES_PATH, JSON.stringify(Object.fromEntries(overrides), null, 2));
}

export function load(): void {
  try {
    if (!fsSync.existsSync(OVERRIDES_PATH)) return;
    const raw = JSON.parse(fsSync.readFileSync(OVERRIDES_PATH, 'utf8')) as Record<string, string>;
    overrides.clear();
    for (const [k, v] of Object.entries(raw)) overrides.set(k, v);
    log.info(`onewire: loaded ${overrides.size} ROM override(s) from ${OVERRIDES_PATH}`);
  } catch (err) {
    log.warn(`onewire: failed to load overrides: ${(err as Error).message}`);
  }
}

/** Resolve a logical sensor name to its current ROM (override > pinmap). */
export function resolveRom(name: string): string | undefined {
  return overrides.get(name) ?? oneWire.sensors[name];
}

/** Snapshot of all logical sensor → ROM assignments. */
export function getAssignments(): Record<string, string> {
  const out: Record<string, string> = {};
  for (const name of Object.keys(oneWire.sensors)) {
    const rom = resolveRom(name);
    if (rom) out[name] = rom;
  }
  for (const [name, rom] of overrides) if (!out[name]) out[name] = rom;
  return out;
}

/** Assign a logical name to a specific ROM and persist. */
export function setSensorRom(name: string, rom: string): void {
  overrides.set(name, rom);
  lastTemp.delete(name);
  lastReadAt.delete(name);
  persistOverrides();
  log.info(`onewire: assigned ${rom} → ${name}`);
}

/** Clear a single override (revert to pinmap default). */
export function clearSensorOverride(name: string): void {
  overrides.delete(name);
  lastTemp.delete(name);
  lastReadAt.delete(name);
  persistOverrides();
  log.info(`onewire: cleared override for ${name}`);
}

/** Clear all overrides. */
export function clearAllOverrides(): void {
  overrides.clear();
  lastTemp.clear();
  lastReadAt.clear();
  persistOverrides();
  log.info('onewire: cleared all ROM overrides');
}

export function _mockSetTemp(name: string, value: number): void {
  mockTemps.set(name, value);
  const rom = resolveRom(name);
  if (rom) mockTemps.set(rom, value);
}

/** Read by logical sensor name (HLT, MASH, ...). */
export async function readSensor(name: string): Promise<number> {
  const rom = resolveRom(name);
  if (!rom) throw new Error(`Unknown 1-Wire sensor: ${name}`);

  if (MOCK) {
    return mockTemps.has(name) ? (mockTemps.get(name) as number)
         : mockTemps.has(rom)  ? (mockTemps.get(rom)  as number)
         : 20.0;
  }
  return readByRom(rom, name);
}

export async function readByRom(rom: string, nameForCache?: string): Promise<number> {
  if (MOCK) {
    return mockTemps.has(rom) ? (mockTemps.get(rom) as number) : 20.0;
  }
  const file = path.join(oneWire.busPath, rom, 'w1_slave');
  try {
    const data = await fs.readFile(file, 'utf8');
    if (!data.includes('YES')) {
      log.warn(`1-wire ${rom}: CRC NO, returning last value`);
      return nameForCache && lastTemp.has(nameForCache) ? (lastTemp.get(nameForCache) as number) : NaN;
    }
    const m = data.match(/t=(-?\d+)/);
    if (!m) {
      log.warn(`1-wire ${rom}: malformed payload`);
      return nameForCache && lastTemp.has(nameForCache) ? (lastTemp.get(nameForCache) as number) : NaN;
    }
    const tC = parseInt(m[1]!, 10) / 1000;
    if (nameForCache) {
      lastTemp.set(nameForCache, tC);
      lastReadAt.set(nameForCache, Date.now());
    }
    return tC;
  } catch (err) {
    log.warn(`1-wire ${rom}: ${(err as Error).message}`);
    return nameForCache && lastTemp.has(nameForCache) ? (lastTemp.get(nameForCache) as number) : NaN;
  }
}

export async function readCached(name: string, maxAgeMs = 800): Promise<number> {
  const age = Date.now() - (lastReadAt.get(name) ?? 0);
  if (age < maxAgeMs && lastTemp.has(name)) return lastTemp.get(name) as number;
  return readSensor(name);
}

export interface ScannedDevice {
  rom:        string;
  temp:       number | null;       // null = read failed
  assignedTo: string[];            // logical names currently assigned to this ROM
}

/** Walk the 1-Wire bus and return every DS18B20 with its current temperature. */
export async function scan(): Promise<ScannedDevice[]> {
  const seenRoms = new Set<string>();
  const result: ScannedDevice[] = [];

  if (MOCK) {
    for (const rom of new Set(Object.values(getAssignments()))) {
      seenRoms.add(rom);
      const temp = mockTemps.get(rom)
        ?? mockTemps.get(_nameForRom(rom) ?? '')
        ?? 20.0;
      result.push({ rom, temp, assignedTo: _namesForRom(rom) });
    }
    for (const rom of MOCK_EXTRA_DEVICES) {
      if (seenRoms.has(rom)) continue;
      seenRoms.add(rom);
      result.push({ rom, temp: mockTemps.get(rom) ?? 22.0, assignedTo: [] });
    }
    return result;
  }

  let entries: string[];
  try {
    entries = await fs.readdir(oneWire.busPath);
  } catch (err) {
    log.warn(`onewire.scan: cannot read ${oneWire.busPath}: ${(err as Error).message}`);
    return [];
  }
  const ds18b20s = entries.filter((e) => e.startsWith('28-'));
  for (const rom of ds18b20s) {
    const t = await readByRom(rom).catch(() => NaN);
    result.push({
      rom,
      temp: Number.isNaN(t) ? null : t,
      assignedTo: _namesForRom(rom),
    });
  }
  return result;
}

function _namesForRom(rom: string): string[] {
  const out: string[] = [];
  for (const [name, r] of Object.entries(getAssignments())) {
    if (r === rom) out.push(name);
  }
  return out;
}

function _nameForRom(rom: string): string | undefined {
  return _namesForRom(rom)[0];
}

export default {
  load,
  readSensor, readCached, readByRom,
  scan,
  getAssignments, resolveRom, setSensorRom,
  clearSensorOverride, clearAllOverrides,
  _mockSetTemp,
};
