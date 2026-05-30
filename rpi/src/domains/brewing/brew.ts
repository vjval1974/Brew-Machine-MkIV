// Brew state machine — port of brew.c with the original concurrent
// step-launch model preserved.
//
// Each step has a `wait` flag (mirrors `ucWait` in the C `BrewSteps[]` array):
//   wait=false → fire `run()` and immediately advance to the next step.
//   wait=true  → first await ALL previously launched steps to complete, then
//                fire `run()`.
//
// This is the same semantics as `bDoesNextStepRequirePreviousStepsComplete`
// at brew.c:375. Critical for total brew duration: the HLT can be reheating
// the next batch of sparge water WHILE the current sparge / mash is running.

import log from '../../platform/util/logger';
import store from '../../platform/store/store';
import params from '../../platform/parameters/parameters';
import { sleep } from '../../platform/hal/io_util';

import * as valves      from '../hydraulics/valves';
import * as mashPump    from '../hydraulics/mashPump';
import * as chillerPump from '../hydraulics/chillerPump';
import * as mill        from '../motion/mill';
import * as stir        from '../motion/stir';
import * as crane       from '../motion/crane';
import * as hopDropper  from '../motion/hopDropper';
import * as hlt         from '../hlt/hlt';
import * as boil        from '../boil/boil';
import * as boilValve   from '../hydraulics/boilValve';
import * as flow        from '../hydraulics/flow';
import * as mashWater   from '../hydraulics/mashWater';

import type { BrewStepDef } from '../../types';

// ── Run-state ──────────────────────────────────────────────────────────────
let running    = false;
let paused     = false;
let quitFlag   = false;
let currentStep    = 0;
let stepStartSec   = 0;
let secondsElapsed = 0;
let tickRunner: NodeJS.Timeout | null = null;

// Test-only hooks. Production code never sets these; the safety regression
// test in scripts/test-safety.ts uses them to verify the failure-abort path
// of the brew engine. Read by `runBrew` at the top of each step launch.
export const _testHooks: { failStepAt: number | null } = {
  failStepAt: null,
};

function publish(stepName?: string): void {
  const r = !running ? 'idle' : paused ? 'paused' : 'running';
  store.set('brew', {
    running:        r,
    step:           currentStep,
    stepName:       stepName ?? steps[currentStep]?.name ?? 'Idle',
    secondsElapsed,
    stepElapsed:    secondsElapsed - stepStartSec,
    maxSteps:       steps.length,
  });
}

function checkQuit(): void { if (quitFlag) throw new Error('QUIT'); }

/** Sleep that wakes early on quit and pauses tick-counting while paused. */
async function abortableSleep(ms: number): Promise<void> {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    checkQuit();
    await sleep(Math.min(100, ms - (Date.now() - t0)));
  }
}

export function safeStates(): void {
  log.warn('BREW: forcing safe states');
  void boil.stop();
  chillerPump.stop();
  mashPump.stop();
  mill.stop();
  stir.stop();
  valves.close('HLT');
  valves.close('INLET');
  valves.close('MASH');
  valves.close('CHILLER');
  hlt.idle();
  boilValve.stop();
  crane.stop();
}

// ── Helper run functions ───────────────────────────────────────────────────
const minToMs = (m: number): number => m * 60_000;

async function mashCycle(): Promise<void> {
  const mashMin = params.get('iMashTime')  ?? 60;
  const stirT1  = params.get('iStirTime1') ?? 0;
  const pumpT1  = params.get('iPumpTime1') ?? 0;
  const pumpT2  = params.get('iPumpTime2') ?? 0;
  const stirT2  = params.get('iStirTime2') ?? 0;

  if (stirT1 > 0) { stir.start();     await abortableSleep(minToMs(stirT1)); stir.stop(); }
  if (pumpT1 > 0) { mashPump.start(); await abortableSleep(minToMs(pumpT1)); mashPump.stop(); }

  const remaining = mashMin - stirT1 - pumpT1 - pumpT2 - stirT2;
  if (remaining > 0) await abortableSleep(minToMs(remaining));

  if (pumpT2 > 0) { mashPump.start(); await abortableSleep(minToMs(pumpT2)); mashPump.stop(); }
  if (stirT2 > 0) { stir.start();     await abortableSleep(minToMs(stirT2)); stir.stop(); }
}

async function spargeCycle(): Promise<void> {
  const spMin  = params.get('iSpargeTime')      ?? 0;
  const pump1  = params.get('iSpargePumpTime1') ?? 0;
  const stir1  = params.get('iSpargeStirTime1') ?? 0;
  const pump2  = params.get('iSpargePumpTime2') ?? 0;
  const stir2  = params.get('iSpargeStirTime2') ?? 0;

  if (stir1 > 0) { stir.start();     await abortableSleep(minToMs(stir1)); stir.stop(); }
  if (pump1 > 0) { mashPump.start(); await abortableSleep(minToMs(pump1)); mashPump.stop(); }

  const remaining = spMin - stir1 - pump1 - pump2 - stir2;
  if (remaining > 0) await abortableSleep(minToMs(remaining));

  if (pump2 > 0) { mashPump.start(); await abortableSleep(minToMs(pump2)); mashPump.stop(); }
  if (stir2 > 0) { stir.start();     await abortableSleep(minToMs(stir2)); stir.stop(); }
}

async function pumpMashToBoilCycle(totalSeconds: number): Promise<void> {
  await boilValve.open();
  mashPump.start();
  const onMs  = (params.get('uiPumpToBoilRecycleOnTime')  ?? 30) * 1000;
  const offMs = (params.get('uiPumpToBoilRecycleOffTime') ?? 300) * 1000;
  const totalMs = totalSeconds * 1000;
  const t0 = Date.now();
  while (Date.now() - t0 < totalMs) {
    await abortableSleep(Math.min(onMs,  totalMs - (Date.now() - t0)));
    if (Date.now() - t0 >= totalMs) break;
    mashPump.stop();
    await abortableSleep(Math.min(offMs, totalMs - (Date.now() - t0)));
    mashPump.start();
  }
  mashPump.stop();
  mashWater.mashTunDrained();
}

async function boilCycle(): Promise<void> {
  const boilMin  = params.get('uiBoilTime') ?? 60;
  const hopTimes = params.get('uiHopTimes') ?? [];
  await boil.setDuty(60);
  const scheduled = hopTimes
    .map((t: number, i: number) => ({ index: i, at: minToMs(boilMin - t) }))
    .filter((x: { at: number }) => x.at >= 0)
    .sort((a, b) => a.at - b.at);

  const totalMs = minToMs(boilMin);
  const t0 = Date.now();
  let nextIdx = 0;
  while (Date.now() - t0 < totalMs) {
    if (nextIdx < scheduled.length && Date.now() - t0 >= scheduled[nextIdx]!.at) {
      log.info(`BREW: hop addition #${scheduled[nextIdx]!.index + 1}`);
      hopDropper.drop().catch((err: Error) => log.warn(`hop drop: ${err.message}`));
      nextIdx++;
    }
    await abortableSleep(1000);
  }
  await boil.stop();
}

async function chillCycle(): Promise<void> {
  valves.open('CHILLER');
  chillerPump.start();
  await abortableSleep(minToMs(params.get('uiChillTime') ?? 20));
  chillerPump.stop();
  valves.close('CHILLER');
}

async function pumpToFermenter(): Promise<void> {
  chillerPump.start();
  await abortableSleep(minToMs(params.get('uiPumpToFermenterTime') ?? 7));
  chillerPump.stop();
}

// ── Step list — mirrors brew.c:2056 BrewSteps[] order and WAIT flags ───────
const steps: BrewStepDef[] = [
  {
    name: 'Waiting',
    wait: false,
    run: async (): Promise<void> => { await sleep(3000); },
  },
  {
    name: 'Raise Crane',
    wait: false,
    run: () => crane.up(),
  },
  {
    name: 'Close D-Valves',
    wait: false,
    run: async (): Promise<void> => { valves.closeAll(); },
  },
  {
    name: 'Close BoilValve',
    wait: true,
    run: () => boilValve.close(),
  },
  {
    name: 'Fill+Heat:Strike',
    wait: false,                                           // ← parallel with Grind
    run: () => hlt.heatAndFill(params.get('fStrikeTemp') ?? 75),
  },
  {
    name: 'Grind Grains',
    wait: false,                                           // ← parallel with Fill+Heat
    run: () => mill.runFor(minToMs(params.get('iGrindTime') ?? 17), () => quitFlag),
  },
  {
    name: 'DrainHLTForMash',
    wait: true,                                            // ← join: HLT heated AND grind done
    run: async (): Promise<void> => {
      valves.open('MASH');
      await sleep(500);
      await hlt.drain(params.get('fStrikeLitres') ?? 21);
      valves.close('MASH');
    },
  },
  {
    name: 'Lower Crane',
    wait: true,
    run: () => crane.incremental(),
  },
  {
    name: 'Fill+Heat:Sparge1',
    wait: true,                                            // start refilling HLT
    run: () => hlt.heatAndFill(params.get('fSpargeTemp') ?? 98),
  },
  {
    name: 'Mash',
    wait: false,                                           // ← parallel with sparge1 fill+heat
    run: () => mashCycle(),
  },
  {
    name: 'MashPumpToBoil',
    wait: true,                                            // join
    run: () => pumpMashToBoilCycle(3 * 60),
  },
  {
    name: 'DrainForSparge1',
    wait: true,
    run: async (): Promise<void> => {
      valves.open('MASH');
      await sleep(500);
      await hlt.drain(params.get('fSpargeLitres') ?? 12);
      valves.close('MASH');
    },
  },
  {
    name: 'Fill+Heat:Sparge2',
    wait: true,
    run: () => hlt.heatAndFill(params.get('fSpargeTemp2') ?? 95),
  },
  {
    name: 'Sparge1',
    wait: false,                                           // ← parallel with sparge2 fill+heat
    run: () => spargeCycle(),
  },
  {
    name: 'Pump to boil1',
    wait: true,
    run: () => pumpMashToBoilCycle(2 * 60),
  },
  {
    name: 'DrainForSparge2',
    wait: true,
    run: async (): Promise<void> => {
      valves.open('MASH');
      await sleep(500);
      await hlt.drain(params.get('fSpargeLitres') ?? 12);
      valves.close('MASH');
    },
  },
  {
    name: 'Sparge2',
    wait: true,
    run: () => spargeCycle(),
  },
  {
    name: 'Pump to boil2',
    wait: true,
    run: () => pumpMashToBoilCycle(2 * 60),
  },
  {
    name: 'Raise Crane',
    wait: true,
    run: () => crane.up(),
  },
  {
    name: 'Fill+Heat:Clean',
    wait: false,                                           // ← parallel with BringToBoil
    run: () => hlt.heatAndFill(params.get('fCleanTemp') ?? 32.34),
  },
  {
    name: 'BringToBoil',
    wait: false,                                           // ← parallel with Fill+Heat:Clean
    run: async (): Promise<void> => {
      await boil.bringToBoil();
      await abortableSleep(minToMs(params.get('uiBringToBoilTime') ?? 17));
    },
  },
  {
    name: 'Pump to boil (final)',
    wait: true,
    run: () => pumpMashToBoilCycle(2 * 60),
  },
  {
    name: 'Boil',
    wait: true,
    run: () => boilCycle(),
  },
  {
    name: 'SettlingBefChill',
    wait: true,
    run: async (): Promise<void> => {
      const mins = params.get('uiSettlingTime') ?? 1;
      if (mins > 0) await abortableSleep(minToMs(mins));
    },
  },
  {
    name: 'Chill',
    wait: true,
    run: () => chillCycle(),
  },
  {
    name: 'Pump Out',
    wait: true,
    run: () => pumpToFermenter(),
  },
  {
    name: 'BREW FINISHED',
    wait: true,
    run: async (): Promise<void> => {
      safeStates();
      mashWater.clear();
    },
  },
];

// ── Brew driver ────────────────────────────────────────────────────────────
async function runBrew(): Promise<void> {
  running = true;
  paused = false;
  quitFlag = false;
  currentStep = 0;
  stepStartSec = secondsElapsed = 0;
  publish();

  /** Promises for steps that have been fired but not yet resolved. */
  const pending: Array<{ idx: number; name: string; promise: Promise<void> }> = [];

  /** Convert a step failure into a brew abort + safe states. */
  const abortOnFailure = (err: Error, where: string): void => {
    if (err.message === 'QUIT') return;             // QUIT is the cooperative path
    log.error(`BREW: aborting — ${where}: ${err.message}`);
    quitFlag = true;
    try { hlt.abortAll('brew aborted'); } catch { /* ignore */ }
    safeStates();
  };

  for (let i = 0; i < steps.length; i++) {
    if (quitFlag) break;

    const step = steps[i]!;

    // WAIT semantics: block until all previously-launched steps complete.
    if (step.wait && pending.length > 0) {
      log.info(`BREW: step ${i} '${step.name}' has WAIT=1, awaiting ${pending.length} pending step(s)`);
      try {
        await Promise.all(pending.map((p) => p.promise));
      } catch (err) {
        abortOnFailure(err as Error, `pending step before '${step.name}'`);
        break;                                       // fail-loud: do not launch the next step
      }
      pending.length = 0;
    }

    if (quitFlag) break;

    currentStep = i;
    stepStartSec = secondsElapsed;
    publish(step.name);
    log.info(`BREW: launching step ${i} '${step.name}' (wait=${step.wait})`);

    const promise = (async (): Promise<void> => {
      try {
        // Test-only: inject a synthetic failure into a specific step.
        if (_testHooks.failStepAt === i) {
          _testHooks.failStepAt = null;
          throw new Error('test-injected step failure');
        }
        await step.run();
        log.info(`BREW: step ${i} '${step.name}' COMPLETE`);
      } catch (err) {
        if ((err as Error).message === 'QUIT') throw err;
        log.error(`BREW: step ${i} '${step.name}' FAILED: ${(err as Error).message}`);
        // Step failure is fatal to the brew. Surface the error to the join
        // and abort immediately so parallel-launched siblings stop too.
        abortOnFailure(err as Error, `step '${step.name}'`);
        throw err;
      }
    })();
    pending.push({ idx: i, name: step.name, promise });
  }

  // Final join — wait for the tail of unfinished work
  try {
    await Promise.all(pending.map((p) => p.promise));
  } catch (err) {
    abortOnFailure(err as Error, 'tail join');
  }

  running = false;
  safeStates();
  publish('Idle');
  log.info('BREW: finished');
}

export function start(): boolean {
  if (running) return false;
  log.info('BREW: starting');
  ensureTick();
  void runBrew().catch((err: Error) => log.error(`Brew runner crashed: ${err.message}`));
  return true;
}

export function pause():  void { paused = true;  publish(); log.info('BREW: paused'); }
export function resume(): void { paused = false; publish(); log.info('BREW: resumed'); }

export function quit(): void {
  quitFlag = true;
  hlt.abortAll('QUIT');
  log.warn('BREW: quit requested');
}

export function gotoStep(idx: number): void {
  if (idx < 0 || idx >= steps.length) return;
  currentStep = idx;
  publish(steps[idx]!.name);
}

function ensureTick(): void {
  if (tickRunner) return;
  tickRunner = setInterval(() => {
    if (running && !paused) {
      secondsElapsed++;
      publish();
    }
  }, 1000);
}

export function getSteps(): Array<{ index: number; name: string; wait: boolean }> {
  return steps.map((s, i) => ({ index: i, name: s.name, wait: s.wait }));
}

export default { start, pause, resume, quit, gotoStep, getSteps, safeStates };
