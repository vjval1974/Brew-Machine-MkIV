// Boil valve — port of boil_valve.c. Motorised, H-bridge with two limit
// switches. Open / Close commands return a Promise that resolves when the
// limit is hit.

import gpio from '../hal/gpio';
import log from '../util/logger';
import store from '../state/store';
import pinmap from '../config/pinmap';
import { sleep, debounceInput } from '../hal/io_util';
import type { BoilValveState } from '../types';

type Cmd = 'open' | 'close' | 'stop';
let cmd: Cmd = 'stop';
let runner: Promise<void> | null = null;

interface CmdResolver {
  target: 'opened' | 'closed' | 'stopped';
  resolve: () => void;
}
const pending: CmdResolver[] = [];

function set(s: BoilValveState): void {
  store.set('boilValve', s);
  log.info(`BoilValve state -> ${s}`);
  for (let i = pending.length - 1; i >= 0; i--) {
    if (pending[i]!.target === s) {
      pending[i]!.resolve();
      pending.splice(i, 1);
    }
  }
}

export function init(): void {
  gpio.acquireOutput('BOIL_VALVE_OPEN',  pinmap.outputs.BOIL_VALVE_OPEN.bcm,  0);
  gpio.acquireOutput('BOIL_VALVE_CLOSE', pinmap.outputs.BOIL_VALVE_CLOSE.bcm, 0);
  gpio.acquireInput('BOIL_VALVE_OPENED', pinmap.inputs.BOIL_VALVE_OPENED.bcm, { pull: 'up' });
  gpio.acquireInput('BOIL_VALVE_CLOSED', pinmap.inputs.BOIL_VALVE_CLOSED.bcm, { pull: 'up' });
  set('stopped');
  log.info('Boil valve initialised');
}

function drive(direction: Cmd): void {
  if (direction === 'open') {
    gpio.writeOutput('BOIL_VALVE_CLOSE', 0);
    gpio.writeOutput('BOIL_VALVE_OPEN',  1);
  } else if (direction === 'close') {
    gpio.writeOutput('BOIL_VALVE_OPEN',  0);
    gpio.writeOutput('BOIL_VALVE_CLOSE', 1);
  } else {
    gpio.writeOutput('BOIL_VALVE_OPEN',  0);
    gpio.writeOutput('BOIL_VALVE_CLOSE', 0);
  }
}

async function openedHit(): Promise<boolean> { return (await debounceInput('BOIL_VALVE_OPENED')) === 0; }
async function closedHit(): Promise<boolean> { return (await debounceInput('BOIL_VALVE_CLOSED')) === 0; }

async function loop(): Promise<void> {
  // eslint-disable-next-line no-constant-condition
  for (;;) {
    if (cmd === 'stop') {
      drive('stop');
      if (state() !== 'opened' && state() !== 'closed') set('stopped');
      await sleep(80);
      continue;
    }
    if (cmd === 'open') {
      if (await openedHit()) { drive('stop'); set('opened'); cmd = 'stop'; continue; }
      drive('open'); set('opening'); await sleep(80); continue;
    }
    if (cmd === 'close') {
      if (await closedHit()) { drive('stop'); set('closed'); cmd = 'stop'; continue; }
      drive('close'); set('closing'); await sleep(80); continue;
    }
  }
}

export function open(): Promise<void> {
  cmd = 'open';
  if (state() === 'opened') return Promise.resolve();
  return new Promise((resolve) => { pending.push({ target: 'opened', resolve }); });
}

export function close(): Promise<void> {
  cmd = 'close';
  if (state() === 'closed') return Promise.resolve();
  return new Promise((resolve) => { pending.push({ target: 'closed', resolve }); });
}

export function stop(): void { cmd = 'stop'; }

export function toggle(): Promise<void> {
  if (state() === 'opened') return close();
  return open();
}

export function state(): BoilValveState { return store.state.boilValve; }

export function start(): void {
  if (runner) return;
  runner = loop().catch((err: Error) => log.error(`BoilValve loop crashed: ${err.message}`));
}

export default { init, start, open, close, stop, toggle, state };
