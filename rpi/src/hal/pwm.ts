// Hardware PWM HAL — drives the boil-element SSR.
//
// Linux sysfs PWM (/sys/class/pwm/pwmchipN). On Pi 5, hardware PWM lives on
// GPIO 12/13/18/19; enable with
//     dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4
// in /boot/firmware/config.txt.
//
// Falls back to software PWM (setInterval flipping a GPIO) in mock mode or
// when sysfs isn't available.

import { promises as fs } from 'fs';
import log from '../util/logger';
import gpio from './gpio';

const MOCK = process.env.MOCK_HARDWARE === '1';

export interface Pwm {
  init(): Promise<void>;
  setDuty(percent: number): Promise<void>;
  stop(): Promise<void>;
}

export interface PwmConfig {
  name?: string;
  chip: number;
  channel: number;
  bcm: number;
  periodHz: number;
}

class HardwarePwm implements Pwm {
  private base: string;
  private channel: number;
  private dir: string;
  private periodNs: number;
  private dutyNs = 0;
  private enabled = false;

  constructor(cfg: PwmConfig) {
    this.base = `/sys/class/pwm/pwmchip${cfg.chip}`;
    this.channel = cfg.channel;
    this.dir = `${this.base}/pwm${cfg.channel}`;
    this.periodNs = Math.round(1e9 / cfg.periodHz);
  }

  private async write(file: string, value: number | string): Promise<void> {
    await fs.writeFile(`${this.dir}/${file}`, String(value));
  }

  async init(): Promise<void> {
    try { await fs.access(this.dir); }
    catch {
      await fs.writeFile(`${this.base}/export`, String(this.channel));
      await new Promise((r) => setTimeout(r, 200));   // wait for udev to chmod
    }
    await this.write('period', this.periodNs);
    await this.write('duty_cycle', 0);
    await this.write('enable', 0);
  }

  async setDuty(percent: number): Promise<void> {
    const p = Math.max(0, Math.min(100, percent));
    this.dutyNs = Math.round((this.periodNs * p) / 100);
    await this.write('duty_cycle', this.dutyNs);
    const want = p > 0;
    if (want !== this.enabled) {
      await this.write('enable', want ? 1 : 0);
      this.enabled = want;
    }
  }

  async stop(): Promise<void> {
    try { await this.write('enable', 0); } catch { /* ignore */ }
    this.enabled = false;
    this.dutyNs = 0;
  }
}

interface SoftwarePwmConfig {
  name?: string;
  bcm: number | null;
  periodHz: number;
}

class SoftwarePwm implements Pwm {
  private name: string;
  private bcm: number | null;
  private periodMs: number;
  private duty = 0;
  private timer: NodeJS.Timeout | null = null;
  private gpioName: string;

  constructor(cfg: SoftwarePwmConfig) {
    this.name = cfg.name ?? 'pwm';
    this.bcm = cfg.bcm;
    this.periodMs = Math.round(1000 / cfg.periodHz);
    this.gpioName = `SOFT_PWM_${this.name}`;
  }

  async init(): Promise<void> {
    if (!MOCK && this.bcm !== null) gpio.acquireOutput(this.gpioName, this.bcm, 0);
  }

  async setDuty(percent: number): Promise<void> {
    this.duty = Math.max(0, Math.min(100, percent));
    if (this.timer) { clearTimeout(this.timer); this.timer = null; }
    if (this.duty === 0)   { if (!MOCK) gpio.writeOutput(this.gpioName, 0); return; }
    if (this.duty === 100) { if (!MOCK) gpio.writeOutput(this.gpioName, 1); return; }
    const onMs  = Math.round((this.periodMs * this.duty) / 100);
    const offMs = this.periodMs - onMs;
    let high = false;
    const flip = (): void => {
      high = !high;
      if (!MOCK) gpio.writeOutput(this.gpioName, high ? 1 : 0);
      this.timer = setTimeout(flip, high ? onMs : offMs);
    };
    flip();
  }

  async stop(): Promise<void> {
    if (this.timer) { clearTimeout(this.timer); this.timer = null; }
    this.duty = 0;
    if (!MOCK) { try { gpio.writeOutput(this.gpioName, 0); } catch { /* ignore */ } }
  }
}

export async function createPwm(cfg: PwmConfig): Promise<Pwm> {
  if (MOCK) {
    log.info(`PWM ${cfg.name ?? ''}: MOCK`);
    const sw = new SoftwarePwm({ ...cfg, bcm: null as number | null });
    await sw.init();
    return sw;
  }
  try {
    const hw = new HardwarePwm(cfg);
    await hw.init();
    log.info(`PWM ${cfg.name ?? ''}: hardware on pwmchip${cfg.chip}/pwm${cfg.channel}`);
    return hw;
  } catch (err) {
    log.warn(`Hardware PWM failed (${(err as Error).message}); falling back to software PWM on GPIO ${cfg.bcm}`);
    const sw = new SoftwarePwm(cfg);
    await sw.init();
    return sw;
  }
}
