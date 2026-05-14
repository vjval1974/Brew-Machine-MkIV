'use strict';

// Grain mill motor — port of mill.c. Single GPIO on/off.

const gpio = require('../hal/gpio');
const log  = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');

function init() {
  gpio.acquireOutput('MILL', pinmap.outputs.MILL.bcm, 0);
  store.set('mill', 'stopped');
  log.info('Mill initialised');
}

function start() {
  gpio.writeOutput('MILL', 1);
  store.set('mill', 'driving');
  log.info('Mill: START');
}

function stop() {
  gpio.writeOutput('MILL', 0);
  store.set('mill', 'stopped');
  log.info('Mill: STOP');
}

function state() { return store.get('mill'); }

module.exports = { init, start, stop, state };
