import { EventEmitter } from 'events';
import type { LogEntry, LogLevel } from '../../types';

export const bus = new EventEmitter();
bus.setMaxListeners(50);

function ts(): string {
  return new Date().toISOString().slice(11, 23);
}

function emit(level: LogLevel, msg: string): void {
  const line = `[${ts()}] ${level.padEnd(5)} ${msg}`;
  // eslint-disable-next-line no-console
  console.log(line);
  const entry: LogEntry = { ts: Date.now(), level, msg };
  bus.emit('log', entry);
}

export const info  = (msg: string): void => emit('INFO',  msg);
export const warn  = (msg: string): void => emit('WARN',  msg);
export const error = (msg: string): void => emit('ERROR', msg);
export const debug = (msg: string): void => { if (process.env.DEBUG) emit('DEBUG', msg); };

export default { bus, info, warn, error, debug };
