// Flow pulse counter worker_thread.
//
// Owns the libgpiod line for the boil flow sensor and sits in a tight
// `eventWait` loop, atomically incrementing a SharedArrayBuffer counter
// for each falling edge. The main thread reads the counter via
// `Atomics.load` and computes deltas. Isolating this loop from the main
// Node event loop is what stops a GC pause or a slow `JSON.parse` from
// dropping flow pulses (which the old in-process implementation silently
// masked at flow.ts:44-46 as a "noise spike").

import { parentPort, workerData } from 'worker_threads';

interface WorkerData {
  bcm: number;
  edge: 'rising' | 'falling' | 'both';
  sharedBuffer: SharedArrayBuffer;
}

const { bcm, edge, sharedBuffer } = workerData as WorkerData;
const counter = new Int32Array(sharedBuffer);

interface Line {
  requestEventMode: (consumer: string, flag: number) => void;
  eventWait:        (timeoutNs: bigint) => Promise<unknown>;
  eventRead:        () => unknown;
}
interface Libgpiod {
  Chip: new (path: string) => unknown;
  Line: new (chip: unknown, bcm: number) => Line;
  LINE_REQ_EV_RISING_EDGE:  number;
  LINE_REQ_EV_FALLING_EDGE: number;
  LINE_REQ_EV_BOTH_EDGES:   number;
}

let libgpiod: Libgpiod;
try {
  // eslint-disable-next-line @typescript-eslint/no-require-imports
  libgpiod = require('node-libgpiod') as Libgpiod;
} catch (err) {
  parentPort?.postMessage({ type: 'fatal', error: `node-libgpiod not available in worker: ${(err as Error).message}` });
  process.exit(1);
}

// Pi 5 uses /dev/gpiochip4 (RP1 bank 0). Pi 4 and earlier use gpiochip0.
// Try the Pi-5 path first.
let chip: unknown;
try { chip = new libgpiod.Chip('/dev/gpiochip4'); }
catch { chip = new libgpiod.Chip('/dev/gpiochip0'); }

const flag =
  edge === 'rising' ? libgpiod.LINE_REQ_EV_RISING_EDGE :
  edge === 'both'   ? libgpiod.LINE_REQ_EV_BOTH_EDGES  :
                      libgpiod.LINE_REQ_EV_FALLING_EDGE;

const line = new libgpiod.Line(chip, bcm);
line.requestEventMode('brew-flow-worker', flag);

parentPort?.postMessage({ type: 'ready' });

// Hot loop. eventWait with a short timeout so we periodically yield and
// the worker stays responsive to messages. Each event drains and bumps
// the atomic counter by one — libgpiod doesn't expose a multi-event
// drain in node-libgpiod, but the worker has no other work to compete
// for its event loop, so pulse rates up to a few kHz are realistic.
async function pulseLoop(): Promise<void> {
  // eslint-disable-next-line no-constant-condition
  while (true) {
    try {
      const ev = await line.eventWait(BigInt(200_000_000)); // 200 ms timeout
      if (ev) {
        line.eventRead();
        Atomics.add(counter, 0, 1);
      }
    } catch (err) {
      parentPort?.postMessage({ type: 'error', error: (err as Error).message });
      // back off briefly, don't spin
      await new Promise((r) => setTimeout(r, 100));
    }
  }
}

void pulseLoop();
