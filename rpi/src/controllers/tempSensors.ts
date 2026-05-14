// Temperature poller — reads HLT + MASH DS18B20s every second.

import log from '../util/logger';
import onewire from '../hal/onewire';
import store from '../state/store';

let runner: Promise<void> | null = null;

async function loop(): Promise<void> {
  // eslint-disable-next-line no-constant-condition
  while (true) {
    try {
      const hlt  = await onewire.readSensor('HLT');
      const mash = await onewire.readSensor('MASH');
      store.set('temps', { HLT: hlt, MASH: mash });
    } catch (err) {
      log.warn(`Temp poll: ${(err as Error).message}`);
    }
    await new Promise((r) => setTimeout(r, 1000));
  }
}

export function start(): void {
  if (runner) return;
  runner = loop().catch((err: Error) => log.error(`Temp loop crashed: ${err.message}`));
}

export function get(name: 'HLT' | 'MASH'): number {
  return store.state.temps[name];
}

export default { start, get };
