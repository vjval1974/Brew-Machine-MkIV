'use strict';

// Valve controller — port of valves.c.
//
// Original: 4 GPIO valves (HLT, MASH, INLET, CHILLER). Each has an onOpen /
// onClose callback. Opening the HLT valve starts boil-flow measurement; closing
// it stops it. The boil valve (motorised, with limit switches) lives in its
// own module (boilValve.js).

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');

const NAMES = ['HLT', 'MASH', 'INLET', 'CHILLER'];
const STATE_OPEN = 'open';
const STATE_CLOSED = 'closed';

const callbacks = { HLT: { onOpen: null, onClose: null } };

function init() {
  for (const n of NAMES) {
    const cfg = pinmap.outputs[`${n}_VALVE`];
    gpio.acquireOutput(`${n}_VALVE`, cfg.bcm, 0);
    store.state.valves[n] = STATE_CLOSED;
  }
  log.info('Valves initialised (HLT, MASH, INLET, CHILLER)');
}

function open(name) {
  if (!NAMES.includes(name)) throw new Error(`Unknown valve: ${name}`);
  gpio.writeOutput(`${name}_VALVE`, 1);
  store.patch('valves', { [name]: STATE_OPEN });
  log.info(`Valve ${name}: OPEN`);
  const cb = callbacks[name];
  if (cb && cb.onOpen) cb.onOpen();
}

function close(name) {
  if (!NAMES.includes(name)) throw new Error(`Unknown valve: ${name}`);
  gpio.writeOutput(`${name}_VALVE`, 0);
  store.patch('valves', { [name]: STATE_CLOSED });
  log.info(`Valve ${name}: CLOSE`);
  const cb = callbacks[name];
  if (cb && cb.onClose) cb.onClose();
}

function toggle(name) {
  if (state(name) === STATE_OPEN) close(name);
  else open(name);
}

function state(name) {
  return store.state.valves[name];
}

// Wire the HLT valve onOpen/onClose hooks (flow measurement start/stop).
function setHooks(name, hooks) {
  callbacks[name] = { ...callbacks[name], ...hooks };
}

function closeAll() {
  for (const n of NAMES) close(n);
}

module.exports = { init, open, close, toggle, state, setHooks, closeAll, NAMES };
