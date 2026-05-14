'use strict';

// Brew parameter store with JSON persistence — port of parameters.c.
//
// On disk: rpi/data/parameters.json (created from config/parameters.default.json
// on first run). The legacy on-MCU "EEPROM" was a process-local struct; here
// we persist across reboots in the data/ directory.

const fs = require('fs');
const path = require('path');
const log = require('../util/logger');
const store = require('../state/store');

const DATA_DIR  = path.join(__dirname, '..', '..', 'data');
const DATA_FILE = path.join(DATA_DIR, 'parameters.json');
const DEFAULTS  = path.join(__dirname, '..', '..', 'config', 'parameters.default.json');

let cache = null;

function load() {
  if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });

  let defaults = {};
  try {
    defaults = JSON.parse(fs.readFileSync(DEFAULTS, 'utf8'));
  } catch (err) {
    log.warn(`No defaults at ${DEFAULTS}: ${err.message}`);
  }

  if (!fs.existsSync(DATA_FILE)) {
    fs.writeFileSync(DATA_FILE, JSON.stringify(defaults, null, 2));
    log.info(`Created ${DATA_FILE} from defaults`);
    cache = { ...defaults };
  } else {
    try {
      const onDisk = JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
      // forward-compat: merge missing keys from defaults
      cache = { ...defaults, ...onDisk };
    } catch (err) {
      log.error(`Failed to parse ${DATA_FILE} — using defaults. ${err.message}`);
      cache = { ...defaults };
    }
  }
  store.set('parameters', cache);
  return cache;
}

function save() {
  fs.writeFileSync(DATA_FILE, JSON.stringify(cache, null, 2));
  store.set('parameters', cache);
}

function get(key) {
  if (key === undefined) return cache;
  return cache[key];
}

function set(key, value) {
  cache[key] = value;
  save();
}

function setMany(patch) {
  Object.assign(cache, patch);
  save();
}

module.exports = { load, save, get, set, setMany };
