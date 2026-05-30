# Architecture — `rpi/`

This document captures the bounded contexts, dependency rules, and
ownership boundaries the codebase enforces. It is the lasting output of
the five-architect design review (see commit history around `940ca46`).
The TL;DR: **modular monolith inside one process, with hardware safety
in physical hardware below it**, not microservices.

## Bounded contexts

```
src/
├── domains/
│   ├── brewing/    orchestrator: brew step engine (the WAIT-flag driver)
│   │               owns store section: brew
│   ├── hlt/        HLT tank — heater SSR, level monitor, fill/heat/drain
│   │               owns store section: hlt
│   ├── boil/       boil kettle — PWM duty controller, level
│   │               owns store section: boil
│   ├── hydraulics/ discrete + motorised valves, pumps, flow meter, mash-water
│   │               owns store sections: valves, pumps, boilValve, flow, mashWater
│   ├── motion/     crane, stir, grain mill, hop dropper
│   │               owns store sections: crane, stir, mill, hopDropper
│   └── sensing/    DS18B20 temperature poller + rolling-buffer stability check
│                   owns store section: temps
├── platform/
│   ├── hal/        libgpiod, i2c-bus, w1, sysfs PWM, watchdog, debounce
│   │               + workers/ (flow pulse counter thread)
│   ├── store/      central state + section ownership registry
│   ├── parameters/ JSON-persisted brew parameters
│   └── util/       logger, in-process event bus
├── server/         Express REST + WebSocket — kiosk talks to here
├── config/         pinmap.ts (BCMs) + parameters.default.json (recipe defaults)
└── index.ts        wires the above + declares ownership + boots watchdog
```

## Dependency rules

1. **`brewing → {hlt, boil, hydraulics, motion, sensing}`** — the brew
   step engine is the only module allowed to issue cross-domain
   **commands**. Its `steps[]` IS the aggregate. Sequencing lives here
   and nowhere else.
2. **Aggregates may read each other's state, never call each other's
   commands.** State reads are synchronous and same-process (e.g.
   `mashPump.okToPump()` reads `crane.state()`). Safety interlocks
   like `okToPump` / `okToStir` MUST stay synchronous function calls
   — never a promise, never a broker hop.
3. **HLT owns the valves it operates as part of heat-and-fill / drain.**
   The HLT controller drives `INLET_VALVE` (fill) and `HLT_VALVE`
   (drain) because those are part of its operational envelope. The
   other discrete valves (MASH, CHILLER) are only touched by the brew
   orchestrator. Documented exception to rule 1.
4. **Domains depend only on `platform/*`, never on each other except via
   the explicit cross-domain reads above.**
5. **`platform/hal` is the only place that touches hardware.** All
   GPIO/I2C/1-Wire/PWM/watchdog reads + writes pass through here.

## State ownership

Each store section has exactly one canonical writer. Declared centrally
in `index.ts#declareDomainOwners`:

| Section       | Owner         |
|---------------|---------------|
| brew          | brewing       |
| hlt           | hlt           |
| boil          | boil          |
| valves        | hydraulics    |
| pumps         | hydraulics    |
| boilValve     | hydraulics    |
| flow          | hydraulics    |
| mashWater     | hydraulics    |
| crane         | motion        |
| stir          | motion        |
| mill          | motion        |
| hopDropper    | motion        |
| temps         | sensing       |
| parameters    | platform      |

The `Store.patch(section, patch, writer?)` API accepts an optional
`writer` argument. When passed, the store consults the registry and
warns on mismatch. Today this is documentation; PR review enforces it
case-by-case. A future ESLint rule or runtime mode can promote this to
hard enforcement.

## Process / hardware planes

```
┌─ Plane 0: Hardware (no CPU) ─────────────────────────────────────────┐
│   E-stop NC ─┐                                                       │
│   HLT float ─┤ ── series chain ── Mains contactor coil               │
│   Klixon NC ─┤                            │                          │
│   WDT IC OK ─┘                            ▼                          │
│                                   ┌────────────────┐                 │
│                                   │   230 V SSRs    │                │
│                                   └────────────────┘                 │
└──────────────────────────────────────────────────────────────────────┘
          ▲ Pi GPIO 26 toggle every 250 ms (watchdog.ts)
          │
┌─ Plane 1: Pi userspace, single Node process ─────────────────────────┐
│   src/domains/* + src/platform/* + src/server/* + src/index.ts       │
│   plus one worker_thread: flow pulse counter (platform/hal/workers/) │
│   /dev/watchdog kicked from main loop                                │
│   External WDT IC toggled from main loop (BCM 26)                    │
└──────────────────────────────────────────────────────────────────────┘
          ▲ WebSocket
┌─ Plane 2: Browser kiosk (Chromium fullscreen) ───────────────────────┐
│   web/ — static assets + ws client                                   │
└──────────────────────────────────────────────────────────────────────┘
```

Future microservice/co-processor splits (per the architect synthesis)
are deliberately **deferred**:

- RP2040 co-processor — only if pigpio/HW-PWM measurements on real Pi 5
  hardware show jitter outside the spec.
- Separate `brew-safety` process — likely redundant once the hardware
  contactor + external WDT IC are wired.
- MQTT broker — wait for a real second consumer (Home Assistant bridge,
  multi-vessel rig).

## Why not microservices

Recapping the synthesis so the next architect-curious reader doesn't
rerun the meeting: this is a single-host, single-operator, single-rig
controller. The signals that matter (level interlock, drive interlock,
brew sequencing) are dominated by sub-second coordination across
modules. Replacing in-process function calls with HTTP/MQTT hops would
add latency, serialisation, and partial-failure modes to operations
that are currently atomic. Conway's law cuts the other way too: there
is one team (one person). The expensive cure has no disease to treat.

Microservice _thinking_ that does apply, and that is honoured here:
sharp module boundaries, one writer per section, contract-shaped
publish/subscribe inside the process, and **the safety watchdog as a
genuinely independent piece of hardware**. That's where process /
device isolation buys you something.

## Conventions for future PRs

- Add a new file → it lives under the domain that owns the behaviour,
  or under `platform/` if it's hardware/infrastructure. If you can't
  decide, you've probably found a bounded context not yet enumerated
  here; document it before merging.
- Adding a `store.patch('foo', ...)` from outside the owner — get
  reviewer sign-off. The point is to surface coupling, not block it.
- Adding a `store.declareOwner` — update the table above.
- Touching `platform/hal/*` — pair the change with a measurement plan
  for jitter / dropped events / fault behaviour. The HAL is where the
  watchdog matters most.
