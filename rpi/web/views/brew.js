// BREW view — START / PAUSE / RESUME / QUIT plus step list and live progress.
window.buildBrewView = function (id) {
  const root = document.getElementById(id);
  let steps = [];

  root.innerHTML = `
    <div class="card">
      <h3>Brew Control</h3>
      <div class="row">
        <button class="btn on lg" id="brew-start">START</button>
        <button class="btn warn lg" id="brew-pause">PAUSE</button>
        <button class="btn primary lg" id="brew-resume">RESUME</button>
        <button class="btn off lg" id="brew-quit">QUIT</button>
      </div>
    </div>

    <div class="card">
      <h3>Live</h3>
      <div class="brew-progress">
        <span class="name" id="brew-step-name">Idle</span>
        <span>Step <b id="brew-step-num">0</b> / <span id="brew-step-max">0</span></span>
        <span>Step time <b id="brew-step-elapsed">00:00</b></span>
        <span>Total <b id="brew-total">00:00:00</b></span>
      </div>
    </div>

    <div class="card">
      <h3>Steps</h3>
      <div id="brew-steps" class="param-list"></div>
    </div>
  `;

  document.getElementById('brew-start').onclick  = () => window.brewSend('brew.start');
  document.getElementById('brew-pause').onclick  = () => window.brewSend('brew.pause');
  document.getElementById('brew-resume').onclick = () => window.brewSend('brew.resume');
  document.getElementById('brew-quit').onclick   = () => {
    if (confirm('Quit brew? All controllers will be set to safe states.')) window.brewSend('brew.quit');
  };

  fetch('/api/brew/steps').then((r) => r.json()).then((s) => {
    steps = s;
    renderSteps();
  });

  function fmtMs(s) {
    s = Math.max(0, Math.floor(s || 0));
    const m = String(Math.floor(s / 60)).padStart(2, '0');
    const ss = String(s % 60).padStart(2, '0');
    return `${m}:${ss}`;
  }
  function fmtHms(s) {
    s = Math.max(0, Math.floor(s || 0));
    const hh = String(Math.floor(s / 3600)).padStart(2, '0');
    const mm = String(Math.floor((s % 3600) / 60)).padStart(2, '0');
    const ss = String(s % 60).padStart(2, '0');
    return `${hh}:${mm}:${ss}`;
  }

  function renderSteps() {
    const cur = window.brewState.brew?.step ?? -1;
    const el = document.getElementById('brew-steps');
    el.innerHTML = steps.map((s) => `
      <div class="param-row ${s.index === cur ? 'selected' : ''}" data-idx="${s.index}">
        <span>${s.index}. ${s.name}</span>
        <b>${s.index < cur ? 'done' : s.index === cur ? 'running' : ''}</b>
      </div>
    `).join('');
    el.querySelectorAll('.param-row').forEach((row) => {
      row.addEventListener('dblclick', () => {
        window.brewSend('brew.gotoStep', { index: Number(row.dataset.idx) });
      });
    });
  }

  function refresh() {
    const b = window.brewState.brew || {};
    document.getElementById('brew-step-name').textContent     = b.stepName || 'Idle';
    document.getElementById('brew-step-num').textContent      = b.step ?? 0;
    document.getElementById('brew-step-max').textContent      = b.maxSteps ?? 0;
    document.getElementById('brew-step-elapsed').textContent  = fmtMs(b.stepElapsed);
    document.getElementById('brew-total').textContent         = fmtHms(b.secondsElapsed);
    renderSteps();
  }
  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};
