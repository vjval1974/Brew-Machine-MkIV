// Stir motor — port of stir.c. Interlocked: only runs during a brew when
// the crane is at the bottom or driving down in increments.

import gpio from '../hal/gpio';
import log from '../util/logger';
import store from '../state/store';
import pinmap from '../config/pinmap';
import * as crane from './crane';
import type { StirState } from '../types';

export function init(): void {
  gpio.acquireOutput('STIR', pinmap.outputs.STIR.bcm, 0);
  store.set('stir', 'stopped');
  log.info('Stir motor initialised');
}

function okToStir(): boolean {
  const brewRunning = store.state.brew.running === 'running';
  if (!brewRunning) return true;
  const cs = crane.state();
  return cs === 'at_bottom' || cs === 'driving_down_incremental';
}

export function start(): void {
  if (state() === 'driving') return;
  if (!okToStir()) {
    log.warn('Stir: NOT OK to stir (crane not at bottom)');
    return;
  }
  gpio.writeOutput('STIR', 1);
  store.set('stir', 'driving');
  log.info('Stir: START');
}

export function stop(): void {
  if (state() === 'stopped') return;
  gpio.writeOutput('STIR', 0);
  store.set('stir', 'stopped');
  log.info('Stir: STOP');
}

export function state(): StirState { return store.state.stir; }

export default { init, start, stop, state };
