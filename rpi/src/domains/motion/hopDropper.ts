// Hop dropper — port of hop_dropper.c.

import gpio from '../../platform/hal/gpio';
import log from '../../platform/util/logger';
import store from '../../platform/store/store';
import pinmap from '../../config/pinmap';
import params from '../../platform/parameters/parameters';
import { sleep, debounceInput } from '../../platform/hal/io_util';
import type { HopDropperState } from '../../types';

let busy = false;

export function init(): void {
  gpio.acquireOutput('HOP_DROPPER', pinmap.outputs.HOP_DROPPER.bcm, 0);
  gpio.acquireInput('HOP_DROPPER_LIMIT', pinmap.inputs.HOP_DROPPER_LIMIT.bcm, { pull: 'up' });
  store.set('hopDropper', 'stopped');
  log.info('Hop dropper initialised');
}

function drive(on: boolean): void { gpio.writeOutput('HOP_DROPPER', on ? 1 : 0); }

/** Drop a single hop addition. Resolves when the next cup arrives. */
export async function drop(): Promise<boolean> {
  if (busy) {
    log.warn('Hop dropper: already running — drop request ignored');
    return false;
  }
  busy = true;
  try {
    drive(true);
    store.set('hopDropper', 'driving_no_gap');
    // Wait for limit LOW (on the cup)
    // eslint-disable-next-line no-constant-condition
    while (true) {
      const v = await debounceInput('HOP_DROPPER_LIMIT');
      if (v === 0) break;
      await sleep(10);
    }
    store.set('hopDropper', 'driving_gap');
    // Wait for limit HIGH (in the gap)
    // eslint-disable-next-line no-constant-condition
    while (true) {
      const v = await debounceInput('HOP_DROPPER_LIMIT');
      if (v === 1) break;
      await sleep(10);
    }
    const stopDelay = params.get('uiHopDropperStopDelayms') ?? 110;
    await sleep(stopDelay);
    drive(false);
    store.set('hopDropper', 'stopped');
    log.info('Hop dropper: dropped one addition');
    return true;
  } finally {
    busy = false;
  }
}

export function startManual(): void {
  drive(true);
  store.set('hopDropper', 'driving_no_gap');
}

export function stopManual(): void {
  drive(false);
  store.set('hopDropper', 'stopped');
}

export function state(): HopDropperState { return store.state.hopDropper; }

export default { init, drop, startManual, stopManual, state };
