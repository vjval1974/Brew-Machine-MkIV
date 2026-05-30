// GPIO HAL — wraps node-libgpiod for Pi 5 (and earlier). Falls back to a
// mock backend if MOCK_HARDWARE=1 or libgpiod isn't installed.
//
// Original C used the STM32 std-periph driver:
//   GPIO_Init(...); GPIO_WriteBit(...); GPIO_ReadInputDataBit(...);
// Replaced here with explicit acquireOutput / writeOutput / readInput calls.

import fs from 'fs';
import log from '../util/logger';
import bus from '../util/emitter';

const MOCK = process.env.MOCK_HARDWARE === '1';

interface GpioOptions {
  pull?: 'up' | 'down' | 'none';
}

interface PulseOptions {
  edge?: 'rising' | 'falling' | 'both';
  onPulse?: () => void;
}

interface GpioBackend {
  acquireOutput(name: string, bcm: number | null, initial?: number): void;
  acquireInput(name: string, bcm: number | null, opts?: GpioOptions): void;
  acquirePulseInput(name: string, bcm: number | null, opts?: PulseOptions): void;
  setValue(bcm: number, value: number): void;
  getValue(bcm: number): number;
  cleanup(): void;
  _setInput?(name: string, value: number): void;
  _getInputByName?(name: string): number | undefined;
}

// Lazy-load libgpiod so the module still works in mock mode without it
// installed.
interface Line {
  requestOutputMode: (initial: number, consumer: string) => void;
  requestInputMode: (consumer: string, flags?: number) => void;
  requestEventMode: (consumer: string, flag: number) => void;
  setValue: (v: number) => void;
  getValue: () => number;
  eventWait: (timeoutNs: bigint) => Promise<unknown>;
  eventRead: () => unknown;
  release?: () => void;
}
interface Libgpiod {
  Chip: new (path: string) => unknown;
  Line: new (chip: unknown, bcm: number) => Line;
  LINE_REQ_FLAG_BIAS_PULL_UP: number;
  LINE_REQ_FLAG_BIAS_PULL_DOWN: number;
  LINE_REQ_EV_RISING_EDGE: number;
  LINE_REQ_EV_FALLING_EDGE: number;
  LINE_REQ_EV_BOTH_EDGES: number;
}

let libgpiod: Libgpiod | null = null;
if (!MOCK) {
  try {
    // eslint-disable-next-line @typescript-eslint/no-require-imports
    libgpiod = require('node-libgpiod') as Libgpiod;
  } catch (err) {
    log.warn(`node-libgpiod not available (${(err as Error).message}). Falling back to mock GPIO.`);
  }
}

function detectChip(): string {
  for (const p of ['/dev/gpiochip4', '/dev/gpiochip0']) {
    try { fs.accessSync(p); return p; } catch { /* keep looking */ }
  }
  return '/dev/gpiochip0';
}

class RealGpioBackend implements GpioBackend {
  private chip: unknown;
  private lines = new Map<number, Line>();
  private values = new Map<number, number>();

  constructor() {
    if (!libgpiod) throw new Error('libgpiod not loaded');
    const chipPath = detectChip();
    log.info(`GPIO backend: libgpiod on ${chipPath}`);
    this.chip = new libgpiod.Chip(chipPath);
  }

  acquireOutput(name: string, bcm: number | null, initial = 0): void {
    if (bcm === null) throw new Error(`GPIO ${name}: pinmap entry is null — assign a BCM pin in config/pinmap.ts`);
    if (!libgpiod) throw new Error('libgpiod not loaded');
    const line = new libgpiod.Line(this.chip, bcm);
    line.requestOutputMode(initial, `brew-${name}`);
    this.lines.set(bcm, line);
    this.values.set(bcm, initial);
  }

  acquireInput(name: string, bcm: number | null, opts: GpioOptions = {}): void {
    if (bcm === null) throw new Error(`GPIO ${name}: pinmap entry is null`);
    if (!libgpiod) throw new Error('libgpiod not loaded');
    const line = new libgpiod.Line(this.chip, bcm);
    const flags =
      opts.pull === 'up'   ? libgpiod.LINE_REQ_FLAG_BIAS_PULL_UP   :
      opts.pull === 'down' ? libgpiod.LINE_REQ_FLAG_BIAS_PULL_DOWN :
      0;
    line.requestInputMode(`brew-${name}`, flags);
    this.lines.set(bcm, line);
  }

  acquirePulseInput(name: string, bcm: number | null, opts: PulseOptions = {}): void {
    if (bcm === null) throw new Error(`GPIO ${name}: pinmap entry is null`);
    if (!libgpiod) throw new Error('libgpiod not loaded');
    const flag =
      opts.edge === 'rising' ? libgpiod.LINE_REQ_EV_RISING_EDGE :
      opts.edge === 'both'   ? libgpiod.LINE_REQ_EV_BOTH_EDGES  :
                               libgpiod.LINE_REQ_EV_FALLING_EDGE;
    const line = new libgpiod.Line(this.chip, bcm);
    line.requestEventMode(`brew-${name}`, flag);
    this.lines.set(bcm, line);
    const onPulse = opts.onPulse;
    void (async () => {
      while (this.lines.has(bcm)) {
        try {
          const ev = await line.eventWait(BigInt(500_000_000));
          if (ev) {
            line.eventRead();
            onPulse?.();
          }
        } catch (err) {
          log.error(`Pulse poll ${name}: ${(err as Error).message}`);
          await new Promise((r) => setTimeout(r, 200));
        }
      }
    })();
  }

  setValue(bcm: number, value: number): void {
    const line = this.lines.get(bcm);
    if (!line) throw new Error(`GPIO bcm=${bcm} not acquired`);
    line.setValue(value ? 1 : 0);
    this.values.set(bcm, value ? 1 : 0);
  }

  getValue(bcm: number): number {
    const line = this.lines.get(bcm);
    if (!line) throw new Error(`GPIO bcm=${bcm} not acquired`);
    return line.getValue();
  }

  cleanup(): void {
    log.info('GPIO cleanup: driving all outputs low');
    for (const line of this.lines.values()) {
      try { line.setValue(0); } catch { /* ignore */ }
      try { line.release?.(); } catch { /* ignore */ }
    }
    this.lines.clear();
  }
}

class MockGpioBackend implements GpioBackend {
  private outputs = new Map<string, { bcm: number | null; value: number }>();
  private inputs  = new Map<string, { bcm: number | null; value: number }>();

  constructor() { log.info('GPIO backend: MOCK'); }

  acquireOutput(name: string, bcm: number | null, initial = 0): void {
    if (bcm === null) log.warn(`Mock GPIO: output "${name}" has null BCM — accepted in mock mode`);
    this.outputs.set(name, { bcm, value: initial });
  }
  acquireInput(name: string, bcm: number | null /*, opts */): void {
    this.inputs.set(name, { bcm, value: 1 });
  }
  acquirePulseInput(): void { /* mock: never fires unless _injectPulses used */ }
  setValue(bcm: number, value: number): void {
    for (const v of this.outputs.values()) if (v.bcm === bcm) v.value = value ? 1 : 0;
  }
  getValue(bcm: number): number {
    for (const v of this.outputs.values()) if (v.bcm === bcm) return v.value;
    for (const v of this.inputs.values())  if (v.bcm === bcm) return v.value;
    return 0;
  }
  _setInput(name: string, value: number): void {
    const i = this.inputs.get(name);
    if (i) i.value = value ? 1 : 0;
  }
  _getInputByName(name: string): number | undefined {
    const i = this.inputs.get(name);
    return i?.value;
  }
  cleanup(): void { /* noop */ }
}

// IMPORTANT: a missing libgpiod binding must NOT silently fall back to a
// mock backend when the user expected real hardware. The only path to the
// mock backend is the explicit MOCK_HARDWARE=1 environment variable. This
// closes the failure mode where a fresh checkout on the Pi without
// `node-libgpiod` installed would "successfully" boot driving a 230 V
// element with no GPIO ever actually being written.
function makeBackend(): GpioBackend {
  if (MOCK) return new MockGpioBackend();
  if (!libgpiod) {
    throw new Error(
      'GPIO HAL: node-libgpiod is not installed and MOCK_HARDWARE is not set. ' +
      'Install libgpiod-dev and `npm install`, or set MOCK_HARDWARE=1 explicitly.'
    );
  }
  return new RealGpioBackend();
}
const backend: GpioBackend = makeBackend();

const acquiredOutputs: Record<string, number | null> = {};
const acquiredInputs:  Record<string, number | null> = {};

export function acquireOutput(name: string, bcm: number | null, initial = 0): void {
  backend.acquireOutput(name, bcm, initial);
  acquiredOutputs[name] = bcm;
}

export function acquireInput(name: string, bcm: number | null, opts?: GpioOptions): void {
  backend.acquireInput(name, bcm, opts);
  acquiredInputs[name] = bcm;
}

export function acquirePulseInput(name: string, bcm: number | null, opts?: PulseOptions): void {
  backend.acquirePulseInput(name, bcm, opts);
}

export function writeOutput(name: string, value: number | boolean): void {
  const bcm = acquiredOutputs[name];
  if (bcm === undefined) throw new Error(`Output ${name} not acquired`);
  if (bcm === null) { /* mock-mode placeholder pin: no-op */ }
  else backend.setValue(bcm, value ? 1 : 0);
  bus.emit('gpio:output', { name, value: value ? 1 : 0 });
}

export function readInput(name: string): number {
  const bcm = acquiredInputs[name];
  if (bcm === undefined) throw new Error(`Input ${name} not acquired`);
  if (bcm === null) {
    // Mock-mode placeholder pin: consult the backend by name so test
    // helpers (gpio._mockSetInput) and the diagnostics UI can drive
    // simulated inputs even without a real BCM mapping.
    const v = backend._getInputByName?.(name);
    return v ?? 1; // default to "released" (pull-up high) if never set
  }
  return backend.getValue(bcm);
}

export function cleanup(): void { backend.cleanup(); }

export function isMock(): boolean { return MOCK; }

export function _mockSetInput(name: string, value: number): void {
  backend._setInput?.(name, value);
}

/**
 * Assert that a set of pins are mapped to real BCM numbers before boot
 * completes. Refuses to continue if any safety-critical signal is `null`
 * on real hardware. In mock mode the check is logged but does not throw,
 * since `pinmap.ts` ships with placeholder `null` values by design.
 */
export function assertCriticalPinsMapped(
  pins: { kind: 'output' | 'input'; name: string; bcm: number | null }[]
): void {
  const unmapped = pins.filter((p) => p.bcm === null);
  if (unmapped.length === 0) return;
  const names = unmapped.map((p) => p.name).join(', ');
  if (MOCK) {
    log.warn(`Critical pins unmapped (${names}) — allowed only because MOCK_HARDWARE=1`);
    return;
  }
  throw new Error(
    `Refusing to boot: safety-critical pins are unmapped in pinmap.ts: ${names}. ` +
    `Assign real BCM numbers, or set MOCK_HARDWARE=1 to bench-test without hardware.`
  );
}

export default {
  acquireOutput, acquireInput, acquirePulseInput,
  writeOutput, readInput,
  cleanup, isMock, _mockSetInput,
  assertCriticalPinsMapped,
};
