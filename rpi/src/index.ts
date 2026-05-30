// Entry point — wires the HAL, controllers, and HTTP/WS server together.
// Direct port of main.c's `main()` + `prvSetupHardware()`.

import log from './platform/util/logger';
import gpio from './platform/hal/gpio';
import i2c from './platform/hal/i2c';
import watchdog from './platform/hal/watchdog';
import pinmap from './config/pinmap';
import store from './platform/store/store';

import params from './platform/parameters/parameters';

import * as valves      from './domains/hydraulics/valves';
import * as mashPump    from './domains/hydraulics/mashPump';
import * as chillerPump from './domains/hydraulics/chillerPump';
import * as mill        from './domains/motion/mill';
import * as stir        from './domains/motion/stir';
import * as crane       from './domains/motion/crane';
import * as hopDropper  from './domains/motion/hopDropper';
import * as hlt         from './domains/hlt/hlt';
import * as boil        from './domains/boil/boil';
import * as boilValve   from './domains/hydraulics/boilValve';
import * as flow        from './domains/hydraulics/flow';
import * as tempSensors from './domains/sensing/tempSensors';
import * as brew        from './domains/brewing/brew';

import { createServer } from './server/server';
import type { Controllers } from './server/routes';

/**
 * Bounded contexts and their canonical store-section owners. Centralised
 * here so the architecture is in one place and not spread across init
 * functions. The registry is consulted whenever a `store.patch(section,
 * patch, writer)` is called with an explicit `writer` — non-matching
 * writers log a one-shot warning. Today this is documentation; tomorrow
 * it's a code-review checklist that a future ESLint rule (or test) can
 * promote to hard enforcement.
 */
function declareDomainOwners(): void {
  store.declareOwner('brew',       'brewing');
  store.declareOwner('hlt',        'hlt');
  store.declareOwner('boil',       'boil');
  store.declareOwner('valves',     'hydraulics');
  store.declareOwner('pumps',      'hydraulics');
  store.declareOwner('boilValve',  'hydraulics');
  store.declareOwner('flow',       'hydraulics');
  store.declareOwner('mashWater',  'hydraulics');
  store.declareOwner('crane',      'motion');
  store.declareOwner('stir',       'motion');
  store.declareOwner('mill',       'motion');
  store.declareOwner('hopDropper', 'motion');
  store.declareOwner('temps',      'sensing');
  store.declareOwner('parameters', 'platform');
}

/**
 * Pins whose failure to be mapped (`bcm === null`) on real hardware would
 * be unsafe: heater SSRs, the inlet valve (overflow), HLT level sense (the
 * cutoff for the heater), and the boil PWM channel. Refuse to boot if any
 * of these are unmapped and MOCK_HARDWARE is not set.
 */
function assertSafetyCriticalPinsMapped(): void {
  gpio.assertCriticalPinsMapped([
    { kind: 'output', name: 'HLT_SSR',        bcm: pinmap.outputs.HLT_SSR.bcm },
    { kind: 'output', name: 'INLET_VALVE',    bcm: pinmap.outputs.INLET_VALVE.bcm },
    { kind: 'input',  name: 'HLT_LEVEL_HIGH', bcm: pinmap.inputs.HLT_LEVEL_HIGH.bcm },
    { kind: 'input',  name: 'HLT_LEVEL_MID',  bcm: pinmap.inputs.HLT_LEVEL_MID.bcm },
  ]);
  // The boil SSR PWM pin's BCM is informational only — its actual claim
  // happens via sysfs PWM, but warn if the chip/channel pair smells wrong.
  if (!pinmap.pwm.BOIL_SSR || pinmap.pwm.BOIL_SSR.bcm === null) {
    if (!gpio.isMock()) {
      throw new Error('Refusing to boot: pinmap.pwm.BOIL_SSR is not configured.');
    }
  }
}

async function main(): Promise<void> {
  log.info('====================================================');
  log.info('  Brew Machine MkIV — Raspberry Pi 5 port');
  log.info(`  MOCK_HARDWARE=${process.env.MOCK_HARDWARE === '1' ? 'YES' : 'no'}`);
  log.info('====================================================');

  // Safety: refuse to boot with unmapped critical pins on real hardware.
  assertSafetyCriticalPinsMapped();
  declareDomainOwners();

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

  valves.setHooks('HLT', {
    onOpen:  () => flow.setMeasuring(true),
    onClose: () => flow.setMeasuring(false),
  });

  tempSensors.start();
  flow.start();
  crane.start();
  boilValve.start();
  hlt.start();

  // Start kicking the watchdogs only after every controller is up. If init
  // throws above we want the watchdog to fire, not be quietly held off.
  await watchdog.start();

  const port = Number(process.env.PORT) || 8080;
  const controllers: Controllers = {
    valves, mashPump, chillerPump, mill, stir, crane, hopDropper,
    hlt, boil, boilValve, flow,
    brew, parameters: params,
  };
  await createServer(controllers, port);

  /**
   * Shutdown protocol — order matters:
   *   1. Drive every energetic output LOW synchronously, BEFORE we await
   *      anything. If the runtime is going to be killed in ~500 ms the
   *      heater must already be off.
   *   2. Tell the brew engine to abort so pending step promises reject.
   *   3. Stop the watchdog kicker — but issue the kernel "magic close"
   *      so the SoC watchdog doesn't reboot the Pi on clean shutdown.
   *   4. Release GPIO + I2C handles last.
   *
   * On `uncaughtException` we follow the same path; the brew engine sees
   * the abort via `hlt.abortAll('shutdown')` and won't launch more steps.
   */
  let shuttingDown = false;
  const shutdown = (sig: string): void => {
    if (shuttingDown) return;
    shuttingDown = true;
    log.warn(`Received ${sig}, shutting down`);

    // 1. Energetic outputs LOW first, synchronously.
    try { brew.safeStates(); } catch (err) { log.error(`safeStates: ${(err as Error).message}`); }
    try { hlt.abortAll('shutdown'); } catch (err) { log.error(`hlt.abortAll: ${(err as Error).message}`); }

    // 2. Async stop of components that own async resources. Don't await —
    //    we don't trust them to finish promptly during a fault path.
    void boil.stop().catch(() => { /* already best-effort */ });

    // 3. Watchdog: clean stop so the kernel doesn't reboot us.
    void watchdog.stop();

    // 4. Release HAL handles. GPIO cleanup also drives outputs low again
    //    as a belt-and-braces measure.
    try { gpio.cleanup(); } catch (err) { log.error(`gpio.cleanup: ${(err as Error).message}`); }
    try { void i2c.close(); } catch (err) { log.error(`i2c.close: ${(err as Error).message}`); }

    setTimeout(() => process.exit(0), 200);
  };
  process.on('SIGINT',  () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));
  process.on('uncaughtException', (err: Error) => {
    log.error(`uncaughtException: ${err.stack ?? err.message}`);
    shutdown('uncaughtException');
  });
  process.on('unhandledRejection', (reason: unknown) => {
    log.error(`unhandledRejection: ${reason instanceof Error ? reason.stack : String(reason)}`);
    shutdown('unhandledRejection');
  });

  log.info('Boot complete. Open http://localhost:' + port);
}

main().catch((err: Error) => {
  log.error(`Fatal: ${err.stack ?? err.message}`);
  process.exit(1);
});
