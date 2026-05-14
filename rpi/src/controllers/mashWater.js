'use strict';

// Mash water tracker — port of MashWater.c.
//
// Tracks the volume of water currently in the mash tun and in the boiler,
// accounting for losses (~1.13 L/kg soaked into the grain plus a constant
// 2.5 L that can't be pumped out).

const log = require('../util/logger');
const store = require('../state/store');
const params = require('../parameters/parameters');

const WATER_LOSS_TO_MASH_LPK = 1.13;       // L per kg of grain
const RESIDUAL_IN_MASH_AFTER_PUMPOUT = 2.5; // L

let mashWet = false;

function _publish() {
  store.set('mashWater', {
    inMash: +mwater.inMash.toFixed(3),
    inBoiler: +mwater.inBoiler.toFixed(3),
  });
}

const mwater = { inMash: 0.0, inBoiler: 0.0 };

function waterAddedToMashTun(litres) {
  if (!mashWet) {
    const grainKilos = params.get('fGrainWeightKilos') || 0;
    mwater.inMash = litres - (grainKilos * WATER_LOSS_TO_MASH_LPK);
    mashWet = true;
  } else {
    mwater.inMash += litres;
  }
  _publish();
  log.info(`MashWater: +${litres.toFixed(3)}L → inMash=${mwater.inMash.toFixed(3)}L`);
}

function mashTunDrained() {
  mwater.inBoiler += mwater.inMash;
  mwater.inMash = 0;
  _publish();
  log.info(`MashWater: drained → inBoiler=${mwater.inBoiler.toFixed(3)}L`);
}

function clear() {
  mwater.inMash = 0;
  mwater.inBoiler = 0;
  mashWet = false;
  _publish();
}

function inMash() { return mwater.inMash; }
function inBoiler() { return mwater.inBoiler; }

module.exports = { waterAddedToMashTun, mashTunDrained, clear, inMash, inBoiler };
