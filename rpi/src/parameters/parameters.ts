import fs from 'fs';
import path from 'path';
import log from '../util/logger';
import store from '../state/store';
import type { Parameters } from '../types';

const DATA_DIR  = path.join(__dirname, '..', '..', 'data');
const DATA_FILE = path.join(DATA_DIR, 'parameters.json');
const DEFAULTS  = path.join(__dirname, '..', '..', 'config', 'parameters.default.json');

let cache: Parameters | null = null;

export function load(): Parameters {
  if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });

  let defaults: Partial<Parameters> = {};
  try {
    defaults = JSON.parse(fs.readFileSync(DEFAULTS, 'utf8')) as Partial<Parameters>;
  } catch (err) {
    log.warn(`No defaults at ${DEFAULTS}: ${(err as Error).message}`);
  }

  if (!fs.existsSync(DATA_FILE)) {
    fs.writeFileSync(DATA_FILE, JSON.stringify(defaults, null, 2));
    log.info(`Created ${DATA_FILE} from defaults`);
    cache = { ...defaults } as Parameters;
  } else {
    try {
      const onDisk = JSON.parse(fs.readFileSync(DATA_FILE, 'utf8')) as Partial<Parameters>;
      cache = { ...defaults, ...onDisk } as Parameters;
    } catch (err) {
      log.error(`Failed to parse ${DATA_FILE} — using defaults. ${(err as Error).message}`);
      cache = { ...defaults } as Parameters;
    }
  }
  store.set('parameters', cache);
  return cache;
}

export function save(): void {
  if (!cache) throw new Error('parameters.load() must be called first');
  fs.writeFileSync(DATA_FILE, JSON.stringify(cache, null, 2));
  store.set('parameters', cache);
}

export function get(): Parameters;
export function get<K extends keyof Parameters>(key: K): Parameters[K];
export function get<K extends keyof Parameters>(key?: K): Parameters | Parameters[K] {
  if (!cache) throw new Error('parameters.load() must be called first');
  if (key === undefined) return cache;
  return cache[key];
}

export function set<K extends keyof Parameters>(key: K, value: Parameters[K]): void {
  if (!cache) throw new Error('parameters.load() must be called first');
  cache[key] = value;
  save();
}

export function setMany(patch: Partial<Parameters>): void {
  if (!cache) throw new Error('parameters.load() must be called first');
  Object.assign(cache, patch);
  save();
}

export default { load, save, get, set, setMany };
