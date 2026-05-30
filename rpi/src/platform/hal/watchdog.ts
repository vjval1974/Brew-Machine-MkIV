// Watchdog HAL — two independent watchdogs, kicked together from the main
// control loop.
//
//   1. SoC watchdog at /dev/watchdog (BCM2712). Kicks reboot the Pi if the
//      kernel watchdog isn't fed. Useful when the entire userspace deadlocks.
//
//   2. External watchdog IC (TPS3823 / MAX6369 / ATtiny class) toggled on a
//      GPIO. Wired in series with the mains contactor coil so that if either
//      software stops kicking OR the Pi's GPIO subsystem misbehaves, the
//      contactor drops and mains is removed from the SSRs. The toggle (not
//      a write) is the critical part — a stuck-high or stuck-low pin must
//      not look like a healthy kick.
//
// The whole module is a no-op when no watchdog is configured / available, so
// development on a non-Pi is unaffected.

import fsSync from 'fs';
import log from '../util/logger';
import gpio from './gpio';
import pinmap from '../../config/pinmap';

const MOCK = process.env.MOCK_HARDWARE === '1';

const SOC_PATH = '/dev/watchdog';
const KICK_PERIOD_MS = 250;     // half of the typical 500 ms WDT timeout

let socFd: number | null = null;
let externalAvailable = false;
let externalLevel = 0;
let timer: NodeJS.Timeout | null = null;
let healthy = true;
let started = false;

/** Opens the SoC watchdog. Silent no-op if unavailable. */
async function openSoc(): Promise<void> {
  if (MOCK) return;
  try {
    socFd = fsSync.openSync(SOC_PATH, 'r+');
    log.info(`Watchdog: SoC ${SOC_PATH} opened`);
  } catch (err) {
    log.warn(`Watchdog: SoC ${SOC_PATH} unavailable (${(err as Error).message}). Continuing without it.`);
    socFd = null;
  }
}

/** Acquires the external WDT toggle pin if it's mapped. */
function openExternal(): void {
  const wd = pinmap.outputs.WATCHDOG_KICK;
  if (!wd || wd.bcm === null) {
    log.warn('Watchdog: external WDT pin not mapped (pinmap.outputs.WATCHDOG_KICK.bcm). External hardware watchdog disabled.');
    return;
  }
  gpio.acquireOutput('WATCHDOG_KICK', wd.bcm, 0);
  externalAvailable = true;
  log.info(`Watchdog: external WDT pin mapped on BCM ${wd.bcm}`);
}

function kick(): void {
  if (!healthy) return;                  // failed health → stop kicking → WDT fires
  // SoC watchdog: any byte keeps it alive.
  if (socFd !== null) {
    try { fsSync.writeSync(socFd, Buffer.from('\0')); }
    catch (err) { log.error(`Watchdog: SoC kick failed: ${(err as Error).message}`); }
  }
  // External WDT: must TOGGLE — a stuck pin must not look like a kick.
  if (externalAvailable) {
    externalLevel = externalLevel ? 0 : 1;
    try { gpio.writeOutput('WATCHDOG_KICK', externalLevel); }
    catch (err) { log.error(`Watchdog: external toggle failed: ${(err as Error).message}`); }
  }
}

export async function start(): Promise<void> {
  if (started) return;
  started = true;
  await openSoc();
  openExternal();
  timer = setInterval(kick, KICK_PERIOD_MS);
  kick();
}

/**
 * Mark the system as unhealthy. Stops kicking the watchdog, which forces a
 * timeout-driven reset / contactor drop. Use this from the safety path when
 * an invariant has been violated and software cannot recover safely.
 */
export function fail(reason: string): void {
  if (!healthy) return;
  healthy = false;
  log.error(`Watchdog: FAIL — ${reason}. Will let watchdog fire.`);
}

export async function stop(): Promise<void> {
  if (timer) { clearInterval(timer); timer = null; }
  // Magic-close: writing 'V' before close tells the kernel watchdog to NOT
  // reboot the Pi (clean shutdown). Without 'V' the Pi reboots on close.
  if (socFd !== null) {
    try { fsSync.writeSync(socFd, Buffer.from('V')); } catch { /* ignore */ }
    try { fsSync.closeSync(socFd); } catch { /* ignore */ }
    socFd = null;
  }
}

export function isHealthy(): boolean { return healthy; }
export function hasSoc():    boolean { return socFd !== null; }
export function hasExternal(): boolean { return externalAvailable; }

export default { start, stop, fail, isHealthy, hasSoc, hasExternal };
