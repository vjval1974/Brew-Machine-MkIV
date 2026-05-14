// Mash water tracker — port of MashWater.c.

import log from '../util/logger';
import store from '../state/store';
import params from '../parameters/parameters';

const WATER_LOSS_TO_MASH_LPK = 1.13;          // L per kg of grain
const RESIDUAL_IN_MASH_AFTER_PUMPOUT = 2.5;   // L (kept for reference)
// Referenced by docs / future expansion of the tracker.
void RESIDUAL_IN_MASH_AFTER_PUMPOUT;

let mashWet = false;
const water = { inMash: 0.0, inBoiler: 0.0 };

function publish(): void {
  store.set('mashWater', {
    inMash:   +water.inMash.toFixed(3),
    inBoiler: +water.inBoiler.toFixed(3),
  });
}

export function waterAddedToMashTun(litres: number): void {
  if (!mashWet) {
    const grainKilos = params.get('fGrainWeightKilos') ?? 0;
    water.inMash = litres - (grainKilos * WATER_LOSS_TO_MASH_LPK);
    mashWet = true;
  } else {
    water.inMash += litres;
  }
  publish();
  log.info(`MashWater: +${litres.toFixed(3)}L → inMash=${water.inMash.toFixed(3)}L`);
}

export function mashTunDrained(): void {
  water.inBoiler += water.inMash;
  water.inMash = 0;
  publish();
  log.info(`MashWater: drained → inBoiler=${water.inBoiler.toFixed(3)}L`);
}

export function clear(): void {
  water.inMash = 0;
  water.inBoiler = 0;
  mashWet = false;
  publish();
}

export function inMash(): number { return water.inMash; }
export function inBoiler(): number { return water.inBoiler; }

export default { waterAddedToMashTun, mashTunDrained, clear, inMash, inBoiler };
