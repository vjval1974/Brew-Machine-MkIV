'use strict';

// Debounce helpers — port of io_util.c.
//
// Original `debounce(port, pin)` polled the pin a few times with small
// delays and reported a stable state. We replicate that for both native
// GPIO inputs and PCF8574 bit reads.

const gpio = require('./gpio');
const i2c  = require('./i2c');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function debounceInput(name, { samples = 4, gapMs = 8, want = null } = {}) {
  let lastReadings = [];
  for (let i = 0; i < samples; i++) {
    lastReadings.push(gpio.readInput(name));
    if (i < samples - 1) await sleep(gapMs);
  }
  const stable = lastReadings.every((v) => v === lastReadings[0]);
  if (!stable) return null;
  if (want !== null) return lastReadings[0] === want;
  return lastReadings[0];
}

async function debounceExpanderBit(addr, bit, { samples = 4, gapMs = 8 } = {}) {
  let lastReadings = [];
  for (let i = 0; i < samples; i++) {
    lastReadings.push(await i2c.readBit(addr, bit) ? 1 : 0);
    if (i < samples - 1) await sleep(gapMs);
  }
  const stable = lastReadings.every((v) => v === lastReadings[0]);
  return stable ? lastReadings[0] : null;
}

module.exports = { sleep, debounceInput, debounceExpanderBit };
