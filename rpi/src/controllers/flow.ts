// Flow sensor — port of Flow1.c. Pulse counter with low/high rate thresholds.

import gpio from '../hal/gpio';
import log from '../util/logger';
import store from '../state/store';
import pinmap from '../config/pinmap';

const LITRES_PER_PULSE_LOW  = 0.0043;
const LITRES_PER_PULSE_HIGH = 0.0052;
const LOWER_THRESH = 13;   // pulses per 500 ms window
const UPPER_THRESH = 30;
const TICK_MS = 500;

let pulses = 0;
let lastPulses = 0;
let litres = 0.0;
let measuring = false;
let runner: Promise<void> | null = null;

function onPulse(): void { pulses++; }

export function init(): void {
  const cfg = pinmap.pulseInputs.BOIL_FLOW;
  if (!cfg) throw new Error('Missing pinmap.pulseInputs.BOIL_FLOW');
  if (cfg.bcm === null) {
    log.warn('Flow: BOIL_FLOW BCM is null — pulse counting disabled');
    return;
  }
  gpio.acquirePulseInput('BOIL_FLOW', cfg.bcm, { edge: cfg.edge, onPulse });
  store.set('flow', { boilLitres: 0, flowing: false, measuring: false });
  log.info('Flow sensor initialised (BOIL_FLOW)');
}

async function loop(): Promise<void> {
  // eslint-disable-next-line no-constant-condition
  while (true) {
    if (measuring) {
      const delta = pulses - lastPulses;
      lastPulses = pulses;
      if (delta <= LOWER_THRESH) {
        litres += delta * LITRES_PER_PULSE_LOW;
      } else if (delta <= UPPER_THRESH) {
        litres += delta * LITRES_PER_PULSE_HIGH;
      } else {
        litres += 15 * LITRES_PER_PULSE_HIGH;     // noise spike average
      }
      if (litres < 0.01 || litres > 50_000) litres = 0;
      const flowing = delta > 0 && delta <= UPPER_THRESH;
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
  pulses = 0;
  lastPulses = 0;
  litres = 0;
  store.patch('flow', { boilLitres: 0 });
  log.info('Flow: reset');
}

export function setMeasuring(on: boolean): void {
  measuring = on;
  store.patch('flow', { measuring });
}

export function getLitres(): number   { return litres; }
export function isMeasuring(): boolean { return measuring; }
export function isFlowing(): boolean   { return store.state.flow.flowing; }

/** Test helper used by mock mode. */
export function _injectPulses(n: number): void { pulses += n; }

export default {
  init, start, reset, setMeasuring,
  getLitres, isMeasuring, isFlowing,
  _injectPulses,
};
