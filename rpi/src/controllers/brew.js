'use strict';

// Brew state machine — port of brew.c.
//
// The original is ~1100 lines of FreeRTOS task + 30+ entry `BrewSteps[]`
// array. Each step had a setup function (kicked off other tasks via queues)
// and a poll function (waited for completion). On the Pi each step is an
// async function; awaiting it is exactly "wait for completion".
//
// Steps are run sequentially. PAUSE pauses the wakeup tick; QUIT aborts the
// current step and forces all controllers to safe states.

const log = require('../util/logger');
const store = require('../state/store');
const params = require('../parameters/parameters');

const valves      = require('./valves');
const mashPump    = require('./mashPump');
const chillerPump = require('./chillerPump');
const mill        = require('./mill');
const stir        = require('./stir');
const crane       = require('./crane');
const hopDropper  = require('./hopDropper');
const hlt         = require('./hlt');
const boil        = require('./boil');
const boilValve   = require('./boilValve');
const flow        = require('./flow');
const mashWater   = require('./mashWater');

let running   = false;     // currently executing a step
let paused    = false;
let quitFlag  = false;
let currentStep = 0;
let stepStartSec = 0;
let secondsElapsed = 0;
let tickRunner = null;

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// abortable sleep — wakes early on quit/pause
async function abortableSleep(ms) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    if (quitFlag) throw new Error('QUIT');
    if (!paused) await sleep(Math.min(100, ms - (Date.now() - t0)));
    else await sleep(100);
  }
}

function _publish(stepName) {
  store.set('brew', {
    running: !running ? 'idle' : paused ? 'paused' : 'running',
    step: currentStep,
    stepName: stepName || steps[currentStep]?.name || 'Idle',
    secondsElapsed,
    stepElapsed: secondsElapsed - stepStartSec,
    maxSteps: steps.length,
  });
}

function safeStates() {
  log.warn('BREW: forcing safe states');
  boil.stop().catch(() => {});
  chillerPump.stop();
  mashPump.stop();
  mill.stop();
  stir.stop();
  valves.close('HLT');
  valves.close('INLET');
  valves.close('MASH');
  valves.close('CHILLER');
}

// Promise wrapper around an HLT command that resolves when the HLT reports
// completion via its onStepComplete callback.
function awaitHlt(name, paramsObj) {
  return new Promise((resolve, reject) => {
    let done = false;
    hlt.command(name, paramsObj, (which) => {
      if (done) return;
      done = true;
      log.info(`BREW: HLT step '${which}' complete`);
      resolve();
    });
    // pump the abortable sleep so quit/pause unblocks the await chain
    (async () => {
      while (!done) {
        if (quitFlag) { done = true; reject(new Error('QUIT')); return; }
        await sleep(200);
      }
    })();
  });
}

// ── BREW STEPS ──────────────────────────────────────────────────────────────
// Faithful translation of brew.c's BrewSteps[] sequence. Each function awaits
// completion before returning.

const steps = [
  {
    name: 'Mill',
    async run() {
      const minutes = params.get('iGrindTime') || 0;
      mill.start();
      await abortableSleep(minutes * 60 * 1000);
      mill.stop();
    },
  },
  {
    name: 'CraneToTop',
    async run() {
      crane.up();
      while (crane.state() !== crane.STATES.AT_TOP) {
        await abortableSleep(500);
      }
    },
  },
  {
    name: 'FillAndHeatHLTToStrike',
    async run() {
      await awaitHlt(hlt.CMD.HEAT_AND_FILL, { setpoint: params.get('fStrikeTemp') });
    },
  },
  {
    name: 'DrainStrikeWaterToMashTun',
    async run() {
      // Open mash valve to mash tun, drain `fStrikeLitres` from HLT
      valves.open('MASH');                                  // To Mash
      await sleep(500);
      await awaitHlt(hlt.CMD.DRAIN, { litres: params.get('fStrikeLitres') });
      valves.close('MASH');
    },
  },
  {
    name: 'CraneToBottom',
    async run() {
      crane.incremental();
      while (crane.state() !== crane.STATES.AT_BOTTOM) {
        await abortableSleep(500);
      }
    },
  },
  {
    name: 'Mash',
    async run() {
      const mashMin = params.get('iMashTime') || 60;
      const pumpT1  = params.get('iPumpTime1') || 0;
      const stirT1  = params.get('iStirTime1') || 0;
      const pumpT2  = params.get('iPumpTime2') || 0;
      const stirT2  = params.get('iStirTime2') || 0;

      // initial mix / pump cycle
      if (stirT1 > 0) { stir.start(); await abortableSleep(stirT1 * 60_000); stir.stop(); }
      if (pumpT1 > 0) { mashPump.start(); await abortableSleep(pumpT1 * 60_000); mashPump.stop(); }

      // Hold at mash temp for the remainder
      const remainingMs = (mashMin - stirT1 - pumpT1) * 60_000;
      if (remainingMs > 0) await abortableSleep(remainingMs);

      if (pumpT2 > 0) { mashPump.start(); await abortableSleep(pumpT2 * 60_000); mashPump.stop(); }
      if (stirT2 > 0) { stir.start(); await abortableSleep(stirT2 * 60_000); stir.stop(); }
    },
  },
  {
    name: 'MashOut',
    async run() {
      const mins = params.get('iMashOutTime') || 0;
      if (mins <= 0) return;
      await awaitHlt(hlt.CMD.HEAT_AND_FILL, { setpoint: params.get('fMashOutTemp') });
      valves.open('MASH');
      await sleep(500);
      await awaitHlt(hlt.CMD.DRAIN, { litres: params.get('fMashOutLitres') });
      valves.close('MASH');
      await abortableSleep(mins * 60_000);
    },
  },
  {
    name: 'Sparge',
    async run() {
      const mins = params.get('iSpargeTime') || 0;
      const spargeL = params.get('fSpargeLitres') || 0;
      await awaitHlt(hlt.CMD.HEAT_AND_FILL, { setpoint: params.get('fSpargeTemp') });
      valves.open('MASH');
      await sleep(500);
      await awaitHlt(hlt.CMD.DRAIN, { litres: spargeL });
      valves.close('MASH');
      mashPump.start();
      mashWater.mashTunDrained();
      await abortableSleep(mins * 60_000);
      mashPump.stop();
    },
  },
  {
    name: 'PumpMashToBoil',
    async run() {
      boilValve.open();
      while (boilValve.state() !== 'opened') await abortableSleep(200);
      mashPump.start();
      const onMs  = (params.get('uiPumpToBoilRecycleOnTime')  || 30) * 1000;
      const offMs = (params.get('uiPumpToBoilRecycleOffTime') || 300) * 1000;
      // recycle pattern: pump for onMs, rest for offMs, two cycles
      for (let i = 0; i < 2; i++) {
        await abortableSleep(onMs);
        mashPump.stop();
        await abortableSleep(offMs);
        mashPump.start();
      }
      mashPump.stop();
      boilValve.close();
      mashWater.mashTunDrained();
    },
  },
  {
    name: 'BringToBoil',
    async run() {
      await boil.bringToBoil();
      const ms = (params.get('uiBringToBoilTime') || 0) * 60_000;
      await abortableSleep(ms);
    },
  },
  {
    name: 'Boil',
    async run() {
      const boilMin = params.get('uiBoilTime') || 60;
      const hopTimes = params.get('uiHopTimes') || [];
      await boil.setDuty(60);
      // schedule hop drops at (boilMin - hopTime) minutes from now
      const scheduled = hopTimes
        .map((t, i) => ({ index: i, at: (boilMin - t) * 60_000 }))
        .filter((x) => x.at >= 0)
        .sort((a, b) => a.at - b.at);
      const t0 = Date.now();
      let nextIdx = 0;
      while (Date.now() - t0 < boilMin * 60_000) {
        if (nextIdx < scheduled.length && (Date.now() - t0) >= scheduled[nextIdx].at) {
          log.info(`BREW: hop addition #${scheduled[nextIdx].index + 1}`);
          hopDropper.drop().catch((err) => log.warn(`hop drop: ${err.message}`));
          nextIdx++;
        }
        await abortableSleep(1000);
      }
      await boil.stop();
    },
  },
  {
    name: 'Settle',
    async run() {
      const mins = params.get('uiSettlingTime') || 0;
      if (mins > 0) await abortableSleep(mins * 60_000);
    },
  },
  {
    name: 'Chill',
    async run() {
      valves.open('CHILLER');
      chillerPump.start();
      const ms = (params.get('uiChillTime') || 20) * 60_000;
      await abortableSleep(ms);
      chillerPump.stop();
      valves.close('CHILLER');
    },
  },
  {
    name: 'PumpToFermenter',
    async run() {
      chillerPump.start();
      const ms = (params.get('uiPumpToFermenterTime') || 7) * 60_000;
      await abortableSleep(ms);
      chillerPump.stop();
    },
  },
  {
    name: 'Done',
    async run() {
      safeStates();
      mashWater.clear();
    },
  },
];

async function _runSteps() {
  running = true;
  paused = false;
  quitFlag = false;
  currentStep = 0;
  stepStartSec = secondsElapsed = 0;
  _publish();

  while (currentStep < steps.length) {
    if (quitFlag) break;
    const step = steps[currentStep];
    log.info(`BREW: step ${currentStep} ${step.name} START`);
    stepStartSec = secondsElapsed;
    _publish(step.name);
    try {
      await step.run();
    } catch (err) {
      if (err.message === 'QUIT') {
        log.warn(`BREW: step ${step.name} aborted by quit`);
        break;
      }
      log.error(`BREW: step ${step.name} threw: ${err.message}`);
      break;
    }
    log.info(`BREW: step ${currentStep} ${step.name} COMPLETE`);
    currentStep++;
  }

  running = false;
  safeStates();
  _publish('Idle');
  log.info('BREW: finished');
}

function start() {
  if (running) return false;
  log.info('BREW: starting');
  _tick();
  _runSteps().catch((err) => log.error(`Brew runner crashed: ${err.message}`));
  return true;
}

function pause()    { paused = true;  _publish(); log.info('BREW: paused'); }
function resume()   { paused = false; _publish(); log.info('BREW: resumed'); }
function quit()     { quitFlag = true; log.warn('BREW: quit requested'); }
function skip()     { /* implemented as part of step semantics if needed */ }

function gotoStep(idx) {
  if (idx < 0 || idx >= steps.length) return;
  currentStep = idx;
  _publish(steps[idx].name);
}

function _tick() {
  if (tickRunner) return;
  tickRunner = setInterval(() => {
    if (running && !paused) {
      secondsElapsed++;
      _publish();
    }
  }, 1000);
}

function getSteps() { return steps.map((s, i) => ({ index: i, name: s.name })); }

module.exports = { start, pause, resume, quit, skip, gotoStep, getSteps, safeStates };
