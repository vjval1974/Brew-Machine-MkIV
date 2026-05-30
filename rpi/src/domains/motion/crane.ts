// Crane controller — port of crane.c.
//
// Three drive commands (Up / Down / Down-incremental) plus Stop. Each command
// returns a Promise that resolves when the corresponding limit switch is
// reached (or when stopped). The background loop runs once per tick and is
// the single owner of the H-bridge outputs.

import gpio from '../../platform/hal/gpio';
import log from '../../platform/util/logger';
import store from '../../platform/store/store';
import pinmap from '../../config/pinmap';
import { sleep, debounceInput } from '../../platform/hal/io_util';
import type { CraneState } from '../../types';

type Cmd = 'up' | 'down' | 'incremental' | 'stop';

let cmd: Cmd = 'stop';
let runner: Promise<void> | null = null;

interface CmdResolver {
  target: CraneState;
  resolve: () => void;
  reject:  (err: Error) => void;
}
const pending: CmdResolver[] = [];

function set(s: CraneState): void {
  store.set('crane', s);
  log.info(`Crane state -> ${s}`);
  for (let i = pending.length - 1; i >= 0; i--) {
    if (pending[i]!.target === s) {
      pending[i]!.resolve();
      pending.splice(i, 1);
    }
  }
}

export function init(): void {
  gpio.acquireOutput('CRANE_UP',   pinmap.outputs.CRANE_UP.bcm,   0);
  gpio.acquireOutput('CRANE_DOWN', pinmap.outputs.CRANE_DOWN.bcm, 0);
  gpio.acquireInput('CRANE_UPPER_LIMIT', pinmap.inputs.CRANE_UPPER_LIMIT.bcm, { pull: 'up' });
  gpio.acquireInput('CRANE_LOWER_LIMIT', pinmap.inputs.CRANE_LOWER_LIMIT.bcm, { pull: 'up' });
  set('stopped');
  log.info('Crane initialised');
}

function drive(dir: 'up' | 'down' | 'stop'): void {
  if (dir === 'up') {
    gpio.writeOutput('CRANE_DOWN', 0);
    gpio.writeOutput('CRANE_UP',   1);
  } else if (dir === 'down') {
    gpio.writeOutput('CRANE_UP',   0);
    gpio.writeOutput('CRANE_DOWN', 1);
  } else {
    gpio.writeOutput('CRANE_UP',   0);
    gpio.writeOutput('CRANE_DOWN', 0);
  }
}

async function upperHit(): Promise<boolean> { return (await debounceInput('CRANE_UPPER_LIMIT')) === 0; }
async function lowerHit(): Promise<boolean> { return (await debounceInput('CRANE_LOWER_LIMIT')) === 0; }

async function loop(): Promise<void> {
  // Lazy import to avoid circular dep with stir.ts
  // eslint-disable-next-line @typescript-eslint/no-require-imports
  const stir = require('./stir') as typeof import('./stir');

  // eslint-disable-next-line no-constant-condition
  for (;;) {
    if (cmd === 'stop') {
      drive('stop');
      if (state() === 'driving_up' || state() === 'driving_down' || state() === 'driving_down_incremental') {
        set('stopped');
      }
      await sleep(80);
      continue;
    }

    if (cmd === 'up') {
      if (await upperHit()) { drive('stop'); set('at_top'); cmd = 'stop'; continue; }
      drive('up'); set('driving_up'); await sleep(80); continue;
    }

    if (cmd === 'down') {
      if (await lowerHit()) { drive('stop'); set('at_bottom'); cmd = 'stop'; continue; }
      drive('down'); set('driving_down'); await sleep(80); continue;
    }

    if (cmd === 'incremental') {
      set('driving_down_incremental');
      let increments = 0;
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      while ((cmd as any) === 'incremental') {
        if (await lowerHit()) {
          drive('down'); await sleep(100); drive('stop');
          set('at_bottom');
          stir.stop();
          cmd = 'stop';
          break;
        }
        drive('down'); await sleep(200);
        drive('stop'); await sleep(1500);
        increments++;
        if (increments === 11) stir.start();
      }
      continue;
    }
  }
}

// ── Public command API (awaitable) ──────────────────────────────────────────
function commandToTarget(c: Cmd, target: CraneState): Promise<void> {
  cmd = c;
  if (state() === target) return Promise.resolve();
  return new Promise((resolve, reject) => {
    pending.push({ target, resolve, reject });
  });
}

export function up():          Promise<void> { return commandToTarget('up',          'at_top'); }
export function down():        Promise<void> { return commandToTarget('down',        'at_bottom'); }
export function incremental(): Promise<void> { return commandToTarget('incremental', 'at_bottom'); }
export function stop(): void { cmd = 'stop'; }

export function state(): CraneState { return store.state.crane; }

export function start(): void {
  if (runner) return;
  runner = loop().catch((err: Error) => log.error(`Crane loop crashed: ${err.message}`));
}

export default { init, start, up, down, incremental, stop, state };
