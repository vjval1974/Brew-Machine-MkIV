/**
 * Safety regression test — runs the brew engine in mock mode, injects a
 * mid-brew step failure, and asserts:
 *
 *   1. The brew aborts (running flag → false).
 *   2. `safeStates()` ran — every energetic output is LOW, every pump
 *      stopped, every valve closed.
 *   3. The HLT pending command queue was drained (`abortAll` called).
 *
 * This is the test that, had it existed earlier, would have caught the
 * brew.ts:357-362 silent-rejection-swallow bug.
 *
 * Run:   MOCK_HARDWARE=1 npx tsx scripts/test-safety.ts
 *        (also exits non-zero on failure — wire into CI.)
 */

process.env.MOCK_HARDWARE = '1';

// During the brew abort and the supersede tests we rely on rejecting
// promises whose callers may not have attached a .catch yet. Don't let
// Node escalate those to a process crash mid-test.
process.on('unhandledRejection', (reason: unknown) => {
  console.error('  ! unhandledRejection:', reason instanceof Error ? reason.message : String(reason));
});

import gpio    from '../src/hal/gpio';
import i2c     from '../src/hal/i2c';
import params  from '../src/parameters/parameters';
import * as valves      from '../src/controllers/valves';
import * as mashPump    from '../src/controllers/mashPump';
import * as chillerPump from '../src/controllers/chillerPump';
import * as mill        from '../src/controllers/mill';
import * as stir        from '../src/controllers/stir';
import * as crane       from '../src/controllers/crane';
import * as hopDropper  from '../src/controllers/hopDropper';
import * as hlt         from '../src/controllers/hlt';
import * as boil        from '../src/controllers/boil';
import * as boilValve   from '../src/controllers/boilValve';
import * as flow        from '../src/controllers/flow';
import * as brew        from '../src/controllers/brew';
import store from '../src/state/store';

const sleep = (ms: number): Promise<void> => new Promise((r) => setTimeout(r, ms));

let failed = 0;
function assert(cond: boolean, msg: string): void {
  if (!cond) { failed++; console.error(`  ✗ ${msg}`); }
  else       {            console.log( `  ✓ ${msg}`); }
}

async function setup(): Promise<void> {
  params.load();
  await i2c.open();
  valves.init();
  mashPump.init();
  chillerPump.init();
  mill.init();
  stir.init();
  crane.init();
  hopDropper.init();
  hlt.init();
  await boil.init();
  boilValve.init();
  flow.init();
  flow.start();
  crane.start();
  boilValve.start();
  hlt.start();
}

async function teardown(): Promise<void> {
  try { gpio.cleanup(); } catch { /* ignore */ }
  try { await i2c.close(); } catch { /* ignore */ }
}

async function testFailureInjection(): Promise<void> {
  console.log('TEST: mid-brew failure aborts brew & calls safeStates');

  // Park controllers in deliberately non-safe states so we can verify
  // safeStates() drives them back.
  valves.open('HLT');
  valves.open('MASH');
  valves.open('INLET');
  valves.open('CHILLER');
  mashPump.start();
  chillerPump.start();
  mill.start();
  await sleep(20);

  // In mock mode the limit switches never fire, so the steps before our
  // injection target (Raise Crane @ idx 1, Close BoilValve @ idx 3) would
  // block forever. Drive the mock inputs to "limit hit" so those steps
  // resolve naturally through the real controller code path.
  gpio._mockSetInput('CRANE_UPPER_LIMIT', 0);   // active-low: 0 = limit hit
  gpio._mockSetInput('CRANE_LOWER_LIMIT', 0);
  gpio._mockSetInput('BOIL_VALVE_OPENED', 0);
  gpio._mockSetInput('BOIL_VALVE_CLOSED', 0);

  // Inject failure at step 4 (Fill+Heat:Strike). This is the exact shape
  // of the bug that brew.ts used to silently swallow: a step's promise
  // rejects while other parallel steps are still in flight.
  brew._testHooks.failStepAt = 4;

  brew.start();
  // Step 0 is a 3 s 'Waiting' sleep; allow up to 8 s for engine to reach +
  // abort + run safeStates.
  for (let i = 0; i < 80; i++) {
    if (store.state.brew.running === 'idle') break;
    await sleep(100);
  }

  // ── Assertions ───────────────────────────────────────────────────────
  assert(store.state.brew.running === 'idle',    'brew is no longer running');
  assert(store.state.valves.HLT     === 'closed', 'HLT valve closed by safeStates');
  assert(store.state.valves.MASH    === 'closed', 'MASH valve closed by safeStates');
  assert(store.state.valves.INLET   === 'closed', 'INLET valve closed by safeStates');
  assert(store.state.valves.CHILLER === 'closed', 'CHILLER valve closed by safeStates');
  assert(store.state.pumps.mash     === 'stopped', 'mash pump stopped by safeStates');
  assert(store.state.pumps.chiller  === 'stopped', 'chiller pump stopped by safeStates');
  assert(store.state.mill           === 'stopped', 'mill stopped by safeStates');
  assert(store.state.boil.duty      === 0,         'boil duty driven to 0');
  assert(store.state.hlt.heating    === false,     'HLT heating driven off');
  assert(store.state.hlt.cmd        === 'idle',    'HLT command driven to idle');
}

async function testShutdownDrivesOutputsLow(): Promise<void> {
  console.log('TEST: brew.safeStates() drives outputs LOW immediately');

  valves.open('HLT'); valves.open('INLET');
  mashPump.start();  chillerPump.start();
  await boil.setDuty(80);

  brew.safeStates();

  assert(store.state.valves.HLT    === 'closed', 'HLT closed synchronously');
  assert(store.state.valves.INLET  === 'closed', 'INLET closed synchronously');
  assert(store.state.pumps.mash    === 'stopped','mash pump stopped synchronously');
  assert(store.state.pumps.chiller === 'stopped','chiller pump stopped synchronously');
}

async function testHltPendingBound(): Promise<void> {
  console.log('TEST: hlt commands of the same type supersede each other');

  // Two heat-and-fills back-to-back; the first must reject with "superseded".
  const first = hlt.heatAndFill(60);
  let firstError: Error | null = null;
  first.catch((err: Error) => { firstError = err; });

  const second = hlt.heatAndFill(70);
  // give the rejection time to propagate
  await sleep(10);

  assert(firstError !== null,                            'first heatAndFill was rejected');
  assert(firstError?.message === 'superseded',           'rejection reason was "superseded"');
  assert(typeof (second as Promise<void>).then === 'function', 'second heatAndFill returned a Promise');

  hlt.abortAll('test cleanup');
}

async function main(): Promise<void> {
  await setup();
  try {
    await testFailureInjection();
    await testShutdownDrivesOutputsLow();
    await testHltPendingBound();
  } finally {
    await teardown();
  }
  if (failed > 0) {
    console.error(`\nFAILED: ${failed} assertion(s) failed`);
    process.exit(1);
  }
  console.log('\nAll safety regression tests passed.');
  process.exit(0);
}

main().catch((err: Error) => {
  console.error(`Test runner crashed: ${err.stack ?? err.message}`);
  process.exit(2);
});
