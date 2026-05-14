// Dashboard: overview of temps, levels, valves, pumps, and brew state.

function fmtTemp(t: number | undefined | null): string {
  if (t === undefined || t === null || Number.isNaN(t)) return '—';
  return `${t.toFixed(2)} °C`;
}
function fmtHms(s: number | undefined | null): string {
  const v = Math.max(0, Math.floor(s ?? 0));
  const hh = String(Math.floor(v / 3600)).padStart(2, '0');
  const mm = String(Math.floor((v % 3600) / 60)).padStart(2, '0');
  const ss = String(v % 60).padStart(2, '0');
  return `${hh}:${mm}:${ss}`;
}
function fmtMs(s: number | undefined | null): string {
  const v = Math.max(0, Math.floor(s ?? 0));
  const m = String(Math.floor(v / 60)).padStart(2, '0');
  const ss = String(v % 60).padStart(2, '0');
  return `${m}:${ss}`;
}

window.buildDashboardView = function (id: string): void {
  const root = document.getElementById(id);
  if (!root) return;
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

  function refresh(): void {
    const s = window.brewState;
    const setText = (id: string, v: string): void => {
      const e = document.getElementById(id);
      if (e) e.textContent = v;
    };
    const setHtml = (id: string, v: string): void => {
      const e = document.getElementById(id);
      if (e) e.innerHTML = v;
    };
    setText('dash-hlt-temp',  fmtTemp(s.temps?.HLT));
    setText('dash-mash-temp', fmtTemp(s.temps?.MASH));
    setText('dash-hlt-sp',    (s.hlt?.setpoint ?? 0).toFixed(1));
    setText('dash-hlt-level', s.hlt?.level ?? '—');
    setHtml('dash-hlt-heating',
      `<span class="led ${s.hlt?.heating ? 'warn' : ''}"></span>${s.hlt?.heating ? 'ON' : 'OFF'}`);
    setText('dash-water-mash',   `${(s.mashWater?.inMash ?? 0).toFixed(2)} L`);
    setText('dash-water-boiler', `${(s.mashWater?.inBoiler ?? 0).toFixed(2)} L`);
    setText('dash-boil-duty',    String(s.boil?.duty ?? 0));
    setText('dash-boil-state',   s.boil?.state ?? '—');
    setText('dash-boil-level',   s.boil?.level ?? '—');
    setText('dash-flow',         `${(s.flow?.boilLitres ?? 0).toFixed(3)} L`);

    const valveNames: Array<'HLT'|'MASH'|'INLET'|'CHILLER'> = ['HLT','MASH','INLET','CHILLER'];
    const rows = valveNames.map((n) =>
      `<span>${n} valve</span><b>${s.valves?.[n] ?? '—'}</b>`,
    ).concat([
      `<span>Mash pump</span><b>${s.pumps?.mash ?? '—'}</b>`,
      `<span>Chiller pump</span><b>${s.pumps?.chiller ?? '—'}</b>`,
      `<span>Boil valve</span><b>${s.boilValve ?? '—'}</b>`,
      `<span>Mill</span><b>${s.mill ?? '—'}</b>`,
      `<span>Stir</span><b>${s.stir ?? '—'}</b>`,
      `<span>Crane</span><b>${s.crane ?? '—'}</b>`,
    ]);
    const vs = document.getElementById('dash-valves');
    if (vs) vs.innerHTML = rows.join('');

    setText('dash-brew-step',   s.brew?.stepName ?? 'Idle');
    setText('dash-brew-stepno', String(s.brew?.step ?? 0));
    setText('dash-brew-max',    String(s.brew?.maxSteps ?? 0));
    setText('dash-brew-stepe',  fmtMs(s.brew?.stepElapsed));
    setText('dash-brew-total',  fmtHms(s.brew?.secondsElapsed));
  }
  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};

export {};
