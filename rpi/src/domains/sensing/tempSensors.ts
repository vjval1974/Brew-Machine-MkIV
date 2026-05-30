// Temperature poller — reads HLT + MASH DS18B20s every second.
//
// Maintains a rolling buffer of recent samples per sensor so controllers
// (notably the HLT loop) can do their three-sample stability check without
// blocking the event loop for 1.8 s the way the original `stableHltTemp`
// did. The poller itself uses async `fs.readFile` so the kernel w1-therm
// 750 ms blocking read runs on the libuv thread pool, never on the main
// thread.

import log from '../../platform/util/logger';
import onewire from '../../platform/hal/onewire';
import store from '../../platform/store/store';
import type { AppState } from '../../types';

type SensorName = 'HLT' | 'MASH';

interface Sample { t: number; v: number }

const POLL_MS = 1000;
const MAX_SAMPLES = 16;
const STALE_AFTER_MS = 5_000;        // a sample older than this is unusable

const buffers = new Map<SensorName, Sample[]>();
const SENSORS: SensorName[] = ['HLT', 'MASH'];

let runner: Promise<void> | null = null;

function record(name: SensorName, v: number): void {
  if (Number.isNaN(v)) return;       // drop bad reads; buffer keeps last good
  const buf = buffers.get(name) ?? [];
  buf.push({ t: Date.now(), v });
  while (buf.length > MAX_SAMPLES) buf.shift();
  buffers.set(name, buf);
}

async function loop(): Promise<void> {
  // eslint-disable-next-line no-constant-condition
  while (true) {
    for (const name of SENSORS) {
      try {
        const v = await onewire.readSensor(name);
        record(name, v);
      } catch (err) {
        log.warn(`Temp poll ${name}: ${(err as Error).message}`);
      }
    }
    const snapshot: AppState['temps'] = {
      HLT:  getLastValue('HLT')  ?? NaN,
      MASH: getLastValue('MASH') ?? NaN,
    };
    store.set('temps', snapshot);
    await new Promise((r) => setTimeout(r, POLL_MS));
  }
}

export function start(): void {
  if (runner) return;
  runner = loop().catch((err: Error) => log.error(`Temp loop crashed: ${err.message}`));
}

/** Most recent sample value, or `undefined` if the poller hasn't seen one. */
function getLastValue(name: SensorName): number | undefined {
  const buf = buffers.get(name);
  if (!buf || buf.length === 0) return undefined;
  return buf[buf.length - 1]!.v;
}

export function get(name: SensorName): number {
  return getLastValue(name) ?? NaN;
}

/**
 * Returns the timestamp (ms epoch) of the latest reading for `name`, or 0 if
 * none yet. Use to detect a stuck poller.
 */
export function getLastReadAt(name: SensorName): number {
  const buf = buffers.get(name);
  if (!buf || buf.length === 0) return 0;
  return buf[buf.length - 1]!.t;
}

/**
 * Three-sample stability check: the latest 3 samples must all be within
 * `tolerance` of each other AND younger than `STALE_AFTER_MS`. Returns the
 * mean of the three samples on success, or `null` on failure (insufficient
 * samples, stale data, or samples disagree). The HLT loop uses null as a
 * sensor-fault signal and drives the heater off.
 *
 * This is the synchronous replacement for the original `stableHltTemp` that
 * blocked the HLT control loop for 1.8 s per call (three reads × 900 ms
 * sleep between them).
 */
export function getStableTemp(name: SensorName, tolerance = 2.0): number | null {
  const buf = buffers.get(name);
  if (!buf || buf.length < 3) return null;
  const recent = buf.slice(-3);
  const now = Date.now();
  if (now - recent[0]!.t > STALE_AFTER_MS) return null;
  const [a, b, c] = recent.map((s) => s.v);
  if (a === undefined || b === undefined || c === undefined) return null;
  if (Math.abs(a - b) >= tolerance) return null;
  if (Math.abs(b - c) >= tolerance) return null;
  return (a + b + c) / 3;
}

export default { start, get, getLastReadAt, getStableTemp };
