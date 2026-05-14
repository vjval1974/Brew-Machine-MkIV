// WebSocket client. Maintains a single `window.brewState` object that is
// updated by patch messages from the server. Views subscribe by calling
// `window.brewBus.on('change', cb)`.
window.brewState = {
  valves: {}, pumps: {}, hlt: {}, boil: {}, flow: {}, temps: {},
  mashWater: {}, brew: {}, parameters: {},
};

window.brewBus = (() => {
  const handlers = new Set();
  return {
    on: (cb) => handlers.add(cb),
    off: (cb) => handlers.delete(cb),
    emit: (e) => handlers.forEach((h) => { try { h(e); } catch (_) {} }),
  };
})();

(function () {
  function open() {
    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
    const ws = new WebSocket(`${proto}://${location.host}/ws`);
    window.brewWs = ws;

    ws.onopen = () => {
      document.getElementById('conn-status').className = 'pill ok';
      document.getElementById('conn-status').textContent = 'live';
    };
    ws.onclose = () => {
      document.getElementById('conn-status').className = 'pill bad';
      document.getElementById('conn-status').textContent = 'reconnecting…';
      setTimeout(open, 1000);
    };
    ws.onerror = () => {};
    ws.onmessage = (ev) => {
      const m = JSON.parse(ev.data);
      if (m.type === 'snapshot') {
        Object.assign(window.brewState, m.state);
        window.brewBus.emit({ type: 'snapshot' });
      } else if (m.type === 'patch') {
        if (m.section === '*') Object.assign(window.brewState, m.value);
        else window.brewState[m.section] = m.value;
        window.brewBus.emit({ type: 'patch', section: m.section });
      } else if (m.type === 'log') {
        const el = document.getElementById('log');
        const line = document.createElement('div');
        line.className = `line ${m.entry.level}`;
        line.textContent = `${new Date(m.entry.ts).toLocaleTimeString()} ${m.entry.level} ${m.entry.msg}`;
        el.appendChild(line);
        while (el.children.length > 60) el.removeChild(el.firstChild);
        el.scrollTop = el.scrollHeight;
      }
    };
  }
  open();

  window.brewSend = (name, args = {}) => {
    if (window.brewWs && window.brewWs.readyState === 1) {
      window.brewWs.send(JSON.stringify({ type: 'cmd', name, args }));
    }
  };
})();
