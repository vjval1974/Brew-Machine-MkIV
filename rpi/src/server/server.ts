import express from 'express';
import path from 'path';
import http from 'http';
import { WebSocketServer, WebSocket } from 'ws';
import log from '../util/logger';
import store from '../state/store';
import buildRouter, { type Controllers } from './routes';
import gpio from '../hal/gpio';
import onewire from '../hal/onewire';
import type { WsClientMessage, WsServerMessage } from '../types';

export async function createServer(c: Controllers, port = 8080): Promise<http.Server> {
  const app = express();
  app.use(express.json());

  // Static web UI (compiled TS lives under web/js/, sources in web/src/)
  app.use(express.static(path.join(__dirname, '..', '..', 'web')));

  app.use('/api', buildRouter(c));

  const httpServer = http.createServer(app);
  const wss = new WebSocketServer({ server: httpServer, path: '/ws' });

  wss.on('connection', (ws) => {
    log.info(`WS client connected; total=${wss.clients.size}`);
    const snapshot: WsServerMessage = { type: 'snapshot', state: store.get() };
    ws.send(JSON.stringify(snapshot));

    ws.on('message', (raw) => {
      try {
        const msg = JSON.parse(raw.toString()) as WsClientMessage;
        if (msg.type === 'cmd') {
          void handleCmd(c, msg).catch((err: Error) => {
            const errMsg: WsServerMessage = { type: 'error', error: err.message };
            ws.send(JSON.stringify(errMsg));
          });
        }
      } catch (err) {
        log.warn(`WS bad message: ${(err as Error).message}`);
      }
    });
  });

  // Push store changes to all connected clients
  store.on('change', (change: { section: string; value: unknown }) => {
    const payload = JSON.stringify({ type: 'patch', section: change.section, value: change.value });
    for (const client of wss.clients) {
      if (client.readyState === WebSocket.OPEN) client.send(payload);
    }
  });

  // Forward log entries
  log.bus.on('log', (entry: { ts: number; level: string; msg: string }) => {
    const payload = JSON.stringify({ type: 'log', entry });
    for (const client of wss.clients) {
      if (client.readyState === WebSocket.OPEN) client.send(payload);
    }
  });

  return new Promise<http.Server>((resolve) => {
    httpServer.listen(port, () => {
      log.info(`HTTP+WS server listening on http://0.0.0.0:${port}`);
      resolve(httpServer);
    });
  });
}

async function handleCmd(c: Controllers, msg: WsClientMessage): Promise<void> {
  const { name } = msg;
  const args = (msg.args ?? {}) as Record<string, unknown>;
  const num = (k: string): number => Number(args[k]);
  const str = (k: string): string => String(args[k]);
  switch (name) {
    case 'valve.toggle':       c.valves.toggle(str('name') as never); return;
    case 'valve.open':         c.valves.open(str('name')   as never); return;
    case 'valve.close':        c.valves.close(str('name')  as never); return;
    case 'mashPump.toggle':    c.mashPump.toggle();    return;
    case 'chillerPump.toggle': c.chillerPump.toggle(); return;
    case 'mill.start':         c.mill.start();   return;
    case 'mill.stop':          c.mill.stop();    return;
    case 'stir.start':         c.stir.start();   return;
    case 'stir.stop':          c.stir.stop();    return;
    case 'crane.up':           void c.crane.up();          return;
    case 'crane.down':         void c.crane.down();        return;
    case 'crane.incremental':  void c.crane.incremental(); return;
    case 'crane.stop':         c.crane.stop();             return;
    case 'hopDropper.drop':    void c.hopDropper.drop();   return;
    case 'hopDropper.start':   c.hopDropper.startManual(); return;
    case 'hopDropper.stop':    c.hopDropper.stopManual();  return;
    case 'boilValve.toggle':   void c.boilValve.toggle();  return;
    case 'boilValve.open':     void c.boilValve.open();    return;
    case 'boilValve.close':    void c.boilValve.close();   return;
    case 'flow.reset':         c.flow.reset();             return;
    case 'hlt.setpoint':       c.hlt.setSetpoint(num('value'));    return;
    case 'hlt.bumpSetpoint':   c.hlt.bumpSetpoint(num('delta'));   return;
    case 'hlt.startHeating':   void c.hlt.startHeating();          return;
    case 'hlt.stopHeating':    c.hlt.stopHeating();                return;
    case 'boil.setDuty':       await c.boil.setDuty(num('value'));   return;
    case 'boil.bumpDuty':      await c.boil.bumpDuty(num('delta'));  return;
    case 'boil.start':         await c.boil.start(); return;
    case 'boil.stop':          await c.boil.stop();  return;
    case 'brew.start':         c.brew.start();  return;
    case 'brew.pause':         c.brew.pause();  return;
    case 'brew.resume':        c.brew.resume(); return;
    case 'brew.quit':          c.brew.quit();   return;
    case 'brew.gotoStep':      c.brew.gotoStep(num('index')); return;
    case 'params.set':         c.parameters.set(str('key') as never, args.value as never); return;
    case 'params.setMany':     c.parameters.setMany((args.values ?? {}) as never);          return;
    case 'mock.setInput':      gpio._mockSetInput(str('name'), num('value')); return;
    case 'mock.setTemp':       onewire._mockSetTemp(str('name'), num('value')); return;
    case 'mock.injectPulses':  c.flow._injectPulses(Number(args.count) || 1);   return;
    default:
      throw new Error(`Unknown command: ${name}`);
  }
}
