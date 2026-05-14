'use strict';

// Central state store. Controllers publish their state changes here; the
// WebSocket server subscribes to `change` events and forwards snapshots to
// connected browsers.

const { EventEmitter } = require('events');

const initial = {
  valves: {
    HLT:     'closed',
    MASH:    'closed',
    INLET:   'closed',
    CHILLER: 'closed',
  },
  pumps: {
    mash:    'stopped',
    chiller: 'stopped',
  },
  mill:    'stopped',
  stir:    'stopped',
  hopDropper: 'stopped',
  crane:   'stopped',
  boilValve: 'stopped',
  hlt: {
    level: 'low',
    heating: false,
    temp: NaN,
    setpoint: 74.5,
    cmd: 'idle',
  },
  boil: {
    state: 'off',          // off | boiling | waiting
    level: 'high',
    duty: 0,
  },
  flow: {
    boilLitres: 0,
    flowing: false,
    measuring: false,
  },
  temps: {
    HLT:  NaN,
    MASH: NaN,
  },
  mashWater: {
    inMash:   0.0,
    inBoiler: 0.0,
  },
  brew: {
    running: 'idle',           // idle | running | paused
    step: 0,
    stepName: 'Idle',
    secondsElapsed: 0,
    stepElapsed: 0,
    maxSteps: 0,
  },
  parameters: {},
};

class Store extends EventEmitter {
  constructor() {
    super();
    this.state = JSON.parse(JSON.stringify(initial));
    this.setMaxListeners(50);
  }

  // patch may be a deep partial; only included keys are updated.
  patch(section, patch) {
    if (typeof section === 'string') {
      this.state[section] = { ...this.state[section], ...patch };
      this.emit('change', { section, value: this.state[section] });
    } else {
      Object.assign(this.state, section);
      this.emit('change', { section: '*', value: this.state });
    }
  }

  set(section, value) {
    this.state[section] = value;
    this.emit('change', { section, value });
  }

  get(section) {
    if (section === undefined) return this.state;
    return this.state[section];
  }
}

module.exports = new Store();
