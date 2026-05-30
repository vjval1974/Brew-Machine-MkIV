// Step-kind registry.
//
// A recipe is a list of `StepInstance`s. Each instance references a `kind` in
// this registry. The kind owns the parameter shape (used by the UI to render
// a per-step form) and the `run(params)` function that actually drives the
// hardware. Adding a new kind: append to the catalog below.
//
// Why a fixed registry vs free-form code? The UI cannot execute arbitrary
// code safely; allowing user-typed JS would be a remote-exec vector. A
// closed set of well-typed primitives is brewing-flexible enough (every
// existing step in the original brew.c BrewSteps[] is expressible) and
// keeps the safety surface bounded.

import log from '../../platform/util/logger';
import { sleep } from '../../platform/hal/io_util';

import * as valves      from '../hydraulics/valves';
import * as mashPump    from '../hydraulics/mashPump';
import * as chillerPump from '../hydraulics/chillerPump';
import * as boilValve   from '../hydraulics/boilValve';
import * as flow        from '../hydraulics/flow';
import * as mashWater   from '../hydraulics/mashWater';
import * as mill        from '../motion/mill';
import * as stir        from '../motion/stir';
import * as crane       from '../motion/crane';
import * as hopDropper  from '../motion/hopDropper';
import * as hlt         from '../hlt/hlt';
import * as boil        from '../boil/boil';

import type { StepKindId, StepKindMeta, ParamSpec, ValveName } from '../../types';

const SEC = 1000;
const MIN = 60_000;

/** A step kind: metadata + the function that runs it. */
export interface StepKind {
  meta: StepKindMeta;
  run:  (params: Record<string, unknown>, abort: () => boolean) => Promise<void>;
}

/** Pull a typed param with a fallback to the kind's declared default. */
function p<T>(params: Record<string, unknown>, key: string, spec: ParamSpec): T {
  const v = params[key];
  if (v === undefined || v === null) return spec.default as T;
  return v as T;
}

/** Abortable sleep — wakes early when the brew engine sets quitFlag. */
async function abortableSleep(ms: number, abort: () => boolean): Promise<void> {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    if (abort()) throw new Error('QUIT');
    await sleep(Math.min(100, ms - (Date.now() - t0)));
  }
}

// ── Catalog ─────────────────────────────────────────────────────────────────
// Each entry declares its params (driving the UI form) and the run body. The
// `run` body should be small — push real work into the controllers.

const catalog: StepKind[] = [
  {
    meta: {
      id: 'wait',
      displayName: 'Wait',
      description: 'Pause for a fixed duration. Useful as a settle / rest step.',
      params: [
        { key: 'ms', label: 'Duration', type: 'integer', default: 3000, min: 0, unit: 'ms' },
      ],
    },
    run: async (params, abort) => {
      const ms = Number(params.ms ?? 3000);
      await abortableSleep(ms, abort);
    },
  },

  {
    meta: {
      id: 'crane.up',
      displayName: 'Crane → Top',
      description: 'Raise the grain basket until the upper limit switch trips.',
      params: [],
    },
    run: () => crane.up(),
  },

  {
    meta: {
      id: 'crane.down',
      displayName: 'Crane → Bottom',
      description: 'Lower the grain basket continuously to the lower limit.',
      params: [],
    },
    run: () => crane.down(),
  },

  {
    meta: {
      id: 'crane.incremental',
      displayName: 'Crane → Bottom (incremental)',
      description: 'Mash-in descent: pulse down, pause, repeat. Kicks the stirrer on after ~11 pulses.',
      params: [],
    },
    run: () => crane.incremental(),
  },

  {
    meta: {
      id: 'mill.run_for',
      displayName: 'Run Mill',
      description: 'Crush the grain for a fixed duration.',
      params: [
        { key: 'minutes', label: 'Duration', type: 'number', default: 15, min: 0, step: 0.5, unit: 'min' },
      ],
    },
    run: (params, abort) =>
      mill.runFor(Number(params.minutes ?? 0) * MIN, abort),
  },

  {
    meta: {
      id: 'hlt.heat_and_fill',
      displayName: 'HLT: Heat & Fill',
      description: 'Open INLET to fill HLT to HIGH level and hold heater on until setpoint reached.',
      params: [
        { key: 'setpoint_c', label: 'Setpoint', type: 'number', default: 75, min: 0, max: 100, step: 0.5, unit: '°C' },
      ],
    },
    run: (params) => hlt.heatAndFill(Number(params.setpoint_c ?? 75)),
  },

  {
    meta: {
      id: 'hlt.drain_to',
      displayName: 'HLT: Drain',
      description:
        'Open the destination valve, drain a target volume from the HLT via the flow meter, then close. ' +
        'destination=mash routes through the MASH valve (to the mash tun); destination=boil opens the boil valve.',
      params: [
        { key: 'litres', label: 'Litres', type: 'number', default: 21, min: 0, step: 0.1, unit: 'L' },
        {
          key: 'destination',
          label: 'Destination',
          type: 'select',
          options: ['mash', 'boil', 'none'],
          default: 'mash',
        },
      ],
    },
    run: async (params) => {
      const litres = Number(params.litres ?? 0);
      const dest   = String(params.destination ?? 'mash') as 'mash' | 'boil' | 'none';
      if (dest === 'mash') {
        valves.open('MASH');
        await sleep(500);
      } else if (dest === 'boil') {
        await boilValve.open();
      }
      try {
        await hlt.drain(litres);
      } finally {
        if (dest === 'mash') valves.close('MASH');
        else if (dest === 'boil') void boilValve.close();
      }
    },
  },

  {
    meta: {
      id: 'valves.close_all',
      displayName: 'Close all discrete valves',
      description: 'Snap HLT, MASH, INLET and CHILLER valves closed.',
      params: [],
    },
    run: async () => { valves.closeAll(); },
  },

  {
    meta: {
      id: 'valve.set',
      displayName: 'Set valve',
      description: 'Open or close one of the four discrete valves.',
      params: [
        { key: 'valve', label: 'Valve', type: 'select', options: ['HLT', 'MASH', 'INLET', 'CHILLER'], default: 'INLET' },
        { key: 'state', label: 'State', type: 'select', options: ['open', 'closed'], default: 'closed' },
      ],
    },
    run: async (params) => {
      const v = String(params.valve ?? 'INLET') as ValveName;
      if (params.state === 'open') valves.open(v);
      else                          valves.close(v);
    },
  },

  {
    meta: {
      id: 'boil_valve.set',
      displayName: 'Set boil valve',
      description: 'Drive the motorised boil valve to OPEN or CLOSED (waits for limit switch).',
      params: [
        { key: 'state', label: 'State', type: 'select', options: ['opened', 'closed'], default: 'closed' },
      ],
    },
    run: async (params) => {
      if (params.state === 'opened') await boilValve.open();
      else                            await boilValve.close();
    },
  },

  {
    meta: {
      id: 'mash_cycle',
      displayName: 'Mash cycle',
      description:
        'Hold the mash at temperature for `total_min` minutes, with optional pump + stir bookends. ' +
        'Replicates the original mash step.',
      params: [
        { key: 'total_min',     label: 'Total time',  type: 'number', default: 60, min: 0, step: 1, unit: 'min' },
        { key: 'stir_t1_min',   label: 'Initial stir', type: 'number', default: 10, min: 0, step: 0.5, unit: 'min' },
        { key: 'pump_t1_min',   label: 'Initial pump', type: 'number', default: 10, min: 0, step: 0.5, unit: 'min' },
        { key: 'pump_t2_min',   label: 'Final pump',   type: 'number', default: 5,  min: 0, step: 0.5, unit: 'min' },
        { key: 'stir_t2_min',   label: 'Final stir',   type: 'number', default: 0,  min: 0, step: 0.5, unit: 'min' },
      ],
    },
    run: async (params, abort) => {
      const total = Number(params.total_min   ?? 60);
      const s1    = Number(params.stir_t1_min ?? 0);
      const p1    = Number(params.pump_t1_min ?? 0);
      const p2    = Number(params.pump_t2_min ?? 0);
      const s2    = Number(params.stir_t2_min ?? 0);
      if (s1 > 0) { stir.start();     await abortableSleep(s1 * MIN, abort); stir.stop(); }
      if (p1 > 0) { mashPump.start(); await abortableSleep(p1 * MIN, abort); mashPump.stop(); }
      const remaining = total - s1 - p1 - p2 - s2;
      if (remaining > 0) await abortableSleep(remaining * MIN, abort);
      if (p2 > 0) { mashPump.start(); await abortableSleep(p2 * MIN, abort); mashPump.stop(); }
      if (s2 > 0) { stir.start();     await abortableSleep(s2 * MIN, abort); stir.stop(); }
    },
  },

  {
    meta: {
      id: 'sparge_cycle',
      displayName: 'Sparge cycle',
      description: 'Sparge rest with optional pump + stir bookends. Same shape as mash_cycle.',
      params: [
        { key: 'total_min',   label: 'Total time',   type: 'number', default: 15, min: 0, step: 1, unit: 'min' },
        { key: 'stir_t1_min', label: 'Initial stir', type: 'number', default: 5,  min: 0, step: 0.5, unit: 'min' },
        { key: 'pump_t1_min', label: 'Initial pump', type: 'number', default: 5,  min: 0, step: 0.5, unit: 'min' },
        { key: 'pump_t2_min', label: 'Final pump',   type: 'number', default: 5,  min: 0, step: 0.5, unit: 'min' },
        { key: 'stir_t2_min', label: 'Final stir',   type: 'number', default: 0,  min: 0, step: 0.5, unit: 'min' },
      ],
    },
    run: async (params, abort) => {
      const total = Number(params.total_min   ?? 15);
      const s1    = Number(params.stir_t1_min ?? 0);
      const p1    = Number(params.pump_t1_min ?? 0);
      const p2    = Number(params.pump_t2_min ?? 0);
      const s2    = Number(params.stir_t2_min ?? 0);
      if (s1 > 0) { stir.start();     await abortableSleep(s1 * MIN, abort); stir.stop(); }
      if (p1 > 0) { mashPump.start(); await abortableSleep(p1 * MIN, abort); mashPump.stop(); }
      const remaining = total - s1 - p1 - p2 - s2;
      if (remaining > 0) await abortableSleep(remaining * MIN, abort);
      if (p2 > 0) { mashPump.start(); await abortableSleep(p2 * MIN, abort); mashPump.stop(); }
      if (s2 > 0) { stir.start();     await abortableSleep(s2 * MIN, abort); stir.stop(); }
    },
  },

  {
    meta: {
      id: 'pump_to_boil_cycle',
      displayName: 'Pump mash → boil (recycle)',
      description:
        'Open the boil valve, run the mash pump in on/off recycle bursts for `total_sec`, then close. ' +
        'Marks the mash tun as drained.',
      params: [
        { key: 'total_sec',       label: 'Total time', type: 'integer', default: 180, min: 0, unit: 's' },
        { key: 'recycle_on_sec',  label: 'Pump on',    type: 'integer', default: 30,  min: 1, unit: 's' },
        { key: 'recycle_off_sec', label: 'Pump off',   type: 'integer', default: 300, min: 1, unit: 's' },
      ],
    },
    run: async (params, abort) => {
      const total = Number(params.total_sec ?? 180) * SEC;
      const onMs  = Number(params.recycle_on_sec  ?? 30)  * SEC;
      const offMs = Number(params.recycle_off_sec ?? 300) * SEC;
      await boilValve.open();
      mashPump.start();
      const t0 = Date.now();
      while (Date.now() - t0 < total) {
        await abortableSleep(Math.min(onMs,  total - (Date.now() - t0)), abort);
        if (Date.now() - t0 >= total) break;
        mashPump.stop();
        await abortableSleep(Math.min(offMs, total - (Date.now() - t0)), abort);
        mashPump.start();
      }
      mashPump.stop();
      mashWater.mashTunDrained();
    },
  },

  {
    meta: {
      id: 'bring_to_boil',
      displayName: 'Bring to boil',
      description: 'Set boil duty to 100% for the configured time (the kettle ramp-up).',
      params: [
        { key: 'minutes', label: 'Duration', type: 'number', default: 17, min: 0, step: 1, unit: 'min' },
      ],
    },
    run: async (params, abort) => {
      await boil.bringToBoil();
      await abortableSleep(Number(params.minutes ?? 0) * MIN, abort);
    },
  },

  {
    meta: {
      id: 'boil_cycle',
      displayName: 'Boil with timed hop additions',
      description:
        'Run the kettle PWM at `duty_pct` for `duration_min` minutes. Drops one hop addition at each minute-' +
        'remaining value in `hop_times_min`. e.g. [60, 30, 15, 5] = drops at start, then 30/45/55 min in.',
      params: [
        { key: 'duration_min', label: 'Boil duration', type: 'number', default: 60, min: 0, step: 1, unit: 'min' },
        { key: 'duty_pct',     label: 'Duty cycle',    type: 'integer', default: 60, min: 0, max: 100, unit: '%' },
        { key: 'hop_times_min', label: 'Hop times (min before end, comma-sep)', type: 'string', default: '60,30,15,5,1,1' },
      ],
    },
    run: async (params, abort) => {
      const totalMin = Number(params.duration_min ?? 60);
      const duty     = Number(params.duty_pct     ?? 60);
      const hopStr   = String(params.hop_times_min ?? '');
      const hopTimes = hopStr
        .split(',')
        .map((s) => Number(s.trim()))
        .filter((n) => Number.isFinite(n) && n >= 0);
      await boil.setDuty(duty);
      const scheduled = hopTimes
        .map((t, i) => ({ index: i, at: (totalMin - t) * MIN }))
        .filter((x) => x.at >= 0)
        .sort((a, b) => a.at - b.at);
      const totalMs = totalMin * MIN;
      const t0 = Date.now();
      let nextIdx = 0;
      while (Date.now() - t0 < totalMs) {
        if (nextIdx < scheduled.length && Date.now() - t0 >= scheduled[nextIdx]!.at) {
          log.info(`BREW: hop addition #${scheduled[nextIdx]!.index + 1}`);
          hopDropper.drop().catch((err: Error) => log.warn(`hop drop: ${err.message}`));
          nextIdx++;
        }
        await abortableSleep(1000, abort);
      }
      await boil.stop();
    },
  },

  {
    meta: {
      id: 'chill',
      displayName: 'Chill (chiller pump + valve)',
      description: 'Open the CHILLER valve and run the chiller pump for `minutes` minutes.',
      params: [
        { key: 'minutes', label: 'Duration', type: 'number', default: 20, min: 0, step: 1, unit: 'min' },
      ],
    },
    run: async (params, abort) => {
      valves.open('CHILLER');
      chillerPump.start();
      try {
        await abortableSleep(Number(params.minutes ?? 0) * MIN, abort);
      } finally {
        chillerPump.stop();
        valves.close('CHILLER');
      }
    },
  },

  {
    meta: {
      id: 'pump_to_fermenter',
      displayName: 'Pump out to fermenter',
      description: 'Run the chiller pump for `minutes` to transfer wort.',
      params: [
        { key: 'minutes', label: 'Duration', type: 'number', default: 7, min: 0, step: 1, unit: 'min' },
      ],
    },
    run: async (params, abort) => {
      chillerPump.start();
      try {
        await abortableSleep(Number(params.minutes ?? 0) * MIN, abort);
      } finally {
        chillerPump.stop();
      }
    },
  },

  {
    meta: {
      id: 'safe_states',
      displayName: 'Safe states',
      description:
        'Drive every output to a safe state and clear the mash-water tracker. Use this as the last step ' +
        'to make sure no SSR or pump is left running after a brew finishes.',
      params: [],
    },
    run: async () => {
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
      mashWater.clear();
    },
  },
];

const byId = new Map<StepKindId, StepKind>(catalog.map((k) => [k.meta.id, k]));

/** All step kinds, in display order. */
export function listKinds(): StepKindMeta[] {
  return catalog.map((k) => k.meta);
}

/** Look up a kind by id. Throws if unknown — recipes must validate on load. */
export function getKind(id: StepKindId): StepKind {
  const k = byId.get(id);
  if (!k) throw new Error(`Unknown step kind: ${id}`);
  return k;
}

/** Build a default param payload for a kind (used when adding a step). */
export function defaultParams(id: StepKindId): Record<string, unknown> {
  const k = getKind(id);
  const out: Record<string, unknown> = {};
  for (const p of k.meta.params) out[p.key] = p.default;
  return out;
}
