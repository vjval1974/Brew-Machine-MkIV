'use strict';

// Mash pump — port of mash_pump.c. The pump is interlocked: it will not run
// while a brew is running unless the crane is at the bottom or the mash
// valve is open (mirrors `OkToPump()`).

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');
const crane = require('./crane');
const valves = require('./valves');

function init() {
  const cfg = pinmap.outputs.MASH_PUMP;
  gpio.acquireOutput('MASH_PUMP', cfg.bcm, 0);
  store.set('pumps', { ...store.get('pumps'), mash: 'stopped' });
  log.info('Mash pump initialised');
}

function okToPump() {
  const brewRunning = store.get('brew').running === 'running';
  const craneAtBottom = crane.state() === 'at_bottom';
  const mashValveOpen = valves.state('MASH') === 'open';
  if (!brewRunning) return true;            // manual / idle
  return craneAtBottom || mashValveOpen;
}

function start() {
  if (state() === 'pumping') return true;
  if (!okToPump()) {
    log.warn('Mash pump: not safe to start (crane up & mash valve closed)');
    return false;
  }
  gpio.writeOutput('MASH_PUMP', 1);
  store.set('pumps', { ...store.get('pumps'), mash: 'pumping' });
  log.info('Mash pump: START');
  return true;
}

function stop() {
  if (state() === 'stopped') return;
  gpio.writeOutput('MASH_PUMP', 0);
  store.set('pumps', { ...store.get('pumps'), mash: 'stopped' });
  log.info('Mash pump: STOP');
}

function toggle() {
  if (state() === 'pumping') stop(); else start();
}

function state() { return store.get('pumps').mash; }

module.exports = { init, start, stop, toggle, state, okToPump };
