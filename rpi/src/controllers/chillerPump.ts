// Chiller pump (also called "boil pump" in the original UI) — port of
// chiller_pump.c. Straight on/off, no interlocks.

import gpio from '../hal/gpio';
import log from '../util/logger';
import store from '../state/store';
import pinmap from '../config/pinmap';
import type { PumpState } from '../types';

export function init(): void {
  const cfg = pinmap.outputs.CHILLER_PUMP;
  if (!cfg) throw new Error('Missing pinmap.outputs.CHILLER_PUMP');
  gpio.acquireOutput('CHILLER_PUMP', cfg.bcm, 0);
  store.patch('pumps', { chiller: 'stopped' });
  log.info('Chiller pump initialised');
}

export function start(): void {
  if (state() === 'pumping') return;
  gpio.writeOutput('CHILLER_PUMP', 1);
  store.patch('pumps', { chiller: 'pumping' });
  log.info('Chiller pump: START');
}

export function stop(): void {
  if (state() === 'stopped') return;
  gpio.writeOutput('CHILLER_PUMP', 0);
  store.patch('pumps', { chiller: 'stopped' });
  log.info('Chiller pump: STOP');
}

export function toggle(): void {
  if (state() === 'pumping') stop(); else start();
}

export function state(): PumpState {
  return store.state.pumps.chiller;
}

export default { init, start, stop, toggle, state };
