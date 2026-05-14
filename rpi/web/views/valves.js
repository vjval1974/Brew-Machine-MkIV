// Valves & Pumps: same layout as the original valves applet in valves.c.
window.buildValvesView = function (id) {
  const root = document.getElementById(id);
  const valves = [
    { name: 'HLT', open: 'To Mash', closed: 'To HLT' },
    { name: 'MASH', open: 'To Boil', closed: 'To Mash' },
    { name: 'INLET', open: 'Filling', closed: 'Closed' },
    { name: 'CHILLER', open: 'Open', closed: 'Closed' },
  ];
  const others = [
    { id: 'BoilValve',    label: 'Boil Valve',  cmd: 'boilValve.toggle' },
    { id: 'MashPump',     label: 'Mash Pump',   cmd: 'mashPump.toggle' },
    { id: 'ChillerPump',  label: 'Chiller/Boil Pump', cmd: 'chillerPump.toggle' },
    { id: 'FlowReset',    label: 'Reset Flow',  cmd: 'flow.reset', alwaysOff: true },
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

  root.querySelectorAll('button[data-valve]').forEach((b) => {
    b.addEventListener('click', () => {
      window.brewSend('valve.toggle', { name: b.dataset.valve });
    });
  });
  root.querySelectorAll('button[data-other]').forEach((b) => {
    b.addEventListener('click', () => window.brewSend(b.dataset.cmd));
  });

  function refresh() {
    const s = window.brewState;
    for (const v of valves) {
      const open = s.valves?.[v.name] === 'open';
      const btn = root.querySelector(`button[data-valve="${v.name}"]`);
      const lab = root.querySelector(`[data-state-for="${v.name}"]`);
      if (btn) btn.className = `btn ${open ? 'on' : 'off'}`;
      if (lab) lab.textContent = open ? v.open : v.closed;
    }
    const mashP    = s.pumps?.mash === 'pumping';
    const chillerP = s.pumps?.chiller === 'pumping';
    const bv       = s.boilValve;
    const setOther = (key, label, on) => {
      const b = root.querySelector(`[data-other="${key}"]`);
      const l = root.querySelector(`[data-other-state="${key}"]`);
      if (b) b.className = `btn ${on ? 'on' : 'off'}`;
      if (l) l.textContent = label;
    };
    setOther('MashPump',    mashP    ? 'Pumping' : 'Stopped', mashP);
    setOther('ChillerPump', chillerP ? 'Pumping' : 'Stopped', chillerP);
    setOther('BoilValve',   bv || '—', bv === 'opened');
    setOther('FlowReset',   'Reset',   false);

    document.getElementById('vlv-hlt-temp').textContent  =
      s.temps?.HLT == null || isNaN(s.temps.HLT) ? '—' : `${s.temps.HLT.toFixed(2)} °C`;
    document.getElementById('vlv-mash-temp').textContent =
      s.temps?.MASH == null || isNaN(s.temps.MASH) ? '—' : `${s.temps.MASH.toFixed(2)} °C`;
    document.getElementById('vlv-flow').textContent      =
      `${(s.flow?.boilLitres || 0).toFixed(3)} L`;
  }

  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};
