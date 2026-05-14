'use strict';

// Boil kettle controller — port of boil.c.
//
// The boil element is driven through a slow PWM (1 Hz period in the original)
// at 0..100% duty. States:
//   off              — element disabled, duty 0
//   boiling          — manual or brew-driven duty
//   waiting          — initial state at boot before first command
//   bring_to_boil    — forced 100% duty
//
// Boil level (LOW/HIGH) inhibits heating when LOW; in the original code the
// HIGH read was hard-coded TRUE, suggesting the probe was unreliable. We
// retain that behaviour but expose the real level via the input if you wire
// one up.

const log = require('../util/logger');
const store = require('../state/store');
const pinmap = require('../../config/pinmap');
const gpio = require('../hal/gpio');
const { createPwm } = require('../hal/pwm');

const STATES = { OFF: 'off', BOILING: 'boiling', WAITING: 'waiting', AUTO_BOILING: 'auto_boiling' };

let pwm = null;
let duty = 0;
let state = STATES.WAITING;
let levelAvailable = false;

function _publish() {
  store.set('boil', {
    state, duty,
    level: levelAvailable ? (gpio.readInput('BOIL_LEVEL') === 0 ? 'high' : 'low') : 'high',
  });
}

async function init() {
  pwm = await createPwm({ name: 'BOIL_SSR', ...pinmap.pwm.BOIL_SSR });
  await pwm.setDuty(0);
  // Optional boil level probe — the original C had a hard-coded HIGH return.
  if (pinmap.inputs.BOIL_LEVEL.bcm !== null) {
    gpio.acquireInput('BOIL_LEVEL', pinmap.inputs.BOIL_LEVEL.bcm, { pull: 'up' });
    levelAvailable = true;
  }
  state = STATES.OFF;
  duty = 0;
  _publish();
  log.info('Boil initialised');
}

async function setDuty(percent) {
  duty = Math.max(0, Math.min(100, Math.round(percent)));
  if (duty > 0) {
    if (levelAvailable && gpio.readInput('BOIL_LEVEL') !== 0) {
      log.warn('Boil: level LOW — refusing to heat');
      state = STATES.OFF;
      await pwm.setDuty(0);
      duty = 0;
      _publish();
      return;
    }
    state = STATES.BOILING;
    await pwm.setDuty(duty);
  } else {
    state = STATES.OFF;
    await pwm.setDuty(0);
  }
  _publish();
}

async function bumpDuty(delta) { await setDuty(duty + delta); }
async function start() { await setDuty(duty > 0 ? duty : 50); }
async function stop()  { await setDuty(0); }
async function bringToBoil() { state = STATES.AUTO_BOILING; await setDuty(100); state = STATES.AUTO_BOILING; _publish(); }

function getDuty() { return duty; }
function getState() { return state; }

module.exports = { init, setDuty, bumpDuty, start, stop, bringToBoil, getDuty, getState, STATES };
