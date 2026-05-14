// Manual Control: each sub-applet from the original menu becomes a card.
// Mirrors `manual_menu[]` in main.c (Mill / Crane / Stir / HopDropper / HLT / Boil).
window.buildManualView = function (id) {
  const root = document.getElementById(id);
  root.innerHTML = `
    <div class="grid cols-2">
      <div class="card">
        <h3>Mill</h3>
        <div class="readout" id="man-mill-state">stopped</div>
        <div class="row">
          <button class="btn on" data-cmd="mill.start">Start</button>
          <button class="btn off" data-cmd="mill.stop">Stop</button>
        </div>
      </div>

      <div class="card">
        <h3>Crane</h3>
        <div class="readout" id="man-crane-state">stopped</div>
        <div class="row">
          <button class="btn primary" data-cmd="crane.up">Up</button>
          <button class="btn primary" data-cmd="crane.down">Down</button>
          <button class="btn off"     data-cmd="crane.stop">Stop</button>
        </div>
      </div>

      <div class="card">
        <h3>Stir</h3>
        <div class="readout" id="man-stir-state">stopped</div>
        <div class="row">
          <button class="btn on"  data-cmd="stir.start">Start</button>
          <button class="btn off" data-cmd="stir.stop">Stop</button>
        </div>
      </div>

      <div class="card">
        <h3>Hop Dropper</h3>
        <div class="readout" id="man-hd-state">stopped</div>
        <div class="row">
          <button class="btn primary" data-cmd="hopDropper.drop">Index (one)</button>
          <button class="btn on"      data-cmd="hopDropper.start">Run</button>
          <button class="btn off"     data-cmd="hopDropper.stop">Stop</button>
        </div>
      </div>

      <div class="card">
        <h3>HLT (heating)</h3>
        <div class="readout"><span id="man-hlt-sp">—</span> °C setpoint</div>
        <div class="row">
          <button class="btn primary" data-cmd="hlt.bumpSetpoint" data-arg="-0.5">- 0.5</button>
          <button class="btn primary" data-cmd="hlt.bumpSetpoint" data-arg="0.5">+ 0.5</button>
          <button class="btn on"      data-cmd="hlt.startHeating">Start</button>
          <button class="btn off"     data-cmd="hlt.stopHeating">Stop</button>
        </div>
        <div class="kv" style="margin-top:8px;">
          <span>Current</span><b id="man-hlt-cur">—</b>
          <span>Level</span><b id="man-hlt-lvl">—</b>
          <span>Heating</span><b id="man-hlt-htg">—</b>
        </div>
      </div>

      <div class="card">
        <h3>Boil (duty)</h3>
        <div class="readout"><span id="man-boil-duty">0</span> %</div>
        <div class="row">
          <button class="btn primary" data-cmd="boil.bumpDuty" data-arg="-1">-1</button>
          <button class="btn primary" data-cmd="boil.bumpDuty" data-arg="1">+1</button>
          <button class="btn on"      data-cmd="boil.start">Start</button>
          <button class="btn off"     data-cmd="boil.stop">Stop</button>
        </div>
        <div class="kv" style="margin-top:8px;">
          <span>State</span><b id="man-boil-state">—</b>
          <span>Level</span><b id="man-boil-level">—</b>
        </div>
      </div>
    </div>
  `;

  root.querySelectorAll('button[data-cmd]').forEach((b) => {
    b.addEventListener('click', () => {
      const cmd = b.dataset.cmd;
      const arg = b.dataset.arg;
      if (cmd === 'hlt.bumpSetpoint')  return window.brewSend(cmd, { delta: Number(arg) });
      if (cmd === 'boil.bumpDuty')     return window.brewSend(cmd, { delta: Number(arg) });
      window.brewSend(cmd);
    });
  });

  function refresh() {
    const s = window.brewState;
    document.getElementById('man-mill-state').textContent  = s.mill || '—';
    document.getElementById('man-crane-state').textContent = s.crane || '—';
    document.getElementById('man-stir-state').textContent  = s.stir || '—';
    document.getElementById('man-hd-state').textContent    = s.hopDropper || '—';
    document.getElementById('man-hlt-sp').textContent      = (s.hlt?.setpoint ?? 0).toFixed(1);
    document.getElementById('man-hlt-cur').textContent     =
      (s.hlt?.temp == null || isNaN(s.hlt.temp)) ? '—' : s.hlt.temp.toFixed(2);
    document.getElementById('man-hlt-lvl').textContent     = s.hlt?.level || '—';
    document.getElementById('man-hlt-htg').textContent     = s.hlt?.heating ? 'ON' : 'OFF';
    document.getElementById('man-boil-duty').textContent   = s.boil?.duty ?? 0;
    document.getElementById('man-boil-state').textContent  = s.boil?.state || '—';
    document.getElementById('man-boil-level').textContent  = s.boil?.level || '—';
  }
  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};
