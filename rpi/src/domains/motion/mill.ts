// Grain mill motor — port of mill.c.

import gpio from '../../platform/hal/gpio';
import log from '../../platform/util/logger';
import store from '../../platform/store/store';
import pinmap from '../../config/pinmap';
import { sleep } from '../../platform/hal/io_util';
import type { MillState } from '../../types';

export function init(): void {
  gpio.acquireOutput('MILL', pinmap.outputs.MILL.bcm, 0);
  store.set('mill', 'stopped');
  log.info('Mill initialised');
}

export function start(): void {
  gpio.writeOutput('MILL', 1);
  store.set('mill', 'driving');
  log.info('Mill: START');
}

export function stop(): void {
  gpio.writeOutput('MILL', 0);
  store.set('mill', 'stopped');
  log.info('Mill: STOP');
}

export function state(): MillState { return store.state.mill; }

/** Run the mill for `ms` milliseconds, then stop. Used by the brew engine. */
export async function runFor(ms: number, abort?: () => boolean): Promise<void> {
  start();
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    if (abort?.()) break;
    await sleep(Math.min(200, ms - (Date.now() - t0)));
  }
  stop();
}

export default { init, start, stop, state, runFor };
