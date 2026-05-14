// Entry point — wires the HAL, controllers, and HTTP/WS server together.
// Direct port of main.c's `main()` + `prvSetupHardware()`.

import log from './util/logger';
import gpio from './hal/gpio';
import i2c from './hal/i2c';

import params from './parameters/parameters';

import * as valves      from './controllers/valves';
import * as mashPump    from './controllers/mashPump';
import * as chillerPump from './controllers/chillerPump';
import * as mill        from './controllers/mill';
import * as stir        from './controllers/stir';
import * as crane       from './controllers/crane';
import * as hopDropper  from './controllers/hopDropper';
import * as hlt         from './controllers/hlt';
import * as boil        from './controllers/boil';
import * as boilValve   from './controllers/boilValve';
import * as flow        from './controllers/flow';
import * as tempSensors from './controllers/tempSensors';
import * as brew        from './controllers/brew';

import { createServer } from './server/server';
import type { Controllers } from './server/routes';

async function main(): Promise<void> {
  log.info('====================================================');
  log.info('  Brew Machine MkIV — Raspberry Pi 5 port');
  log.info(`  MOCK_HARDWARE=${process.env.MOCK_HARDWARE === '1' ? 'YES' : 'no'}`);
  log.info('====================================================');

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

  const port = Number(process.env.PORT) || 8080;
  const controllers: Controllers = {
    valves, mashPump, chillerPump, mill, stir, crane, hopDropper,
    hlt, boil, boilValve, flow,
    brew, parameters: params,
  };
  await createServer(controllers, port);

  const shutdown = (sig: string): void => {
    log.warn(`Received ${sig}, shutting down`);
    try { brew.safeStates(); } catch { /* ignore */ }
    try { void boil.stop(); } catch { /* ignore */ }
    try { gpio.cleanup(); } catch { /* ignore */ }
    try { void i2c.close(); } catch { /* ignore */ }
    setTimeout(() => process.exit(0), 500);
  };
  process.on('SIGINT',  () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));
  process.on('uncaughtException', (err: Error) => {
    log.error(`uncaughtException: ${err.stack ?? err.message}`);
    shutdown('uncaughtException');
  });

  log.info('Boot complete. Open http://localhost:' + port);
}

main().catch((err: Error) => {
  log.error(`Fatal: ${err.stack ?? err.message}`);
  process.exit(1);
});
