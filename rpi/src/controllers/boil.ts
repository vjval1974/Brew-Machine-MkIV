// Boil kettle controller — port of boil.c.

import log from '../util/logger';
import store from '../state/store';
import pinmap from '../config/pinmap';
import gpio from '../hal/gpio';
import { createPwm, type Pwm } from '../hal/pwm';
import type { BoilState } from '../types';

let pwm: Pwm | null = null;
let duty = 0;
let st: BoilState = 'waiting';
let levelAvailable = false;

function publish(): void {
  store.set('boil', {
    state: st,
    duty,
    level: levelAvailable ? (gpio.readInput('BOIL_LEVEL') === 0 ? 'high' : 'low') : 'high',
  });
}

export async function init(): Promise<void> {
  pwm = await createPwm({ name: 'BOIL_SSR', ...pinmap.pwm.BOIL_SSR });
  await pwm.setDuty(0);
  if (pinmap.inputs.BOIL_LEVEL.bcm !== null) {
    gpio.acquireInput('BOIL_LEVEL', pinmap.inputs.BOIL_LEVEL.bcm, { pull: 'up' });
    levelAvailable = true;
  }
  st = 'off';
  duty = 0;
  publish();
  log.info('Boil initialised');
}

export async function setDuty(percent: number): Promise<void> {
  if (!pwm) throw new Error('boil.init() must be called first');
  duty = Math.max(0, Math.min(100, Math.round(percent)));
  if (duty > 0) {
    if (levelAvailable && gpio.readInput('BOIL_LEVEL') !== 0) {
      log.warn('Boil: level LOW — refusing to heat');
      st = 'off';
      await pwm.setDuty(0);
      duty = 0;
      publish();
      return;
    }
    st = 'boiling';
    await pwm.setDuty(duty);
  } else {
    st = 'off';
    await pwm.setDuty(0);
  }
  publish();
}

export async function bumpDuty(delta: number): Promise<void> { await setDuty(duty + delta); }
export async function start(): Promise<void> { await setDuty(duty > 0 ? duty : 50); }
export async function stop(): Promise<void> { await setDuty(0); }
export async function bringToBoil(): Promise<void> { await setDuty(100); st = 'auto_boiling'; publish(); }

export function getDuty(): number { return duty; }
export function getState(): BoilState { return st; }

export default { init, setDuty, bumpDuty, start, stop, bringToBoil, getDuty, getState };
