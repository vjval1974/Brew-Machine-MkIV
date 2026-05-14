'use strict';

// Hop dropper — port of hop_dropper.c. A rotating disc with notches drops one
// hop addition each time the optical limit transitions from "on cup" to "in
// gap" (low → high). The drive motor runs until that transition, plus a
// small delay (`uiHopDropperStopDelayms`), then stops.

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');
const params = require('../parameters/parameters');
const { sleep, debounceInput } = require('../hal/io_util');

const STATES = { STOPPED: 'stopped', DRIVING_NO_GAP: 'driving_no_gap', DRIVING_GAP: 'driving_gap' };

let busy = false;

function init() {
  gpio.acquireOutput('HOP_DROPPER', pinmap.outputs.HOP_DROPPER.bcm, 0);
  gpio.acquireInput('HOP_DROPPER_LIMIT', pinmap.inputs.HOP_DROPPER_LIMIT.bcm, { pull: 'up' });
  store.set('hopDropper', STATES.STOPPED);
  log.info('Hop dropper initialised');
}

function _drive(on) {
  gpio.writeOutput('HOP_DROPPER', on ? 1 : 0);
}

async function drop() {
  if (busy) {
    log.warn('Hop dropper: already running — drop request ignored');
    return false;
  }
  busy = true;
  try {
    _drive(true);
    store.set('hopDropper', STATES.DRIVING_NO_GAP);
    // wait for limit to go LOW (on the cup)
    while (true) {
      const low = (await debounceInput('HOP_DROPPER_LIMIT')) === 0;
      if (low) break;
      await sleep(10);
    }
    store.set('hopDropper', STATES.DRIVING_GAP);
    // wait for limit to return HIGH (in the gap between cups)
    while (true) {
      const high = (await debounceInput('HOP_DROPPER_LIMIT')) === 1;
      if (high) break;
      await sleep(10);
    }
    const stopDelay = params.get('uiHopDropperStopDelayms') || 110;
    await sleep(stopDelay);
    _drive(false);
    store.set('hopDropper', STATES.STOPPED);
    log.info('Hop dropper: dropped one addition');
    return true;
  } finally {
    busy = false;
  }
}

function startManual() {
  _drive(true);
  store.set('hopDropper', STATES.DRIVING_NO_GAP);
}

function stopManual() {
  _drive(false);
  store.set('hopDropper', STATES.STOPPED);
}

function state() { return store.get('hopDropper'); }

module.exports = { init, drop, startManual, stopManual, state, STATES };
