// Mash pump — port of mash_pump.c.

import gpio from '../hal/gpio';
import log from '../util/logger';
import store from '../state/store';
import pinmap from '../config/pinmap';
import * as crane from './crane';
import * as valves from './valves';
import type { PumpState } from '../types';

export function init(): void {
  const cfg = pinmap.outputs.MASH_PUMP;
  if (!cfg) throw new Error('Missing pinmap.outputs.MASH_PUMP');
  gpio.acquireOutput('MASH_PUMP', cfg.bcm, 0);
  store.patch('pumps', { mash: 'stopped' });
  log.info('Mash pump initialised');
}

function okToPump(): boolean {
  const brewRunning = store.state.brew.running === 'running';
  if (!brewRunning) return true;
  const craneAtBottom = crane.state() === 'at_bottom';
  const mashValveOpen = valves.state('MASH') === 'open';
  return craneAtBottom || mashValveOpen;
}

export function start(): boolean {
  if (state() === 'pumping') return true;
  if (!okToPump()) {
    log.warn('Mash pump: not safe to start (crane up & mash valve closed)');
    return false;
  }
  gpio.writeOutput('MASH_PUMP', 1);
  store.patch('pumps', { mash: 'pumping' });
  log.info('Mash pump: START');
  return true;
}

export function stop(): void {
  if (state() === 'stopped') return;
  gpio.writeOutput('MASH_PUMP', 0);
  store.patch('pumps', { mash: 'stopped' });
  log.info('Mash pump: STOP');
}

export function toggle(): void {
  if (state() === 'pumping') stop(); else start();
}

export function state(): PumpState {
  return store.state.pumps.mash;
}

export { okToPump };

export default { init, start, stop, toggle, state, okToPump };
