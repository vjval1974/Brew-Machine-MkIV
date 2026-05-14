// WebSocket client. Maintains a single `window.brewState` object updated by
// patch messages from the server. Views subscribe via `window.brewBus.on(cb)`.

import type { AppStateLite, BrewBus, BrewBusEvent, WsServerMessage } from './types';

window.brewState = {
  valves: {}, pumps: {}, hlt: {}, boil: {}, flow: {}, temps: {},
  mashWater: {}, brew: {}, parameters: {},
};

const bus: BrewBus = ((): BrewBus => {
  const handlers = new Set<(e: BrewBusEvent) => void>();
  return {
    on:  (cb) => { handlers.add(cb); },
    off: (cb) => { handlers.delete(cb); },
    emit: (e)  => { handlers.forEach((h) => { try { h(e); } catch { /* ignore */ } }); },
  };
})();
window.brewBus = bus;

function open(): void {
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const ws = new WebSocket(`${proto}://${location.host}/ws`);
  window.brewWs = ws;

  ws.onopen = (): void => {
    const el = document.getElementById('conn-status');
    if (el) { el.className = 'pill ok'; el.textContent = 'live'; }
  };
  ws.onclose = (): void => {
    const el = document.getElementById('conn-status');
    if (el) { el.className = 'pill bad'; el.textContent = 'reconnecting…'; }
    setTimeout(open, 1000);
  };
  ws.onerror = (): void => { /* close handler will re-open */ };
  ws.onmessage = (ev: MessageEvent): void => {
    const m = JSON.parse(ev.data as string) as WsServerMessage;
    if (m.type === 'snapshot') {
      Object.assign(window.brewState, m.state);
      bus.emit({ type: 'snapshot' });
    } else if (m.type === 'patch') {
      if (m.section === '*') Object.assign(window.brewState, m.value as AppStateLite);
      else (window.brewState as Record<string, unknown>)[m.section] = m.value;
      bus.emit({ type: 'patch', section: m.section });
    } else if (m.type === 'log') {
      const el = document.getElementById('log');
      if (!el) return;
      const line = document.createElement('div');
      line.className = `line ${m.entry.level}`;
      line.textContent = `${new Date(m.entry.ts).toLocaleTimeString()} ${m.entry.level} ${m.entry.msg}`;
      el.appendChild(line);
      while (el.children.length > 60) el.removeChild(el.firstChild as Node);
      el.scrollTop = el.scrollHeight;
    }
  };
}
open();

window.brewSend = (name: string, args: Record<string, unknown> = {}): void => {
  const ws = window.brewWs;
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'cmd', name, args }));
  }
};

export {};
