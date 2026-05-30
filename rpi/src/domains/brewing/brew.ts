// Brew state machine — port of brew.c with the original concurrent
// step-launch model preserved.
//
// Each step has a `wait` flag (mirrors `ucWait` in the C `BrewSteps[]` array):
//   wait=false → fire `run()` and immediately advance to the next step.
//   wait=true  → first await ALL previously launched steps to complete, then
//                fire `run()`.
//
// Where the original was a static array, this version loads its step list
// from the active Recipe at start() time (see ../brewing/recipes.ts). Each
// step instance references a `kind` in stepKinds.ts; the kind's `run`
// function does the actual work. The UI can add/remove/reorder steps and
// edit per-step params without touching any code.

import log from '../../platform/util/logger';
import store from '../../platform/store/store';

import * as valves      from '../hydraulics/valves';
import * as mashPump    from '../hydraulics/mashPump';
import * as chillerPump from '../hydraulics/chillerPump';
import * as mill        from '../motion/mill';
import * as stir        from '../motion/stir';
import * as crane       from '../motion/crane';
import * as hlt         from '../hlt/hlt';
import * as boil        from '../boil/boil';
import * as boilValve   from '../hydraulics/boilValve';

import recipes from './recipes';
import { getKind } from './stepKinds';
import type { StepInstance } from '../../types';

// ── Run-state ──────────────────────────────────────────────────────────────
let running    = false;
let paused     = false;
let quitFlag   = false;
let currentStep    = 0;
let stepStartSec   = 0;
let secondsElapsed = 0;
let activeSteps: StepInstance[] = [];
let activeRecipeName = 'Idle';
let tickRunner: NodeJS.Timeout | null = null;

// Test-only hooks. Production code never sets these; the safety regression
// test in scripts/test-safety.ts uses them to verify the failure-abort path
// of the brew engine. Read by `runBrew` at the top of each step launch.
export const _testHooks: { failStepAt: number | null } = {
  failStepAt: null,
};

function stepDisplayName(idx: number): string {
  const s = activeSteps[idx];
  if (!s) return 'Idle';
  try { return getKind(s.kind).meta.displayName; }
  catch { return s.kind; }
}

function publish(stepName?: string): void {
  const r = !running ? 'idle' : paused ? 'paused' : 'running';
  store.set('brew', {
    running:        r,
    step:           currentStep,
    stepName:       stepName ?? stepDisplayName(currentStep),
    secondsElapsed,
    stepElapsed:    secondsElapsed - stepStartSec,
    maxSteps:       activeSteps.length,
  }, 'brewing');
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

// ── Brew driver ────────────────────────────────────────────────────────────
async function runBrew(): Promise<void> {
  // Snapshot the active recipe so a UI edit mid-brew can't mutate our copy.
  const active = recipes.getActive();
  if (!active) {
    log.error('BREW: no active recipe — nothing to run');
    return;
  }
  activeSteps = active.steps.filter((s) => s.enabled).map((s) => ({ ...s }));
  activeRecipeName = active.name;
  log.info(`BREW: loaded recipe '${active.name}' (${activeSteps.length} enabled steps)`);

  running = true;
  paused = false;
  quitFlag = false;
  currentStep = 0;
  stepStartSec = secondsElapsed = 0;
  publish();

  /** Convert a step failure into a brew abort + safe states. */
  const abortOnFailure = (err: Error, where: string): void => {
    if (err.message === 'QUIT') return;
    log.error(`BREW: aborting — ${where}: ${err.message}`);
    quitFlag = true;
    try { hlt.abortAll('brew aborted'); } catch { /* ignore */ }
    safeStates();
  };

  const pending: Array<{ idx: number; name: string; promise: Promise<void> }> = [];

  for (let i = 0; i < activeSteps.length; i++) {
    if (quitFlag) break;
    const step = activeSteps[i]!;
    const kind = (() => {
      try { return getKind(step.kind); }
      catch (err) {
        log.error(`BREW: step ${i} references unknown kind '${step.kind}' — skipping`);
        return null;
      }
    })();
    if (!kind) continue;

    const displayName = kind.meta.displayName;

    if (step.wait && pending.length > 0) {
      log.info(`BREW: step ${i} '${displayName}' has WAIT=1, awaiting ${pending.length} pending step(s)`);
      try {
        await Promise.all(pending.map((p) => p.promise));
      } catch (err) {
        abortOnFailure(err as Error, `pending step before '${displayName}'`);
        break;
      }
      pending.length = 0;
    }

    if (quitFlag) break;

    currentStep = i;
    stepStartSec = secondsElapsed;
    publish(displayName);
    log.info(`BREW: launching step ${i} '${displayName}' (wait=${step.wait}, params=${JSON.stringify(step.params)})`);

    const promise = (async (): Promise<void> => {
      try {
        if (_testHooks.failStepAt === i) {
          _testHooks.failStepAt = null;
          throw new Error('test-injected step failure');
        }
        await kind.run(step.params, () => quitFlag);
        log.info(`BREW: step ${i} '${displayName}' COMPLETE`);
      } catch (err) {
        if ((err as Error).message === 'QUIT') throw err;
        log.error(`BREW: step ${i} '${displayName}' FAILED: ${(err as Error).message}`);
        abortOnFailure(err as Error, `step '${displayName}'`);
        throw err;
      }
    })();
    pending.push({ idx: i, name: displayName, promise });
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
  if (idx < 0 || idx >= activeSteps.length) return;
  currentStep = idx;
  publish();
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

/**
 * Step preview for the BREW tab in the UI. Returns the active recipe's
 * step list with display names so the front-end can render the same view
 * regardless of which recipe is active.
 */
export function getSteps(): Array<{ index: number; name: string; wait: boolean; enabled: boolean; kind: string }> {
  const active = recipes.getActive();
  if (!active) return [];
  return active.steps.map((s, i) => ({
    index:   i,
    name:    safeDisplayName(s),
    wait:    s.wait,
    enabled: s.enabled,
    kind:    s.kind,
  }));
}

function safeDisplayName(s: StepInstance): string {
  try { return getKind(s.kind).meta.displayName; }
  catch { return s.kind; }
}

export function getActiveRecipeName(): string {
  return activeRecipeName;
}

export default { start, pause, resume, quit, gotoStep, getSteps, safeStates, getActiveRecipeName, _testHooks };
