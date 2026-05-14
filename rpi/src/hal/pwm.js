'use strict';

// Hardware PWM HAL — drives the boil-element SSR.
//
// Original C used STM32 TIM4 OC1 at 1 Hz, 0..10000 compare value (0..100%
// duty). Replaced here with the Linux sysfs PWM interface
// (/sys/class/pwm/pwmchipN). The Pi 5 exposes hardware PWM on
// GPIO 12/13/18/19; enable with
//     dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4
// in /boot/firmware/config.txt.
//
// Falls back to a software PWM (setInterval flipping a GPIO) in mock mode
// or if sysfs is unavailable.

const fs = require('fs').promises;
const log = require('../util/logger');
const gpio = require('./gpio');

const MOCK = process.env.MOCK_HARDWARE === '1';

class HardwarePwm {
  constructor({ chip, channel, periodHz }) {
    this.base = `/sys/class/pwm/pwmchip${chip}`;
    this.channel = channel;
    this.dir = `${this.base}/pwm${channel}`;
    this.periodNs = Math.round(1e9 / periodHz);
    this.dutyNs = 0;
    this.enabled = false;
  }

  async _write(file, value) {
    await fs.writeFile(`${this.dir}/${file}`, String(value));
  }

  async init() {
    try {
      await fs.access(this.dir);
    } catch (_) {
      await fs.writeFile(`${this.base}/export`, String(this.channel));
      // give udev a moment to chmod
      await new Promise((r) => setTimeout(r, 200));
    }
    await this._write('period', this.periodNs);
    await this._write('duty_cycle', 0);
    await this._write('enable', 0);
  }

  // duty 0..100
  async setDuty(percent) {
    const p = Math.max(0, Math.min(100, percent));
    this.dutyNs = Math.round((this.periodNs * p) / 100);
    await this._write('duty_cycle', this.dutyNs);
    const wantEnabled = p > 0;
    if (wantEnabled !== this.enabled) {
      await this._write('enable', wantEnabled ? 1 : 0);
      this.enabled = wantEnabled;
    }
  }

  async stop() {
    try { await this._write('enable', 0); } catch (_) {}
    this.enabled = false;
    this.dutyNs = 0;
  }
}

class SoftwarePwm {
  constructor({ name, bcm, periodHz }) {
    this.name = name;
    this.bcm = bcm;
    this.periodMs = Math.round(1000 / periodHz);
    this.duty = 0;
    this.timer = null;
    this.gpioName = `SOFT_PWM_${name}`;
  }
  async init() {
    if (!MOCK && this.bcm != null) gpio.acquireOutput(this.gpioName, this.bcm, 0);
  }
  async setDuty(percent) {
    this.duty = Math.max(0, Math.min(100, percent));
    if (this.timer) { clearInterval(this.timer); this.timer = null; }
    if (this.duty === 0) {
      if (!MOCK) gpio.writeOutput(this.gpioName, 0);
      return;
    }
    if (this.duty === 100) {
      if (!MOCK) gpio.writeOutput(this.gpioName, 1);
      return;
    }
    const onMs  = Math.round((this.periodMs * this.duty) / 100);
    const offMs = this.periodMs - onMs;
    let high = false;
    const flip = () => {
      high = !high;
      if (!MOCK) gpio.writeOutput(this.gpioName, high ? 1 : 0);
      this.timer = setTimeout(flip, high ? onMs : offMs);
    };
    flip();
  }
  async stop() {
    if (this.timer) { clearInterval(this.timer); clearTimeout(this.timer); this.timer = null; }
    this.duty = 0;
    if (!MOCK) try { gpio.writeOutput(this.gpioName, 0); } catch (_) {}
  }
}

async function createPwm(cfg) {
  if (MOCK) {
    log.info(`PWM ${cfg.name || ''}: MOCK`);
    return new SoftwarePwm({ name: cfg.name || 'pwm', bcm: null, periodHz: cfg.periodHz });
  }
  // Try hardware sysfs first
  try {
    const hw = new HardwarePwm(cfg);
    await hw.init();
    log.info(`PWM ${cfg.name || ''}: hardware on pwmchip${cfg.chip}/pwm${cfg.channel}`);
    return hw;
  } catch (err) {
    log.warn(`Hardware PWM failed (${err.message}); falling back to software PWM on GPIO ${cfg.bcm}`);
    const sw = new SoftwarePwm({ name: cfg.name || 'pwm', bcm: cfg.bcm, periodHz: cfg.periodHz });
    await sw.init();
    return sw;
  }
}

module.exports = { createPwm };
