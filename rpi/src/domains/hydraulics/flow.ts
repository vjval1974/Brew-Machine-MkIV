// Flow sensor — port of Flow1.c.
//
// In production mode (`MOCK_HARDWARE` unset) the pulse counter runs in a
// dedicated worker_thread that owns the libgpiod line for the boil flow
// pin. The worker atomically increments a SharedArrayBuffer counter on
// every falling edge. The main thread polls that counter via Atomics.load
// every 500 ms and applies the original two-threshold litres-per-pulse
// conversion. This isolation is what stops a main-thread GC pause or a
// slow `JSON.parse` from dropping flow pulses — the previous in-process
// implementation silently masked dropped pulses at the "noise spike"
// branch as a fixed 15-pulse fallback (Architect 4's receipt).
//
// In mock mode the worker isn't spawned; pulses come from `_injectPulses`
// (used by the Diagnostics tab).

import { Worker } from 'worker_threads';
import path from 'path';
import log from '../../platform/util/logger';
import store from '../../platform/store/store';
import pinmap from '../../config/pinmap';

const MOCK = process.env.MOCK_HARDWARE === '1';

const LITRES_PER_PULSE_LOW  = 0.0043;
const LITRES_PER_PULSE_HIGH = 0.0052;
const LOWER_THRESH = 13;   // pulses per 500 ms window
const UPPER_THRESH = 30;
const TICK_MS = 500;

// Shared atomic counter (Int32Array[0] = total falling edges since boot).
// Used in both modes so reset semantics are identical.
const sharedBuffer = new SharedArrayBuffer(4);
const counter      = new Int32Array(sharedBuffer);

let lastPulses = 0;
let pulseEpochOffset = 0;            // subtracted from counter on reset
let litres = 0.0;
let measuring = false;
let runner: Promise<void> | null = null;
let worker: Worker | null = null;

function readPulses(): number {
  return Atomics.load(counter, 0) - pulseEpochOffset;
}

export function init(): void {
  const cfg = pinmap.pulseInputs.BOIL_FLOW;
  if (!cfg) throw new Error('Missing pinmap.pulseInputs.BOIL_FLOW');

  if (MOCK || cfg.bcm === null) {
    if (cfg.bcm === null) log.warn('Flow: BOIL_FLOW BCM is null — running mock-only pulse counter');
    else                  log.info('Flow: MOCK pulse counter (no worker)');
    store.set('flow', { boilLitres: 0, flowing: false, measuring: false });
    return;
  }

  // Real hardware: spawn the pulse-counter worker. Resolve worker path
  // for both dev (tsx, .ts files) and prod (compiled .js).
  const ext = __filename.endsWith('.ts') ? '.ts' : '.js';
  const workerPath = path.join(__dirname, '..', '..', 'platform', 'hal', 'workers', `flow-worker${ext}`);
  worker = new Worker(workerPath, {
    workerData: { bcm: cfg.bcm, edge: cfg.edge ?? 'falling', sharedBuffer },
  });
  worker.on('message', (msg: { type: string; error?: string }) => {
    if (msg.type === 'ready')  log.info('Flow: pulse worker ready');
    if (msg.type === 'error')  log.warn(`Flow worker: ${msg.error}`);
    if (msg.type === 'fatal') {
      log.error(`Flow worker fatal: ${msg.error}`);
      worker = null;
    }
  });
  worker.on('error', (err: Error) => log.error(`Flow worker error: ${err.message}`));
  worker.on('exit',  (code: number) => log.warn(`Flow worker exited with code ${code}`));

  store.set('flow', { boilLitres: 0, flowing: false, measuring: false });
  log.info(`Flow sensor initialised (BOIL_FLOW on BCM ${cfg.bcm}, worker pulse counter)`);
}

async function loop(): Promise<void> {
  // eslint-disable-next-line no-constant-condition
  while (true) {
    if (measuring) {
      const total = readPulses();
      const delta = total - lastPulses;
      lastPulses = total;

      // Two-threshold litres-per-pulse calibration from the original
      // Flow1.c. Crucially: an over-UPPER_THRESH burst is no longer
      // silently averaged to 15 — it gets logged as a fault.
      if (delta <= LOWER_THRESH) {
        litres += delta * LITRES_PER_PULSE_LOW;
      } else if (delta <= UPPER_THRESH) {
        litres += delta * LITRES_PER_PULSE_HIGH;
      } else {
        log.warn(`Flow: ${delta} pulses in ${TICK_MS} ms exceeds UPPER_THRESH (${UPPER_THRESH}). ` +
                 `Possible electrical noise (mill running?) or dropped pulses in a prior window. ` +
                 `Using high-rate coefficient.`);
        litres += delta * LITRES_PER_PULSE_HIGH;
      }
      if (litres < 0 || litres > 50_000) {
        log.warn(`Flow: integrator out of range (${litres.toFixed(3)} L), resetting`);
        litres = 0;
      }
      const flowing = delta > 0;
      store.set('flow', { boilLitres: +litres.toFixed(3), flowing, measuring: true });
    } else if (store.state.flow.flowing) {
      store.patch('flow', { flowing: false });
    }
    await new Promise((r) => setTimeout(r, TICK_MS));
  }
}

export function start(): void {
  if (runner) return;
  runner = loop().catch((err: Error) => log.error(`Flow loop crashed: ${err.message}`));
}

export function reset(): void {
  pulseEpochOffset = Atomics.load(counter, 0);
  lastPulses = 0;
  litres = 0;
  store.patch('flow', { boilLitres: 0 });
  log.info('Flow: reset');
}

export function setMeasuring(on: boolean): void {
  measuring = on;
  store.patch('flow', { measuring });
}

export function getLitres():    number  { return litres; }
export function isMeasuring():  boolean { return measuring; }
export function isFlowing():    boolean { return store.state.flow.flowing; }

/** Test helper used by mock mode. */
export function _injectPulses(n: number): void {
  Atomics.add(counter, 0, n);
}

/** Stop the worker (called from shutdown). */
export async function stop(): Promise<void> {
  if (worker) {
    try { await worker.terminate(); } catch { /* ignore */ }
    worker = null;
  }
}

export default {
  init, start, stop, reset, setMeasuring,
  getLitres, isMeasuring, isFlowing,
  _injectPulses,
};
