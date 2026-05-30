import express from 'express';
import type { Request, Response, Router } from 'express';
import store from '../platform/store/store';

import type * as valvesT      from '../domains/hydraulics/valves';
import type * as mashPumpT    from '../domains/hydraulics/mashPump';
import type * as chillerPumpT from '../domains/hydraulics/chillerPump';
import type * as millT        from '../domains/motion/mill';
import type * as stirT        from '../domains/motion/stir';
import type * as craneT       from '../domains/motion/crane';
import type * as hopDropperT  from '../domains/motion/hopDropper';
import type * as hltT         from '../domains/hlt/hlt';
import type * as boilT        from '../domains/boil/boil';
import type * as boilValveT   from '../domains/hydraulics/boilValve';
import type * as flowT        from '../domains/hydraulics/flow';
import type * as brewT        from '../domains/brewing/brew';
import type recipesT          from '../domains/brewing/recipes';
import type paramsT           from '../platform/parameters/parameters';
import type { ValveName, StepKindId } from '../types';
import { listKinds }          from '../domains/brewing/stepKinds';

export interface Controllers {
  valves:      typeof valvesT;
  mashPump:    typeof mashPumpT;
  chillerPump: typeof chillerPumpT;
  mill:        typeof millT;
  stir:        typeof stirT;
  crane:       typeof craneT;
  hopDropper:  typeof hopDropperT;
  hlt:         typeof hltT;
  boil:        typeof boilT;
  boilValve:   typeof boilValveT;
  flow:        typeof flowT;
  brew:        typeof brewT;
  parameters:  typeof paramsT;
  recipes:     typeof recipesT;
}

export default function buildRouter(c: Controllers): Router {
  const r = express.Router();

  r.get('/state', (_req: Request, res: Response) => res.json(store.get()));
  r.get('/parameters', (_req, res) => res.json(c.parameters.get()));
  r.put('/parameters/:key', (req, res) => {
    const body = req.body as { value: unknown };
    c.parameters.set(req.params.key as never, body.value as never);
    res.json({ ok: true, value: body.value });
  });
  r.put('/parameters', (req, res) => {
    c.parameters.setMany(req.body as Partial<Parameters>);
    res.json({ ok: true });
  });

  r.get('/brew/steps', (_req, res) => res.json(c.brew.getSteps()));
  r.post('/brew/start',  (_req, res) => res.json({ ok: c.brew.start() }));
  r.post('/brew/pause',  (_req, res) => { c.brew.pause();  res.json({ ok: true }); });
  r.post('/brew/resume', (_req, res) => { c.brew.resume(); res.json({ ok: true }); });
  r.post('/brew/quit',   (_req, res) => { c.brew.quit();   res.json({ ok: true }); });
  r.post('/brew/goto/:i',(req,  res) => { c.brew.gotoStep(Number(req.params.i)); res.json({ ok: true }); });

  r.post('/valve/:name/:action', (req, res) => {
    const name = req.params.name as ValveName;
    const action = req.params.action;
    if (action === 'open')       c.valves.open(name);
    else if (action === 'close') c.valves.close(name);
    else                         c.valves.toggle(name);
    res.json({ ok: true, state: c.valves.state(name) });
  });

  r.post('/pump/mash/toggle',    (_req, res) => { c.mashPump.toggle();    res.json({ state: c.mashPump.state() }); });
  r.post('/pump/chiller/toggle', (_req, res) => { c.chillerPump.toggle(); res.json({ state: c.chillerPump.state() }); });

  r.post('/mill/:action', (req, res) => {
    const a = req.params.action;
    if (a === 'start') c.mill.start();
    if (a === 'stop')  c.mill.stop();
    res.json({ state: c.mill.state() });
  });
  r.post('/stir/:action', (req, res) => {
    const a = req.params.action;
    if (a === 'start') c.stir.start();
    if (a === 'stop')  c.stir.stop();
    res.json({ state: c.stir.state() });
  });
  r.post('/crane/:action', async (req, res) => {
    const a = req.params.action;
    if (a === 'up')          { void c.crane.up(); }
    if (a === 'down')        { void c.crane.down(); }
    if (a === 'incremental') { void c.crane.incremental(); }
    if (a === 'stop')        c.crane.stop();
    res.json({ state: c.crane.state() });
  });
  r.post('/boilValve/:action', async (req, res) => {
    const a = req.params.action;
    if (a === 'open')   { void c.boilValve.open(); }
    if (a === 'close')  { void c.boilValve.close(); }
    if (a === 'toggle') { void c.boilValve.toggle(); }
    if (a === 'stop')   c.boilValve.stop();
    res.json({ state: c.boilValve.state() });
  });

  r.post('/hopDropper/drop',  (_req, res) => { void c.hopDropper.drop(); res.json({ ok: true }); });
  r.post('/hopDropper/start', (_req, res) => { c.hopDropper.startManual(); res.json({ ok: true }); });
  r.post('/hopDropper/stop',  (_req, res) => { c.hopDropper.stopManual(); res.json({ ok: true }); });

  r.post('/hlt/setpoint', (req, res) => {
    const body = req.body as { value: number };
    c.hlt.setSetpoint(Number(body.value));
    res.json({ ok: true });
  });
  r.post('/hlt/start', (_req, res) => { void c.hlt.startHeating(); res.json({ ok: true }); });
  r.post('/hlt/stop',  (_req, res) => { c.hlt.stopHeating();  res.json({ ok: true }); });

  r.post('/boil/duty', async (req, res) => {
    const body = req.body as { value: number };
    await c.boil.setDuty(Number(body.value));
    res.json({ ok: true });
  });
  r.post('/boil/start', async (_req, res) => { await c.boil.start(); res.json({ ok: true }); });
  r.post('/boil/stop',  async (_req, res) => { await c.boil.stop();  res.json({ ok: true }); });

  r.post('/flow/reset', (_req, res) => { c.flow.reset(); res.json({ ok: true }); });

  // ── Recipes ───────────────────────────────────────────────────────────────
  r.get('/recipes',           (_req, res) => res.json(c.recipes.get()));
  r.get('/recipes/kinds',     (_req, res) => res.json(listKinds()));
  r.post('/recipes',          (req, res) => {
    const body = req.body as { name: string; description?: string };
    res.json(c.recipes.create(body.name, body.description));
  });
  r.post('/recipes/:id/duplicate', (req, res) => {
    const body = req.body as { name?: string };
    res.json(c.recipes.duplicate(req.params.id, body.name));
  });
  r.post('/recipes/:id/activate',  (req, res) => { c.recipes.activate(req.params.id); res.json({ ok: true }); });
  r.delete('/recipes/:id',         (req, res) => { c.recipes.remove(req.params.id);   res.json({ ok: true }); });
  r.put('/recipes/:id',            (req, res) => {
    const body = req.body as { name: string; description?: string };
    c.recipes.rename(req.params.id, body.name, body.description);
    res.json({ ok: true });
  });

  r.post('/recipes/:id/steps', (req, res) => {
    const body = req.body as { kind: StepKindId; position?: number };
    res.json(c.recipes.addStep(req.params.id, body.kind, body.position));
  });
  r.delete('/recipes/:id/steps/:stepId', (req, res) => {
    c.recipes.removeStep(req.params.id, req.params.stepId);
    res.json({ ok: true });
  });
  r.post('/recipes/:id/steps/:stepId/move', (req, res) => {
    const body = req.body as { delta: number };
    c.recipes.moveStep(req.params.id, req.params.stepId, Number(body.delta));
    res.json({ ok: true });
  });
  r.put('/recipes/:id/steps/:stepId', (req, res) => {
    const body = req.body as { wait?: boolean; enabled?: boolean; params?: Record<string, unknown> };
    c.recipes.updateStep(req.params.id, req.params.stepId, body);
    res.json({ ok: true });
  });

  return r;
}

// Local alias to keep above type compact
type Parameters = import('../types').Parameters;
