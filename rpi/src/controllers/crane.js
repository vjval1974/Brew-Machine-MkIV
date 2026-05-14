'use strict';

// Crane controller — port of crane.c. Drives the grain-basket lift up and
// down between two limit switches. Three commands: UP, DOWN, DOWN_INCREMENTAL
// (for mash-in cup stirring — pulses down a few times, stirs in between).
//
// State machine:
//   stopped → driving_up → at_top
//   at_top  → driving_down → at_bottom
//   driving_down_incremental: pulse-down-stop loop, kicks stir on after 11
//                             increments, ends when lower limit asserts.

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');
const { sleep, debounceInput } = require('../hal/io_util');
let stir = null;   // lazy require to avoid circular dep with stir.js

const STATES = {
  AT_TOP: 'at_top',
  AT_BOTTOM: 'at_bottom',
  DRIVING_UP: 'driving_up',
  DRIVING_DOWN: 'driving_down',
  DRIVING_DOWN_INC: 'driving_down_incremental',
  STOPPED: 'stopped',
};

let currentCmd = 'stop';      // up | down | incremental | stop
let runner = null;            // active promise of the control loop

function _set(state) {
  store.set('crane', state);
  log.info(`Crane state -> ${state}`);
}

function init() {
  gpio.acquireOutput('CRANE_UP',   pinmap.outputs.CRANE_UP.bcm,   0);
  gpio.acquireOutput('CRANE_DOWN', pinmap.outputs.CRANE_DOWN.bcm, 0);
  gpio.acquireInput('CRANE_UPPER_LIMIT', pinmap.inputs.CRANE_UPPER_LIMIT.bcm, { pull: 'up' });
  gpio.acquireInput('CRANE_LOWER_LIMIT', pinmap.inputs.CRANE_LOWER_LIMIT.bcm, { pull: 'up' });
  _set(STATES.STOPPED);
  log.info('Crane initialised');
}

function _drive(dir) {
  // active-high H-bridge: never assert both at once.
  if (dir === 'up') {
    gpio.writeOutput('CRANE_DOWN', 0);
    gpio.writeOutput('CRANE_UP',   1);
  } else if (dir === 'down') {
    gpio.writeOutput('CRANE_UP',   0);
    gpio.writeOutput('CRANE_DOWN', 1);
  } else {
    gpio.writeOutput('CRANE_UP',   0);
    gpio.writeOutput('CRANE_DOWN', 0);
  }
}

// limit switches: active-low (input reads 0 when limit hit)
async function _upperHit() { return (await debounceInput('CRANE_UPPER_LIMIT')) === 0; }
async function _lowerHit() { return (await debounceInput('CRANE_LOWER_LIMIT')) === 0; }

async function _loop() {
  for (;;) {
    if (currentCmd === 'stop') {
      _drive('stop');
      if (store.get('crane') === STATES.DRIVING_UP || store.get('crane') === STATES.DRIVING_DOWN) {
        _set(STATES.STOPPED);
      }
      await sleep(80);
      continue;
    }

    if (currentCmd === 'up') {
      if (await _upperHit()) {
        _drive('stop');
        _set(STATES.AT_TOP);
        currentCmd = 'stop';
        continue;
      }
      _drive('up');
      _set(STATES.DRIVING_UP);
      await sleep(80);
      continue;
    }

    if (currentCmd === 'down') {
      if (await _lowerHit()) {
        _drive('stop');
        _set(STATES.AT_BOTTOM);
        currentCmd = 'stop';
        continue;
      }
      _drive('down');
      _set(STATES.DRIVING_DOWN);
      await sleep(80);
      continue;
    }

    if (currentCmd === 'incremental') {
      _set(STATES.DRIVING_DOWN_INC);
      let increments = 0;
      while (currentCmd === 'incremental') {
        if (await _lowerHit()) {
          _drive('down');                 // make sure we're seated
          await sleep(100);
          _drive('stop');
          _set(STATES.AT_BOTTOM);
          if (!stir) stir = require('./stir');
          stir.stop();
          currentCmd = 'stop';
          break;
        }
        _drive('down');
        await sleep(200);
        _drive('stop');
        await sleep(1500);
        increments++;
        if (increments === 11) {
          if (!stir) stir = require('./stir');
          stir.start();
        }
      }
      continue;
    }
  }
}

function up()          { currentCmd = 'up'; }
function down()        { currentCmd = 'down'; }
function incremental() { currentCmd = 'incremental'; }
function stop()        { currentCmd = 'stop'; }

function state() { return store.get('crane'); }

// Start the background loop after init() has been called.
function start() {
  if (runner) return;
  runner = _loop().catch((err) => log.error(`Crane loop crashed: ${err.message}`));
}

module.exports = { init, start, up, down, incremental, stop, state, STATES };
