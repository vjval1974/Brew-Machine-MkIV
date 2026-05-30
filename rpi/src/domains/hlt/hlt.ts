// HLT (Hot Liquor Tank) — port of hlt.c.
//
// Commands return Promise<void> that resolves when the step is fully done:
//   heatAndFill(setpoint)  – fills via INLET, heats element to setpoint
//   drain(litres)          – opens HLT valve, counts flow until target hit
//   idle()                 – heater off, valves closed, no work
//
// An independent level-checker monitor (background) forces the SSR off when
// the level reads LOW while heating, and slams the inlet valve closed if it
// stays open with HIGH level for >3 s.

import gpio from '../../platform/hal/gpio';
import log from '../../platform/util/logger';
import store from '../../platform/store/store';
import pinmap from '../../config/pinmap';
import { sleep, debounceInput } from '../../platform/hal/io_util';
import watchdog from '../../platform/hal/watchdog';
import * as valves from '../hydraulics/valves';
import * as flow from '../hydraulics/flow';
import * as mashWater from '../hydraulics/mashWater';
import * as tempSensors from '../sensing/tempSensors';
import type { HltLevel, HltCmd } from '../../types';

const MAX_CONSECUTIVE_TEMP_FAULTS = 5;     // ~5 ticks; ~1s without stable read

interface HeatFillParams { setpoint: number }
interface DrainParams    { litres:   number }
type CmdParams = HeatFillParams | DrainParams | Record<string, never>;

interface CmdResolver {
  cmd: HltCmd;
  resolve: () => void;
  reject:  (err: Error) => void;
}

let setpoint = 74.5;
let cmd: HltCmd = 'idle';
let cmdParams: CmdParams = {};
const pending: CmdResolver[] = [];
let consecutiveTempFaults = 0;
let actualLitresDelivered = 0;
let runner: Promise<void> | null = null;
let levelRunner: Promise<void> | null = null;

function resolvePending(matchCmd: HltCmd): void {
  for (let i = pending.length - 1; i >= 0; i--) {
    if (pending[i]!.cmd === matchCmd) {
      pending[i]!.resolve();
      pending.splice(i, 1);
    }
  }
}

export function init(): void {
  gpio.acquireOutput('HLT_SSR', pinmap.outputs.HLT_SSR.bcm, 0);
  gpio.acquireInput('HLT_LEVEL_MID',  pinmap.inputs.HLT_LEVEL_MID.bcm,  { pull: 'up' });
  gpio.acquireInput('HLT_LEVEL_HIGH', pinmap.inputs.HLT_LEVEL_HIGH.bcm, { pull: 'up' });
  store.patch('hlt', { level: 'low', heating: false, cmd: 'idle', setpoint });
  log.info('HLT initialised');
}

async function getLevel(): Promise<HltLevel> {
  const high = (await debounceInput('HLT_LEVEL_HIGH')) === 0;
  if (high) return 'high';
  const mid = (await debounceInput('HLT_LEVEL_MID')) === 0;
  if (mid) return 'mid';
  return 'low';
}

function setHeater(on: boolean): void {
  gpio.writeOutput('HLT_SSR', on ? 1 : 0);
  store.patch('hlt', { heating: on });
}

async function maintainHighLevel(): Promise<boolean> {
  const high = (await debounceInput('HLT_LEVEL_HIGH')) === 0;
  if (high) {
    await sleep(2000);
    valves.close('INLET');
    return true;
  }
  valves.open('INLET');
  return false;
}

/**
 * Decide whether the heater should be on, given a target setpoint and the
 * current stable temperature read from the tempSensors rolling buffer.
 *
 * Fault behaviour: if the buffer can't produce a stable three-sample read
 * (no recent samples, samples disagree, or data is stale), increment a
 * consecutive-fault counter, turn the heater OFF, and report "not at temp".
 * After `MAX_CONSECUTIVE_TEMP_FAULTS` strikes, escalate to the hardware
 * watchdog so the external WDT IC drops the contactor coil.
 */
async function maintainTemp(target: number): Promise<boolean> {
  const level = await getLevel();
  if (level !== 'mid' && level !== 'high') {
    setHeater(false);
    return false;
  }

  const t = tempSensors.getStableTemp('HLT', 2.0);
  if (t === null) {
    setHeater(false);
    consecutiveTempFaults++;
    if (consecutiveTempFaults === 1) {
      log.warn('HLT: no stable temperature read — heater forced OFF, waiting for sensor');
    }
    if (consecutiveTempFaults >= MAX_CONSECUTIVE_TEMP_FAULTS) {
      log.error(`HLT: ${consecutiveTempFaults} consecutive temp faults — escalating to watchdog`);
      watchdog.fail('HLT temperature sensor faulted');
    }
    return false;
  }

  consecutiveTempFaults = 0;
  if (t < target)            { setHeater(true);  return false; }
  if (t > target + 0.1)      { setHeater(false); return true;  }
  return true;
}

async function loop(): Promise<void> {
  let drainDelayTicks = 0;
  let drainTargetL = 0;
  let lastCmd: HltCmd | null = null;

  // eslint-disable-next-line no-constant-condition
  for (;;) {
    if (cmd !== lastCmd) {
      drainDelayTicks = 0;
      lastCmd = cmd;
    }

    const level = await getLevel();
    const cachedTemp = tempSensors.get('HLT');
    store.patch('hlt', { level, temp: cachedTemp, cmd, setpoint });

    if (cmd === 'idle') {
      setHeater(false);
      valves.close('INLET');
      valves.close('HLT');
      await sleep(200);
      continue;
    }

    if (cmd === 'heat_and_fill') {
      valves.close('HLT');
      const filled = await maintainHighLevel();
      const target = (cmdParams as HeatFillParams).setpoint ?? setpoint;
      const hot    = await maintainTemp(target);
      if (filled && hot) {
        log.info('HLT: heat+fill complete');
        resolvePending('heat_and_fill');
        // After signalling completion, fall back to idle so the next
        // command issuer starts from a clean state. The HLT keeps the
        // element off; mid-state level holds because the inlet just closed.
        cmd = 'idle';
      }
      await sleep(200);
      continue;
    }

    if (cmd === 'drain') {
      if (drainDelayTicks === 0) {
        setHeater(false);
        valves.open('HLT');
        flow.reset();
        drainTargetL = (cmdParams as DrainParams).litres;
        log.info(`HLT: draining ${drainTargetL.toFixed(2)} L`);
      }
      drainDelayTicks++;
      if (drainDelayTicks > 10) {                   // 2s @ 200 ms ticks
        const delivered = flow.getLitres();
        if (delivered >= drainTargetL) {
          valves.close('HLT');
          await sleep(500);
          actualLitresDelivered += delivered;
          mashWater.waterAddedToMashTun(delivered);
          log.info(`HLT: drained ${delivered.toFixed(3)} L (target ${drainTargetL.toFixed(3)} L)`);
          resolvePending('drain');
          cmd = 'idle';
        }
      }
      await sleep(200);
      continue;
    }

    await sleep(200);
  }
}

async function levelMonitor(): Promise<void> {
  // eslint-disable-next-line no-constant-condition
  for (;;) {
    const level = await getLevel();
    const heating = store.state.hlt.heating;
    if (level === 'low' && heating) {
      setHeater(false);
      log.error('HLT level monitor: SSR was on while level LOW — INTERVENED');
    }
    if (level === 'high' && valves.state('INLET') === 'open') {
      await sleep(3000);
      const level2 = await getLevel();
      if (level2 === 'high' && valves.state('INLET') === 'open') {
        valves.close('INLET');
        log.error('HLT level monitor: INLET open with level HIGH for >3 s — INTERVENED');
      }
    }
    await sleep(500);
  }
}

export function start(): void {
  if (runner) return;
  runner = loop().catch((err: Error) => log.error(`HLT loop crashed: ${err.message}`));
  levelRunner = levelMonitor().catch((err: Error) => log.error(`HLT level monitor crashed: ${err.message}`));
}

// ── Public, awaitable command API ───────────────────────────────────────────

/**
 * Reject a same-typed pending command. We bound `pending` to one in-flight
 * request per command type to avoid unbounded queue growth from a chatty UI.
 */
function ensureOneInFlight(c: HltCmd): void {
  for (let i = pending.length - 1; i >= 0; i--) {
    if (pending[i]!.cmd === c) {
      log.warn(`HLT: superseding in-flight '${c}' command`);
      pending[i]!.reject(new Error('superseded'));
      pending.splice(i, 1);
    }
  }
}

export function heatAndFill(targetC: number): Promise<void> {
  ensureOneInFlight('heat_and_fill');
  cmd = 'heat_and_fill';
  cmdParams = { setpoint: targetC };
  log.info(`HLT command: heat_and_fill ${targetC} °C`);
  return new Promise<void>((resolve, reject) => {
    pending.push({ cmd: 'heat_and_fill', resolve, reject });
  });
}

export function drain(litres: number): Promise<void> {
  ensureOneInFlight('drain');
  cmd = 'drain';
  cmdParams = { litres };
  log.info(`HLT command: drain ${litres.toFixed(2)} L`);
  return new Promise<void>((resolve, reject) => {
    pending.push({ cmd: 'drain', resolve, reject });
  });
}

export function idle(): void {
  cmd = 'idle';
  cmdParams = {};
  log.info('HLT command: idle');
}

// ── Diagnostic / manual API ─────────────────────────────────────────────────
export function setSetpoint(t: number): void {
  setpoint = t;
  store.patch('hlt', { setpoint });
}

export function bumpSetpoint(delta: number): void { setSetpoint(setpoint + delta); }

/** Manual "start heating" from the diagnostics UI — heats to current setpoint. */
export function startHeating(): Promise<void> { return heatAndFill(setpoint); }
export function stopHeating(): void { idle(); }

export function getActualLitresDelivered(): number { return actualLitresDelivered; }
export function getSetpoint(): number { return setpoint; }

/** Abort all pending command promises (called by brew QUIT). */
export function abortAll(reason = 'aborted'): void {
  cmd = 'idle';
  for (const p of pending.splice(0)) p.reject(new Error(reason));
}

export default {
  init, start,
  heatAndFill, drain, idle,
  setSetpoint, bumpSetpoint,
  startHeating, stopHeating,
  getActualLitresDelivered, getSetpoint,
  abortAll,
};

// Reference suppression to silence "unused" warnings without removing the
// stored cache.
void levelRunner;
