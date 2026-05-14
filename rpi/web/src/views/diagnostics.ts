// Diagnostics view: live sensor readouts + mock-hardware controls.

window.buildDiagnosticsView = function (id: string): void {
  const root = document.getElementById(id);
  if (!root) return;
  root.innerHTML = `
    <div class="grid cols-2">
      <div class="card">
        <h3>Temperature sensors (DS18B20)</h3>
        <div class="kv">
          <span>HLT (28-c652b6...)</span><b id="diag-hlt-temp">—</b>
          <span>MASH (28-d7c6b5...)</span><b id="diag-mash-temp">—</b>
        </div>
      </div>
      <div class="card">
        <h3>Flow sensor (BOIL)</h3>
        <div class="kv">
          <span>Litres</span><b id="diag-flow-litres">0.000 L</b>
          <span>Flowing</span><b id="diag-flow-flowing">—</b>
          <span>Measuring</span><b id="diag-flow-measuring">—</b>
        </div>
        <div class="row" style="margin-top:8px;">
          <button class="btn primary" data-cmd="flow.reset">Reset</button>
        </div>
      </div>
      <div class="card">
        <h3>HLT levels</h3>
        <div class="kv">
          <span>Level</span><b id="diag-hlt-level">—</b>
          <span>SSR / heating</span><b id="diag-hlt-heating">—</b>
        </div>
      </div>
      <div class="card">
        <h3>Mock hardware</h3>
        <p style="font-size: 12px; color: #8b949e;">Only meaningful when started with MOCK_HARDWARE=1.</p>
        <div class="grid cols-2">
          <button class="btn warn" id="diag-mock-hlt-mid">Toggle HLT MID</button>
          <button class="btn warn" id="diag-mock-hlt-high">Toggle HLT HIGH</button>
          <button class="btn warn" id="diag-mock-temp-hlt-up">HLT temp +1 °C</button>
          <button class="btn warn" id="diag-mock-temp-hlt-dn">HLT temp -1 °C</button>
          <button class="btn warn" id="diag-mock-pulse-1">Inject 1 flow pulse</button>
          <button class="btn warn" id="diag-mock-pulse-100">Inject 100 pulses</button>
        </div>
      </div>
    </div>
  `;

  let hltMidState = 1, hltHighState = 1;
  let mockHltTemp = 20;
  document.getElementById('diag-mock-hlt-mid')?.addEventListener('click', () => {
    hltMidState = hltMidState ? 0 : 1;
    window.brewSend('mock.setInput', { name: 'HLT_LEVEL_MID', value: hltMidState });
  });
  document.getElementById('diag-mock-hlt-high')?.addEventListener('click', () => {
    hltHighState = hltHighState ? 0 : 1;
    window.brewSend('mock.setInput', { name: 'HLT_LEVEL_HIGH', value: hltHighState });
  });
  document.getElementById('diag-mock-temp-hlt-up')?.addEventListener('click', () => {
    mockHltTemp += 1;
    window.brewSend('mock.setTemp', { name: 'HLT', value: mockHltTemp });
  });
  document.getElementById('diag-mock-temp-hlt-dn')?.addEventListener('click', () => {
    mockHltTemp -= 1;
    window.brewSend('mock.setTemp', { name: 'HLT', value: mockHltTemp });
  });
  document.getElementById('diag-mock-pulse-1')?.addEventListener('click',   () => window.brewSend('mock.injectPulses', { count: 1 }));
  document.getElementById('diag-mock-pulse-100')?.addEventListener('click', () => window.brewSend('mock.injectPulses', { count: 100 }));

  root.querySelectorAll<HTMLButtonElement>('button[data-cmd]').forEach((b) =>
    b.addEventListener('click', () => { if (b.dataset.cmd) window.brewSend(b.dataset.cmd); }),
  );

  function refresh(): void {
    const s = window.brewState;
    const ft = (t: number | undefined | null): string =>
      t == null || Number.isNaN(t) ? '—' : `${t.toFixed(3)} °C`;
    const set = (id: string, v: string): void => {
      const e = document.getElementById(id);
      if (e) e.textContent = v;
    };
    set('diag-hlt-temp',       ft(s.temps?.HLT));
    set('diag-mash-temp',      ft(s.temps?.MASH));
    set('diag-flow-litres',    `${(s.flow?.boilLitres ?? 0).toFixed(3)} L`);
    set('diag-flow-flowing',   s.flow?.flowing   ? 'YES' : 'no');
    set('diag-flow-measuring', s.flow?.measuring ? 'YES' : 'no');
    set('diag-hlt-level',      s.hlt?.level ?? '—');
    set('diag-hlt-heating',    s.hlt?.heating ? 'ON' : 'OFF');
  }
  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};

export {};
