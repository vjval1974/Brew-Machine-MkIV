'use strict';

// HLT (Hot Liquor Tank) — port of hlt.c.
//
// Inputs:  HLT_LEVEL_MID, HLT_LEVEL_HIGH (active-low float switches)
// Output:  HLT_SSR (drives the heating element via a solid-state relay)
//
// Commands:
//   idle           — element off, valves closed
//   heat_and_fill  — fill via INLET valve to high-level, heat to setpoint
//   drain          — open HLT valve until `litresToDrain` reaches target via
//                    flow counter
//
// Safety: an independent monitor (`levelChecker`) forces the SSR off if the
// level reads LOW while heating, and slams the inlet valve closed if it stays
// open with level HIGH for >3s. Direct port of `vTaskHLTLevelChecker`.

const gpio   = require('../hal/gpio');
const log    = require('../util/logger');
const store  = require('../state/store');
const pinmap = require('../../config/pinmap');
const { sleep, debounceInput } = require('../hal/io_util');
const onewire = require('../hal/onewire');
const valves = require('./valves');
const flow = require('./flow');
const mashWater = require('./mashWater');

const LEVEL = { LOW: 'low', MID: 'mid', HIGH: 'high' };
const CMD   = { IDLE: 'idle', HEAT_AND_FILL: 'heat_and_fill', DRAIN: 'drain' };

let setpoint = 74.5;
let cmd = CMD.IDLE;
let cmdParams = {};
let onStepComplete = null;
let stepCompleteFired = false;
let stableTemp = NaN;
let actualLitresDelivered = 0;
let runner = null;
let levelRunner = null;

function init() {
  gpio.acquireOutput('HLT_SSR', pinmap.outputs.HLT_SSR.bcm, 0);
  gpio.acquireInput('HLT_LEVEL_MID',  pinmap.inputs.HLT_LEVEL_MID.bcm,  { pull: 'up' });
  gpio.acquireInput('HLT_LEVEL_HIGH', pinmap.inputs.HLT_LEVEL_HIGH.bcm, { pull: 'up' });
  store.patch('hlt', { level: LEVEL.LOW, heating: false, cmd: CMD.IDLE, setpoint });
  log.info('HLT initialised');
}

async function _getLevel() {
  const high = (await debounceInput('HLT_LEVEL_HIGH')) === 0;
  if (high) return LEVEL.HIGH;
  const mid = (await debounceInput('HLT_LEVEL_MID')) === 0;
  if (mid) return LEVEL.MID;
  return LEVEL.LOW;
}

function _setHeater(on) {
  gpio.writeOutput('HLT_SSR', on ? 1 : 0);
  store.patch('hlt', { heating: !!on });
}

// Take 3 samples ~900 ms apart; only return new mean if all three are within
// `tolerance` of each other. Matches `fGetStableHltTemp` exactly.
async function _stableTemp(tolerance = 2.0) {
  const t1 = await onewire.readSensor('HLT'); await sleep(900);
  const t2 = await onewire.readSensor('HLT'); await sleep(900);
  const t3 = await onewire.readSensor('HLT');
  if (Math.abs(t1 - t2) < tolerance && Math.abs(t2 - t3) < tolerance) {
    stableTemp = (t1 + t2 + t3) / 3;
  }
  return stableTemp;
}

async function _maintainHighLevel() {
  const high = (await debounceInput('HLT_LEVEL_HIGH')) === 0;
  if (high) {
    await sleep(2000);
    valves.close('INLET');
    return true;
  }
  valves.open('INLET');
  return false;
}

async function _maintainTemp(target) {
  const level = await _getLevel();
  if (level === LEVEL.MID || level === LEVEL.HIGH) {
    const t = await _stableTemp(2.0);
    if (t < target) {
      _setHeater(true);
      return false;
    }
    if (t > target + 0.1) {
      _setHeater(false);
      return true;
    }
    return true;
  }
  _setHeater(false);
  return false;
}

async function _loop() {
  let drainValveDelayTicks = 0;
  let drainTargetL = 0;
  let lastCmd = null;

  for (;;) {
    if (cmd !== lastCmd) {
      drainValveDelayTicks = 0;
      stepCompleteFired = false;
      lastCmd = cmd;
    }

    const level = await _getLevel();
    store.patch('hlt', { level, temp: await onewire.readCached('HLT', 800), cmd, setpoint });

    if (cmd === CMD.IDLE) {
      _setHeater(false);
      valves.close('INLET');
      valves.close('HLT');
      await sleep(200);
      continue;
    }

    if (cmd === CMD.HEAT_AND_FILL) {
      valves.close('HLT');
      const filled = await _maintainHighLevel();
      const hot    = await _maintainTemp(cmdParams.setpoint || setpoint);
      if (filled && hot && !stepCompleteFired) {
        log.info('HLT: heat+fill complete');
        stepCompleteFired = true;
        if (onStepComplete) onStepComplete('heat_and_fill');
      }
      await sleep(200);
      continue;
    }

    if (cmd === CMD.DRAIN) {
      if (drainValveDelayTicks === 0) {
        _setHeater(false);
        valves.open('HLT');
        flow.reset();
        drainTargetL = cmdParams.litres;
        log.info(`HLT: draining ${drainTargetL.toFixed(2)}L`);
      }
      drainValveDelayTicks++;
      if (drainValveDelayTicks > 10) {  // 2 s @ 200 ms ticks before counting
        const delivered = flow.getLitres();
        if (delivered >= drainTargetL && !stepCompleteFired) {
          valves.close('HLT');
          await sleep(500);
          actualLitresDelivered += delivered;
          mashWater.waterAddedToMashTun(delivered);
          log.info(`HLT: drained ${delivered.toFixed(3)}L (target ${drainTargetL.toFixed(3)}L)`);
          stepCompleteFired = true;
          if (onStepComplete) onStepComplete('drain');
          cmd = CMD.IDLE;
        }
      }
      await sleep(200);
      continue;
    }

    await sleep(200);
  }
}

async function _levelMonitor() {
  for (;;) {
    const level = await _getLevel();
    const heating = store.get('hlt').heating;
    if (level === LEVEL.LOW && heating) {
      _setHeater(false);
      log.error('HLT level monitor: SSR was on while level LOW — INTERVENED');
    }
    if (level === LEVEL.HIGH && valves.state('INLET') === 'open') {
      await sleep(3000);
      const level2 = await _getLevel();
      if (level2 === LEVEL.HIGH && valves.state('INLET') === 'open') {
        valves.close('INLET');
        log.error('HLT level monitor: INLET open with level HIGH for >3s — INTERVENED');
      }
    }
    await sleep(500);
  }
}

function start() {
  if (runner) return;
  runner = _loop().catch((err) => log.error(`HLT loop crashed: ${err.message}`));
  levelRunner = _levelMonitor().catch((err) => log.error(`HLT level monitor crashed: ${err.message}`));
}

function command(name, paramsObj = {}, onComplete = null) {
  cmd = name;
  cmdParams = paramsObj;
  onStepComplete = onComplete;
  log.info(`HLT command: ${name} ${JSON.stringify(paramsObj)}`);
}

function setSetpoint(t) {
  setpoint = t;
  store.patch('hlt', { setpoint });
}

function bumpSetpoint(delta) { setSetpoint(setpoint + delta); }

function startHeating() {
  // diagnostic mode — heat to current setpoint, ignoring fill
  command(CMD.HEAT_AND_FILL, { setpoint });
}

function stopHeating() { command(CMD.IDLE); }

function getActualLitresDelivered() { return actualLitresDelivered; }
function getSetpoint() { return setpoint; }

module.exports = {
  init, start, command,
  setSetpoint, bumpSetpoint,
  startHeating, stopHeating,
  getActualLitresDelivered, getSetpoint,
  CMD, LEVEL,
};
