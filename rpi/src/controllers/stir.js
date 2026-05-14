'use strict';

// Stir motor — port of stir.c. Interlocked: only runs while a brew is
// running and the crane is either at the bottom or driving down in
// increments (otherwise STIR_DRIVING is rejected with a console warning,
// matching `OkToStir()`).

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');
const crane = require('./crane');

function init() {
  gpio.acquireOutput('STIR', pinmap.outputs.STIR.bcm, 0);
  store.set('stir', 'stopped');
  log.info('Stir motor initialised');
}

function okToStir() {
  const brewRunning = store.get('brew').running === 'running';
  if (!brewRunning) return true;
  const cs = crane.state();
  return cs === 'at_bottom' || cs === 'driving_down_incremental';
}

function start() {
  if (state() === 'driving') return;
  if (!okToStir()) {
    log.warn('Stir: NOT OK to stir (crane not at bottom)');
    return;
  }
  gpio.writeOutput('STIR', 1);
  store.set('stir', 'driving');
  log.info('Stir: START');
}

function stop() {
  if (state() === 'stopped') return;
  gpio.writeOutput('STIR', 0);
  store.set('stir', 'stopped');
  log.info('Stir: STOP');
}

function state() { return store.get('stir'); }

module.exports = { init, start, stop, state };
