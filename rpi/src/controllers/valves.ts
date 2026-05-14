// Valve controller — port of valves.c.

import gpio from '../hal/gpio';
import log from '../util/logger';
import store from '../state/store';
import pinmap from '../config/pinmap';
import type { ValveName } from '../types';

const NAMES: ValveName[] = ['HLT', 'MASH', 'INLET', 'CHILLER'];

interface ValveHooks {
  onOpen?: () => void;
  onClose?: () => void;
}

const callbacks: Partial<Record<ValveName, ValveHooks>> = {};

export function init(): void {
  for (const n of NAMES) {
    const cfg = pinmap.outputs[`${n}_VALVE`];
    if (!cfg) throw new Error(`Missing pinmap.outputs.${n}_VALVE`);
    gpio.acquireOutput(`${n}_VALVE`, cfg.bcm, 0);
    store.patch('valves', { [n]: 'closed' } as Partial<typeof store.state.valves>);
  }
  log.info('Valves initialised (HLT, MASH, INLET, CHILLER)');
}

export function open(name: ValveName): void {
  gpio.writeOutput(`${name}_VALVE`, 1);
  store.patch('valves', { [name]: 'open' } as Partial<typeof store.state.valves>);
  log.info(`Valve ${name}: OPEN`);
  callbacks[name]?.onOpen?.();
}

export function close(name: ValveName): void {
  gpio.writeOutput(`${name}_VALVE`, 0);
  store.patch('valves', { [name]: 'closed' } as Partial<typeof store.state.valves>);
  log.info(`Valve ${name}: CLOSE`);
  callbacks[name]?.onClose?.();
}

export function toggle(name: ValveName): void {
  if (state(name) === 'open') close(name);
  else open(name);
}

export function state(name: ValveName): 'open' | 'closed' {
  return store.state.valves[name];
}

export function setHooks(name: ValveName, hooks: ValveHooks): void {
  callbacks[name] = { ...callbacks[name], ...hooks };
}

export function closeAll(): void {
  for (const n of NAMES) close(n);
}

export { NAMES };

export default { init, open, close, toggle, state, setHooks, closeAll, NAMES };
