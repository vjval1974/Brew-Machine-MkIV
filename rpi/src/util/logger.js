'use strict';

const { EventEmitter } = require('events');

const bus = new EventEmitter();
bus.setMaxListeners(50);

function ts() {
  return new Date().toISOString().slice(11, 23);
}

function emit(level, msg) {
  const line = `[${ts()}] ${level.padEnd(5)} ${msg}`;
  console.log(line);
  bus.emit('log', { ts: Date.now(), level, msg });
}

module.exports = {
  bus,
  info:  (msg) => emit('INFO',  msg),
  warn:  (msg) => emit('WARN',  msg),
  error: (msg) => emit('ERROR', msg),
  debug: (msg) => { if (process.env.DEBUG) emit('DEBUG', msg); },
};
