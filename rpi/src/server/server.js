'use strict';

const express = require('express');
const path = require('path');
const http = require('http');
const { WebSocketServer } = require('ws');
const log = require('../util/logger');
const store = require('../state/store');
const buildRouter = require('./routes');

function createServer(controllers, port = 8080) {
  const app = express();
  app.use(express.json());

  // Web UI
  app.use(express.static(path.join(__dirname, '..', '..', 'web')));

  // REST API
  app.use('/api', buildRouter(controllers));

  const httpServer = http.createServer(app);
  const wss = new WebSocketServer({ server: httpServer, path: '/ws' });

  wss.on('connection', (ws) => {
    log.info(`WS client connected; total=${wss.clients.size}`);
    ws.send(JSON.stringify({ type: 'snapshot', state: store.get() }));

    ws.on('message', (raw) => {
      try {
        const msg = JSON.parse(raw.toString());
        if (msg.type === 'cmd') {
          handleCmd(controllers, msg).catch((err) => {
            ws.send(JSON.stringify({ type: 'error', error: err.message }));
          });
        }
      } catch (err) {
        log.warn(`WS bad message: ${err.message}`);
      }
    });
  });

  // Push store changes to all connected clients
  store.on('change', ({ section, value }) => {
    const payload = JSON.stringify({ type: 'patch', section, value });
    for (const c of wss.clients) {
      if (c.readyState === 1) c.send(payload);
    }
  });

  // Also forward log lines to clients
  log.bus.on('log', (entry) => {
    const payload = JSON.stringify({ type: 'log', entry });
    for (const c of wss.clients) if (c.readyState === 1) c.send(payload);
  });

  return new Promise((resolve) => {
    httpServer.listen(port, () => {
      log.info(`HTTP+WS server listening on http://0.0.0.0:${port}`);
      resolve(httpServer);
    });
  });
}

// Handle inbound WebSocket commands. Mirrors the REST endpoints but allows
// lower-latency control from the kiosk UI.
async function handleCmd(c, msg) {
  const { name, args = {} } = msg;
  switch (name) {
    case 'valve.toggle':      return c.valves.toggle(args.name);
    case 'valve.open':        return c.valves.open(args.name);
    case 'valve.close':       return c.valves.close(args.name);
    case 'mashPump.toggle':   return c.mashPump.toggle();
    case 'chillerPump.toggle':return c.chillerPump.toggle();
    case 'mill.start':        return c.mill.start();
    case 'mill.stop':         return c.mill.stop();
    case 'stir.start':        return c.stir.start();
    case 'stir.stop':         return c.stir.stop();
    case 'crane.up':          return c.crane.up();
    case 'crane.down':        return c.crane.down();
    case 'crane.stop':        return c.crane.stop();
    case 'hopDropper.drop':   return c.hopDropper.drop();
    case 'hopDropper.start':  return c.hopDropper.startManual();
    case 'hopDropper.stop':   return c.hopDropper.stopManual();
    case 'boilValve.toggle':  return c.boilValve.toggle();
    case 'flow.reset':        return c.flow.reset();
    case 'hlt.setpoint':      return c.hlt.setSetpoint(Number(args.value));
    case 'hlt.bumpSetpoint':  return c.hlt.bumpSetpoint(Number(args.delta));
    case 'hlt.startHeating':  return c.hlt.startHeating();
    case 'hlt.stopHeating':   return c.hlt.stopHeating();
    case 'boil.setDuty':      return c.boil.setDuty(Number(args.value));
    case 'boil.bumpDuty':     return c.boil.bumpDuty(Number(args.delta));
    case 'boil.start':        return c.boil.start();
    case 'boil.stop':         return c.boil.stop();
    case 'brew.start':        return c.brew.start();
    case 'brew.pause':        return c.brew.pause();
    case 'brew.resume':       return c.brew.resume();
    case 'brew.quit':         return c.brew.quit();
    case 'brew.gotoStep':     return c.brew.gotoStep(Number(args.index));
    case 'params.set':        return c.parameters.set(args.key, args.value);
    case 'params.setMany':    return c.parameters.setMany(args.values || {});
    case 'mock.setInput':     return require('../hal/gpio')._mockSetInput(args.name, args.value);
    case 'mock.setTemp':      return require('../hal/onewire')._mockSetTemp(args.name, Number(args.value));
    case 'mock.injectPulses': return c.flow._injectPulses(Number(args.count) || 1);
    default:
      throw new Error(`Unknown command: ${name}`);
  }
}

module.exports = { createServer };
