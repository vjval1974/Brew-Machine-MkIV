// Valves & Pumps view — same layout as the original valves applet in valves.c.

import type { ValveName } from '../types';

interface ValveDef { name: ValveName; open: string; closed: string }
interface OtherDef { id: string; label: string; cmd: string }

window.buildValvesView = function (id: string): void {
  const root = document.getElementById(id);
  if (!root) return;

  const valves: ValveDef[] = [
    { name: 'HLT',     open: 'To Mash', closed: 'To HLT' },
    { name: 'MASH',    open: 'To Boil', closed: 'To Mash' },
    { name: 'INLET',   open: 'Filling', closed: 'Closed' },
    { name: 'CHILLER', open: 'Open',    closed: 'Closed' },
  ];
  const others: OtherDef[] = [
    { id: 'BoilValve',    label: 'Boil Valve',         cmd: 'boilValve.toggle' },
    { id: 'MashPump',     label: 'Mash Pump',          cmd: 'mashPump.toggle' },
    { id: 'ChillerPump',  label: 'Chiller/Boil Pump',  cmd: 'chillerPump.toggle' },
    { id: 'FlowReset',    label: 'Reset Flow',         cmd: 'flow.reset' },
  ];

  root.innerHTML = `
    <div class="card">
      <h3>Valves</h3>
      <div class="grid cols-4">
        ${valves.map((v) => `
          <button class="btn off" data-valve="${v.name}">
            <span>${v.name}</span><br><span class="state" data-state-for="${v.name}">${v.closed}</span>
          </button>
        `).join('')}
      </div>
    </div>
    <div class="card">
      <h3>Pumps / Boil Valve</h3>
      <div class="grid cols-4">
        ${others.map((o) => `
          <button class="btn off" data-other="${o.id}" data-cmd="${o.cmd}">
            <span>${o.label}</span><br><span class="state" data-other-state="${o.id}">—</span>
          </button>
        `).join('')}
      </div>
    </div>
    <div class="card">
      <h3>Readouts</h3>
      <div class="kv">
        <span>HLT temp</span><b id="vlv-hlt-temp">—</b>
        <span>Mash temp</span><b id="vlv-mash-temp">—</b>
        <span>Boil flow</span><b id="vlv-flow">0.000 L</b>
      </div>
    </div>
  `;

  root.querySelectorAll<HTMLButtonElement>('button[data-valve]').forEach((b) => {
    b.addEventListener('click', () => {
      const name = b.dataset.valve;
      if (name) window.brewSend('valve.toggle', { name });
    });
  });
  root.querySelectorAll<HTMLButtonElement>('button[data-other]').forEach((b) => {
    b.addEventListener('click', () => { if (b.dataset.cmd) window.brewSend(b.dataset.cmd); });
  });

  function refresh(): void {
    const s = window.brewState;
    for (const v of valves) {
      const open = s.valves?.[v.name] === 'open';
      const btn = root!.querySelector<HTMLButtonElement>(`button[data-valve="${v.name}"]`);
      const lab = root!.querySelector<HTMLElement>(`[data-state-for="${v.name}"]`);
      if (btn) btn.className = `btn ${open ? 'on' : 'off'}`;
      if (lab) lab.textContent = open ? v.open : v.closed;
    }
    const mashP    = s.pumps?.mash === 'pumping';
    const chillerP = s.pumps?.chiller === 'pumping';
    const bv       = s.boilValve;
    const setOther = (key: string, label: string, on: boolean): void => {
      const b = root!.querySelector<HTMLButtonElement>(`[data-other="${key}"]`);
      const l = root!.querySelector<HTMLElement>(`[data-other-state="${key}"]`);
      if (b) b.className = `btn ${on ? 'on' : 'off'}`;
      if (l) l.textContent = label;
    };
    setOther('MashPump',    mashP    ? 'Pumping' : 'Stopped', mashP);
    setOther('ChillerPump', chillerP ? 'Pumping' : 'Stopped', chillerP);
    setOther('BoilValve',   bv ?? '—', bv === 'opened');
    setOther('FlowReset',   'Reset',   false);

    const setText = (id: string, v: string): void => {
      const e = document.getElementById(id);
      if (e) e.textContent = v;
    };
    const ht = s.temps?.HLT, mt = s.temps?.MASH;
    setText('vlv-hlt-temp',  ht == null || Number.isNaN(ht) ? '—' : `${ht.toFixed(2)} °C`);
    setText('vlv-mash-temp', mt == null || Number.isNaN(mt) ? '—' : `${mt.toFixed(2)} °C`);
    setText('vlv-flow',      `${(s.flow?.boilLitres ?? 0).toFixed(3)} L`);
  }
  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};

export {};
