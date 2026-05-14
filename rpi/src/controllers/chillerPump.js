'use strict';

// Chiller pump (also called "boil pump" in the original UI) — port of
// chiller_pump.c. Straight on/off with no interlocks.

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');

function init() {
  const cfg = pinmap.outputs.CHILLER_PUMP;
  gpio.acquireOutput('CHILLER_PUMP', cfg.bcm, 0);
  store.set('pumps', { ...store.get('pumps'), chiller: 'stopped' });
  log.info('Chiller pump initialised');
}

function start() {
  if (state() === 'pumping') return;
  gpio.writeOutput('CHILLER_PUMP', 1);
  store.set('pumps', { ...store.get('pumps'), chiller: 'pumping' });
  log.info('Chiller pump: START');
}

function stop() {
  if (state() === 'stopped') return;
  gpio.writeOutput('CHILLER_PUMP', 0);
  store.set('pumps', { ...store.get('pumps'), chiller: 'stopped' });
  log.info('Chiller pump: STOP');
}

function toggle() { if (state() === 'pumping') stop(); else start(); }
function state() { return store.get('pumps').chiller; }

module.exports = { init, start, stop, toggle, state };
