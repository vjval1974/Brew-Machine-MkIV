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

import gpio from '../hal/gpio';
import log from '../util/logger';
import store from '../state/store';
import pinmap from '../config/pinmap';
import { sleep, debounceInput } from '../hal/io_util';
import onewire from '../hal/onewire';
import * as valves from './valves';
import * as flow from './flow';
import * as mashWater from './mashWater';
import type { HltLevel, HltCmd } from '../types';

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
let stableTemp = NaN;
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

/** Three-sample stable read; only updates `stableTemp` when all three agree. */
async function stableHltTemp(tolerance = 2.0): Promise<number> {
  const t1 = await onewire.readSensor('HLT'); await sleep(900);
  const t2 = await onewire.readSensor('HLT'); await sleep(900);
  const t3 = await onewire.readSensor('HLT');
  if (Math.abs(t1 - t2) < tolerance && Math.abs(t2 - t3) < tolerance) {
    stableTemp = (t1 + t2 + t3) / 3;
  }
  return stableTemp;
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

async function maintainTemp(target: number): Promise<boolean> {
  const level = await getLevel();
  if (level === 'mid' || level === 'high') {
    const t = await stableHltTemp(2.0);
    if (t < target) { setHeater(true);  return false; }
    if (t > target + 0.1) { setHeater(false); return true; }
    return true;
  }
  setHeater(false);
  return false;
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
    const cachedTemp = await onewire.readCached('HLT', 800);
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
export function heatAndFill(targetC: number): Promise<void> {
  cmd = 'heat_and_fill';
  cmdParams = { setpoint: targetC };
  log.info(`HLT command: heat_and_fill ${targetC} °C`);
  return new Promise<void>((resolve, reject) => {
    pending.push({ cmd: 'heat_and_fill', resolve, reject });
  });
}

export function drain(litres: number): Promise<void> {
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
