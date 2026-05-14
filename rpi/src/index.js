'use strict';

// Entry point — wires the HAL, controllers, and HTTP/WS server together.
// Direct port of main.c's `main()` + `prvSetupHardware()`.

const log    = require('./util/logger');
const gpio   = require('./hal/gpio');
const i2c    = require('./hal/i2c');

const params = require('./parameters/parameters');

const valves      = require('./controllers/valves');
const mashPump    = require('./controllers/mashPump');
const chillerPump = require('./controllers/chillerPump');
const mill        = require('./controllers/mill');
const stir        = require('./controllers/stir');
const crane       = require('./controllers/crane');
const hopDropper  = require('./controllers/hopDropper');
const hlt         = require('./controllers/hlt');
const boil        = require('./controllers/boil');
const boilValve   = require('./controllers/boilValve');
const flow        = require('./controllers/flow');
const mashWater   = require('./controllers/mashWater');
const tempSensors = require('./controllers/tempSensors');
const brew        = require('./controllers/brew');

const { createServer } = require('./server/server');

async function main() {
  log.info('====================================================');
  log.info('  Brew Machine MkIV — Raspberry Pi port');
  log.info(`  MOCK_HARDWARE=${process.env.MOCK_HARDWARE === '1' ? 'YES' : 'no'}`);
  log.info('====================================================');

  // 1. Parameters
  params.load();

  // 2. Open buses
  await i2c.open();

  // 3. Init controllers (in the same order as main.c)
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

  // 4. Wire HLT valve open/close → flow measurement (same as
  // vOnOpenHlt / vOnCloseHlt in valves.c)
  valves.setHooks('HLT', {
    onOpen:  () => flow.setMeasuring(true),
    onClose: () => flow.setMeasuring(false),
  });

  // 5. Background tasks
  tempSensors.start();
  flow.start();
  crane.start();
  boilValve.start();
  hlt.start();

  // 6. HTTP+WS server
  const port = Number(process.env.PORT) || 8080;
  const controllers = {
    valves, mashPump, chillerPump, mill, stir, crane, hopDropper,
    hlt, boil, boilValve, flow, mashWater, tempSensors,
    brew, parameters: params,
  };
  await createServer(controllers, port);

  // 7. Graceful shutdown — drive every output low and release the GPIO chip.
  const shutdown = (sig) => {
    log.warn(`Received ${sig}, shutting down`);
    try { brew.safeStates(); } catch (_) {}
    try { boil.stop(); } catch (_) {}
    try { gpio.cleanup(); } catch (_) {}
    try { i2c.close(); } catch (_) {}
    process.exit(0);
  };
  process.on('SIGINT',  () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));
  process.on('uncaughtException', (err) => {
    log.error(`uncaughtException: ${err.stack || err.message}`);
    shutdown('uncaughtException');
  });

  log.info('Boot complete. Open http://localhost:' + port);
}

main().catch((err) => {
  log.error(`Fatal: ${err.stack || err.message}`);
  process.exit(1);
});
