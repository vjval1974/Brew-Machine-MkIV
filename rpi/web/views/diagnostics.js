// Diagnostics: equivalents of the original diag_menu (DS1820, Flow1, PWM,
// example applet) plus mock-hardware controls for development.
window.buildDiagnosticsView = function (id) {
  const root = document.getElementById(id);
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
        <p style="font-size: 12px; color: #8b949e;">
          Only meaningful when started with MOCK_HARDWARE=1.
        </p>
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

  // Mock helpers — they're a no-op when running on real hardware.
  let hltMidState = 1, hltHighState = 1;
  let mockHltTemp = 20;
  document.getElementById('diag-mock-hlt-mid').onclick = () => {
    hltMidState = hltMidState ? 0 : 1;
    window.brewSend('mock.setInput', { name: 'HLT_LEVEL_MID', value: hltMidState });
  };
  document.getElementById('diag-mock-hlt-high').onclick = () => {
    hltHighState = hltHighState ? 0 : 1;
    window.brewSend('mock.setInput', { name: 'HLT_LEVEL_HIGH', value: hltHighState });
  };
  document.getElementById('diag-mock-temp-hlt-up').onclick = () => {
    mockHltTemp += 1;
    window.brewSend('mock.setTemp', { name: 'HLT', value: mockHltTemp });
  };
  document.getElementById('diag-mock-temp-hlt-dn').onclick = () => {
    mockHltTemp -= 1;
    window.brewSend('mock.setTemp', { name: 'HLT', value: mockHltTemp });
  };
  document.getElementById('diag-mock-pulse-1').onclick   = () => window.brewSend('mock.injectPulses', { count: 1 });
  document.getElementById('diag-mock-pulse-100').onclick = () => window.brewSend('mock.injectPulses', { count: 100 });

  root.querySelectorAll('button[data-cmd]').forEach((b) =>
    b.addEventListener('click', () => window.brewSend(b.dataset.cmd))
  );

  function refresh() {
    const s = window.brewState;
    const ft = (t) => t == null || isNaN(t) ? '—' : `${t.toFixed(3)} °C`;
    document.getElementById('diag-hlt-temp').textContent  = ft(s.temps?.HLT);
    document.getElementById('diag-mash-temp').textContent = ft(s.temps?.MASH);
    document.getElementById('diag-flow-litres').textContent    = `${(s.flow?.boilLitres || 0).toFixed(3)} L`;
    document.getElementById('diag-flow-flowing').textContent   = s.flow?.flowing ? 'YES' : 'no';
    document.getElementById('diag-flow-measuring').textContent = s.flow?.measuring ? 'YES' : 'no';
    document.getElementById('diag-hlt-level').textContent      = s.hlt?.level || '—';
    document.getElementById('diag-hlt-heating').textContent    = s.hlt?.heating ? 'ON' : 'OFF';
  }
  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};
