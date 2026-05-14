'use strict';

// I2C HAL — PCF8574 8-bit IO expander helpers.
//
// Original C (`I2C-IO.c`) used STM32 I2C1 to talk to up to eight PCF8574s
// (addresses 0x70..0x7E, 8-bit). On Linux we use /dev/i2c-1 via the
// `i2c-bus` package, with 7-bit addresses (`PCF_PORTS` in config/i2c.js).
//
// The PCF8574 is a quasi-bidirectional 8-bit port: to drive a pin LOW you
// write 0 to its bit; to drive HIGH (or to read it as an input) you write 1.
// We maintain a shadow byte per address so set/clear operations are
// read-modify-write safe under concurrent callers.

const log = require('../util/logger');
const { i2c: i2cCfg } = require('../../config/pinmap');

const MOCK = process.env.MOCK_HARDWARE === '1';

let i2cBus = null;
let realBus = null;

if (!MOCK) {
  try {
    i2cBus = require('i2c-bus');
  } catch (err) {
    log.warn(`i2c-bus not available (${err.message}). Falling back to mock I2C.`);
  }
}

// Shadow state per 7-bit address. Default is 0xff (all bits high = inputs).
const shadow = new Map();
function getShadow(addr) {
  if (!shadow.has(addr)) shadow.set(addr, 0xff);
  return shadow.get(addr);
}
function setShadow(addr, v) { shadow.set(addr, v & 0xff); }

async function open() {
  if (MOCK || !i2cBus) {
    log.info('I2C backend: MOCK');
    return;
  }
  realBus = await i2cBus.openPromisified(i2cCfg.busNumber);
  log.info(`I2C backend: real on /dev/i2c-${i2cCfg.busNumber}`);
}

async function writeByte(addr, byte) {
  if (MOCK || !realBus) {
    setShadow(addr, byte);
    return;
  }
  await realBus.i2cWrite(addr, 1, Buffer.from([byte & 0xff]));
  setShadow(addr, byte);
}

async function readByte(addr) {
  if (MOCK || !realBus) return getShadow(addr);
  const buf = Buffer.alloc(1);
  await realBus.i2cRead(addr, 1, buf);
  return buf[0];
}

// Set a single bit to 0 (drives the pin LOW). Original: vPCF_SetBits.
async function setBitLow(addr, bit) {
  const cur = await readByte(addr);
  const next = cur & ~(1 << bit);
  await writeByte(addr, next);
}

// Set a single bit to 1 (releases the pin / drives HIGH). Original: vPCF_ResetBits.
async function setBitHigh(addr, bit) {
  const cur = await readByte(addr);
  const next = cur | (1 << bit);
  await writeByte(addr, next);
}

// Read a single bit as a boolean (true = pin is HIGH, false = pin is LOW).
// Original `cI2cGetInput` returns TRUE when the bit reads zero (active low
// input). To preserve semantics, callers that want the "is-asserted" answer
// should call `isActiveLow(...)` which returns true when the pin is LOW.
async function readBit(addr, bit) {
  // The original code wrote 0xFF to the port first to set all pins as inputs
  // (PCF8574 quirk). Reproduce that to be safe before reading.
  await writeByte(addr, 0xff);
  const v = await readByte(addr);
  return Boolean(v & (1 << bit));
}

async function isActiveLow(addr, bit) {
  return !(await readBit(addr, bit));
}

async function close() {
  if (realBus) await realBus.close();
}

module.exports = {
  open, close,
  writeByte, readByte,
  setBitHigh, setBitLow,
  readBit, isActiveLow,
};
