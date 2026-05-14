'use strict';

const express = require('express');
const store = require('../state/store');

module.exports = function buildRouter(c) {
  const r = express.Router();

  r.get('/state', (_req, res) => res.json(store.get()));
  r.get('/parameters', (_req, res) => res.json(c.parameters.get()));
  r.put('/parameters/:key', (req, res) => {
    c.parameters.set(req.params.key, req.body.value);
    res.json({ ok: true, value: req.body.value });
  });
  r.put('/parameters', (req, res) => {
    c.parameters.setMany(req.body || {});
    res.json({ ok: true });
  });

  r.get('/brew/steps', (_req, res) => res.json(c.brew.getSteps()));
  r.post('/brew/start',  (_req, res) => res.json({ ok: c.brew.start() }));
  r.post('/brew/pause',  (_req, res) => { c.brew.pause(); res.json({ ok: true }); });
  r.post('/brew/resume', (_req, res) => { c.brew.resume(); res.json({ ok: true }); });
  r.post('/brew/quit',   (_req, res) => { c.brew.quit(); res.json({ ok: true }); });
  r.post('/brew/goto/:i',(req, res) => { c.brew.gotoStep(Number(req.params.i)); res.json({ ok: true }); });

  r.post('/valve/:name/:action', (req, res) => {
    const { name, action } = req.params;
    if (action === 'open')  c.valves.open(name);
    else if (action === 'close') c.valves.close(name);
    else c.valves.toggle(name);
    res.json({ ok: true, state: c.valves.state(name) });
  });

  r.post('/pump/mash/toggle',    (_req, res) => { c.mashPump.toggle();    res.json({ state: c.mashPump.state() }); });
  r.post('/pump/chiller/toggle', (_req, res) => { c.chillerPump.toggle(); res.json({ state: c.chillerPump.state() }); });

  r.post('/mill/:action',  (req, res) => { c.mill[req.params.action](); res.json({ state: c.mill.state() }); });
  r.post('/stir/:action',  (req, res) => { c.stir[req.params.action](); res.json({ state: c.stir.state() }); });
  r.post('/crane/:action', (req, res) => { c.crane[req.params.action](); res.json({ state: c.crane.state() }); });
  r.post('/boilValve/:action', (req, res) => { c.boilValve[req.params.action](); res.json({ state: c.boilValve.state() }); });

  r.post('/hopDropper/drop',  (_req, res) => { c.hopDropper.drop(); res.json({ ok: true }); });
  r.post('/hopDropper/start', (_req, res) => { c.hopDropper.startManual(); res.json({ ok: true }); });
  r.post('/hopDropper/stop',  (_req, res) => { c.hopDropper.stopManual(); res.json({ ok: true }); });

  r.post('/hlt/setpoint', (req, res) => { c.hlt.setSetpoint(Number(req.body.value)); res.json({ ok: true }); });
  r.post('/hlt/start',    (_req, res) => { c.hlt.startHeating(); res.json({ ok: true }); });
  r.post('/hlt/stop',     (_req, res) => { c.hlt.stopHeating();  res.json({ ok: true }); });

  r.post('/boil/duty',  async (req, res) => { await c.boil.setDuty(Number(req.body.value)); res.json({ ok: true }); });
  r.post('/boil/start', async (_req, res) => { await c.boil.start(); res.json({ ok: true }); });
  r.post('/boil/stop',  async (_req, res) => { await c.boil.stop();  res.json({ ok: true }); });

  r.post('/flow/reset', (_req, res) => { c.flow.reset(); res.json({ ok: true }); });

  return r;
};
