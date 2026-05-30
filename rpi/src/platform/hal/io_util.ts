// Debounce helpers — port of io_util.c.

import gpio from './gpio';
import i2c from './i2c';

export const sleep = (ms: number): Promise<void> => new Promise((r) => setTimeout(r, ms));

export interface DebounceOpts {
  samples?: number;
  gapMs?: number;
  want?: number | null;
}

export async function debounceInput(
  name: string,
  { samples = 4, gapMs = 8, want = null }: DebounceOpts = {}
): Promise<number | null> {
  const readings: number[] = [];
  for (let i = 0; i < samples; i++) {
    readings.push(gpio.readInput(name));
    if (i < samples - 1) await sleep(gapMs);
  }
  const stable = readings.every((v) => v === readings[0]);
  if (!stable) return null;
  const first = readings[0]!;
  if (want !== null) return first === want ? 1 : 0;
  return first;
}

export async function debounceExpanderBit(
  addr: number,
  bit: number,
  { samples = 4, gapMs = 8 }: DebounceOpts = {}
): Promise<number | null> {
  const readings: number[] = [];
  for (let i = 0; i < samples; i++) {
    readings.push((await i2c.readBit(addr, bit)) ? 1 : 0);
    if (i < samples - 1) await sleep(gapMs);
  }
  const stable = readings.every((v) => v === readings[0]);
  return stable ? readings[0]! : null;
}
