// I2C HAL — PCF8574 8-bit IO expander helpers.
//
// Original C (`I2C-IO.c`) used STM32 I2C1 to talk to up to eight PCF8574s
// (addresses 0x70..0x7E, 8-bit). On Linux we use /dev/i2c-1 via the
// `i2c-bus` package, with 7-bit addresses (see src/config/i2c.ts).
//
// PCF8574 quirk: to drive a pin LOW we write 0 to its bit; to drive HIGH
// (or read it) we write 1. We maintain a shadow byte per address so
// set/clear operations are read-modify-write safe under concurrent callers.

import log from '../util/logger';
import { i2c as i2cCfg } from '../../config/pinmap';

const MOCK = process.env.MOCK_HARDWARE === '1';

interface I2cBusModule {
  openPromisified: (busNumber: number) => Promise<I2cBusHandle>;
}
interface I2cBusHandle {
  i2cWrite: (addr: number, length: number, buffer: Buffer) => Promise<unknown>;
  i2cRead: (addr: number, length: number, buffer: Buffer) => Promise<unknown>;
  close: () => Promise<void>;
}

let i2cBus: I2cBusModule | null = null;
let realBus: I2cBusHandle | null = null;

if (!MOCK) {
  try {
    // eslint-disable-next-line @typescript-eslint/no-require-imports
    i2cBus = require('i2c-bus') as I2cBusModule;
  } catch (err) {
    log.warn(`i2c-bus not available (${(err as Error).message}). Falling back to mock I2C.`);
  }
}

// Shadow state per 7-bit address. Default 0xff (all bits high = inputs).
const shadow = new Map<number, number>();
function getShadow(addr: number): number {
  if (!shadow.has(addr)) shadow.set(addr, 0xff);
  return shadow.get(addr) as number;
}
function setShadow(addr: number, v: number): void {
  shadow.set(addr, v & 0xff);
}

export async function open(): Promise<void> {
  if (MOCK || !i2cBus) {
    log.info('I2C backend: MOCK');
    return;
  }
  realBus = await i2cBus.openPromisified(i2cCfg.busNumber);
  log.info(`I2C backend: real on /dev/i2c-${i2cCfg.busNumber}`);
}

export async function writeByte(addr: number, byte: number): Promise<void> {
  if (MOCK || !realBus) { setShadow(addr, byte); return; }
  await realBus.i2cWrite(addr, 1, Buffer.from([byte & 0xff]));
  setShadow(addr, byte);
}

export async function readByte(addr: number): Promise<number> {
  if (MOCK || !realBus) return getShadow(addr);
  const buf = Buffer.alloc(1);
  await realBus.i2cRead(addr, 1, buf);
  return buf[0]!;
}

/** Drive a single PCF8574 bit LOW. Original: vPCF_SetBits. */
export async function setBitLow(addr: number, bit: number): Promise<void> {
  const cur = await readByte(addr);
  await writeByte(addr, cur & ~(1 << bit));
}

/** Release a single PCF8574 bit HIGH. Original: vPCF_ResetBits. */
export async function setBitHigh(addr: number, bit: number): Promise<void> {
  const cur = await readByte(addr);
  await writeByte(addr, cur | (1 << bit));
}

export async function readBit(addr: number, bit: number): Promise<boolean> {
  await writeByte(addr, 0xff);            // ensure inputs are released
  const v = await readByte(addr);
  return Boolean(v & (1 << bit));
}

export async function isActiveLow(addr: number, bit: number): Promise<boolean> {
  return !(await readBit(addr, bit));
}

export async function close(): Promise<void> {
  if (realBus) await realBus.close();
}

export default {
  open, close,
  writeByte, readByte,
  setBitHigh, setBitLow,
  readBit, isActiveLow,
};
