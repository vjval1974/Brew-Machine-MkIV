'use strict';

// Boil valve — port of boil_valve.c. This is a motorised valve with an
// H-bridge (two relay outputs: OPEN drive, CLOSE drive) and two limit
// switches (`opened`, `closed`, active-low). The state machine:
//
//   closed → opening → opened   (asserted by closed-limit going HIGH, or
//                                opened-limit going LOW)
//   opened → closing → closed
//   *      → stopped on STOP

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');
const { sleep, debounceInput } = require('../hal/io_util');

const STATES = {
  OPENED: 'opened', CLOSED: 'closed',
  OPENING: 'opening', CLOSING: 'closing',
  STOPPED: 'stopped',
};

let cmd = 'stop';
let runner = null;

function _set(s) {
  store.set('boilValve', s);
  log.info(`BoilValve state -> ${s}`);
}

function init() {
  gpio.acquireOutput('BOIL_VALVE_OPEN',  pinmap.outputs.BOIL_VALVE_OPEN.bcm,  0);
  gpio.acquireOutput('BOIL_VALVE_CLOSE', pinmap.outputs.BOIL_VALVE_CLOSE.bcm, 0);
  gpio.acquireInput('BOIL_VALVE_OPENED', pinmap.inputs.BOIL_VALVE_OPENED.bcm, { pull: 'up' });
  gpio.acquireInput('BOIL_VALVE_CLOSED', pinmap.inputs.BOIL_VALVE_CLOSED.bcm, { pull: 'up' });
  _set(STATES.STOPPED);
  log.info('Boil valve initialised');
}

function _drive(direction) {
  if (direction === 'open') {
    gpio.writeOutput('BOIL_VALVE_CLOSE', 0);
    gpio.writeOutput('BOIL_VALVE_OPEN',  1);
  } else if (direction === 'close') {
    gpio.writeOutput('BOIL_VALVE_OPEN',  0);
    gpio.writeOutput('BOIL_VALVE_CLOSE', 1);
  } else {
    gpio.writeOutput('BOIL_VALVE_OPEN',  0);
    gpio.writeOutput('BOIL_VALVE_CLOSE', 0);
  }
}

async function _openedHit() { return (await debounceInput('BOIL_VALVE_OPENED')) === 0; }
async function _closedHit() { return (await debounceInput('BOIL_VALVE_CLOSED')) === 0; }

async function _loop() {
  for (;;) {
    if (cmd === 'stop') {
      _drive('stop');
      if (![STATES.OPENED, STATES.CLOSED].includes(state())) _set(STATES.STOPPED);
      await sleep(80);
      continue;
    }
    if (cmd === 'open') {
      if (await _openedHit()) {
        _drive('stop'); _set(STATES.OPENED); cmd = 'stop'; continue;
      }
      _drive('open'); _set(STATES.OPENING); await sleep(80); continue;
    }
    if (cmd === 'close') {
      if (await _closedHit()) {
        _drive('stop'); _set(STATES.CLOSED); cmd = 'stop'; continue;
      }
      _drive('close'); _set(STATES.CLOSING); await sleep(80); continue;
    }
  }
}

function open()  { cmd = 'open'; }
function close() { cmd = 'close'; }
function stop()  { cmd = 'stop'; }

function toggle() {
  if (state() === STATES.OPENED) close();
  else open();
}

function state() { return store.get('boilValve'); }

function start() {
  if (runner) return;
  runner = _loop().catch((err) => log.error(`BoilValve loop crashed: ${err.message}`));
}

module.exports = { init, start, open, close, stop, toggle, state, STATES };
