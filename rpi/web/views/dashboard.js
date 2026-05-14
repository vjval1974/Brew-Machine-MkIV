// Dashboard: overview of temps, levels, valves, pumps, and brew state.
window.buildDashboardView = function (id) {
  const root = document.getElementById(id);
  root.innerHTML = `
    <div class="grid cols-3">
      <div class="card">
        <h3>HLT</h3>
        <div class="readout" id="dash-hlt-temp">— °C</div>
        <div class="kv">
          <span>Setpoint</span><b id="dash-hlt-sp">—</b>
          <span>Level</span><b id="dash-hlt-level">—</b>
          <span>Heating</span><b id="dash-hlt-heating">—</b>
        </div>
      </div>
      <div class="card">
        <h3>Mash</h3>
        <div class="readout" id="dash-mash-temp">— °C</div>
        <div class="kv">
          <span>In Mash</span><b id="dash-water-mash">0.0 L</b>
          <span>In Boiler</span><b id="dash-water-boiler">0.0 L</b>
        </div>
      </div>
      <div class="card">
        <h3>Boil</h3>
        <div class="readout"><span id="dash-boil-duty">0</span>%</div>
        <div class="kv">
          <span>State</span><b id="dash-boil-state">—</b>
          <span>Level</span><b id="dash-boil-level">—</b>
          <span>Flow</span><b id="dash-flow">0.000 L</b>
        </div>
      </div>
    </div>

    <div class="card">
      <h3>Valves &amp; Pumps</h3>
      <div class="kv" id="dash-valves"></div>
    </div>

    <div class="card">
      <h3>Brew</h3>
      <div class="brew-progress">
        <span class="name" id="dash-brew-step">Idle</span>
        <span>Step <b id="dash-brew-stepno">0</b>/<span id="dash-brew-max">0</span></span>
        <span>Step time <b id="dash-brew-stepe">00:00</b></span>
        <span>Total <b id="dash-brew-total">00:00:00</b></span>
      </div>
    </div>
  `;

  function fmtTemp(t) { return t == null || isNaN(t) ? '—' : `${t.toFixed(2)} °C`; }
  function fmtHms(s)  {
    s = Math.max(0, Math.floor(s || 0));
    const hh = String(Math.floor(s / 3600)).padStart(2, '0');
    const mm = String(Math.floor((s % 3600) / 60)).padStart(2, '0');
    const ss = String(s % 60).padStart(2, '0');
    return `${hh}:${mm}:${ss}`;
  }
  function fmtMs(s)   {
    s = Math.max(0, Math.floor(s || 0));
    const m = String(Math.floor(s / 60)).padStart(2, '0');
    const ss = String(s % 60).padStart(2, '0');
    return `${m}:${ss}`;
  }

  function refresh() {
    const s = window.brewState;
    document.getElementById('dash-hlt-temp').textContent  = fmtTemp(s.temps?.HLT);
    document.getElementById('dash-mash-temp').textContent = fmtTemp(s.temps?.MASH);
    document.getElementById('dash-hlt-sp').textContent     = (s.hlt?.setpoint ?? 0).toFixed(1);
    document.getElementById('dash-hlt-level').textContent  = s.hlt?.level || '—';
    document.getElementById('dash-hlt-heating').innerHTML  =
      `<span class="led ${s.hlt?.heating ? 'warn' : ''}"></span>${s.hlt?.heating ? 'ON' : 'OFF'}`;
    document.getElementById('dash-water-mash').textContent   = (s.mashWater?.inMash || 0).toFixed(2) + ' L';
    document.getElementById('dash-water-boiler').textContent = (s.mashWater?.inBoiler || 0).toFixed(2) + ' L';
    document.getElementById('dash-boil-duty').textContent   = s.boil?.duty ?? 0;
    document.getElementById('dash-boil-state').textContent  = s.boil?.state || '—';
    document.getElementById('dash-boil-level').textContent  = s.boil?.level || '—';
    document.getElementById('dash-flow').textContent        = (s.flow?.boilLitres || 0).toFixed(3) + ' L';

    const vrows = ['HLT', 'MASH', 'INLET', 'CHILLER'].map((n) =>
      `<span>${n} valve</span><b>${s.valves?.[n] || '—'}</b>`
    ).concat([
      `<span>Mash pump</span><b>${s.pumps?.mash || '—'}</b>`,
      `<span>Chiller pump</span><b>${s.pumps?.chiller || '—'}</b>`,
      `<span>Boil valve</span><b>${s.boilValve || '—'}</b>`,
      `<span>Mill</span><b>${s.mill || '—'}</b>`,
      `<span>Stir</span><b>${s.stir || '—'}</b>`,
      `<span>Crane</span><b>${s.crane || '—'}</b>`,
    ]);
    document.getElementById('dash-valves').innerHTML = vrows.join('');

    document.getElementById('dash-brew-step').textContent    = s.brew?.stepName || 'Idle';
    document.getElementById('dash-brew-stepno').textContent  = s.brew?.step ?? 0;
    document.getElementById('dash-brew-max').textContent     = s.brew?.maxSteps ?? 0;
    document.getElementById('dash-brew-stepe').textContent   = fmtMs(s.brew?.stepElapsed);
    document.getElementById('dash-brew-total').textContent   = fmtHms(s.brew?.secondsElapsed);
  }

  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};
