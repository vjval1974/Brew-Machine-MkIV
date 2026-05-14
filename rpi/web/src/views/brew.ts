// BREW view — START / PAUSE / RESUME / QUIT plus step list and live progress.

interface StepInfo { index: number; name: string; wait: boolean }

function fmtMs(s: number | undefined | null): string {
  const v = Math.max(0, Math.floor(s ?? 0));
  const m = String(Math.floor(v / 60)).padStart(2, '0');
  const ss = String(v % 60).padStart(2, '0');
  return `${m}:${ss}`;
}
function fmtHms(s: number | undefined | null): string {
  const v = Math.max(0, Math.floor(s ?? 0));
  const hh = String(Math.floor(v / 3600)).padStart(2, '0');
  const mm = String(Math.floor((v % 3600) / 60)).padStart(2, '0');
  const ss = String(v % 60).padStart(2, '0');
  return `${hh}:${mm}:${ss}`;
}

window.buildBrewView = function (id: string): void {
  const root = document.getElementById(id);
  if (!root) return;
  let steps: StepInfo[] = [];

  root.innerHTML = `
    <div class="card">
      <h3>Brew Control</h3>
      <div class="row">
        <button class="btn on lg"      id="brew-start">START</button>
        <button class="btn warn lg"    id="brew-pause">PAUSE</button>
        <button class="btn primary lg" id="brew-resume">RESUME</button>
        <button class="btn off lg"     id="brew-quit">QUIT</button>
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
      <p style="font-size: 12px; color: #8b949e;">Bold = waits for prior steps to complete before starting. Others fire in parallel.</p>
      <div id="brew-steps" class="param-list"></div>
    </div>
  `;

  document.getElementById('brew-start')?.addEventListener('click',  () => window.brewSend('brew.start'));
  document.getElementById('brew-pause')?.addEventListener('click',  () => window.brewSend('brew.pause'));
  document.getElementById('brew-resume')?.addEventListener('click', () => window.brewSend('brew.resume'));
  document.getElementById('brew-quit')?.addEventListener('click', () => {
    if (confirm('Quit brew? All controllers will be set to safe states.')) window.brewSend('brew.quit');
  });

  fetch('/api/brew/steps').then((r) => r.json() as Promise<StepInfo[]>).then((s) => {
    steps = s;
    renderSteps();
  }).catch(() => { /* server down; will retry naturally on next refresh */ });

  function renderSteps(): void {
    const cur = window.brewState.brew?.step ?? -1;
    const el = document.getElementById('brew-steps');
    if (!el) return;
    el.innerHTML = steps.map((s) => `
      <div class="param-row ${s.index === cur ? 'selected' : ''}" data-idx="${s.index}">
        <span style="${s.wait ? 'font-weight:700;' : ''}">${s.index}. ${s.name}${s.wait ? ' ⏳' : ''}</span>
        <b>${s.index < cur ? 'done' : s.index === cur ? 'running' : ''}</b>
      </div>
    `).join('');
    el.querySelectorAll<HTMLDivElement>('.param-row').forEach((row) => {
      row.addEventListener('dblclick', () => {
        const idx = Number(row.dataset.idx);
        window.brewSend('brew.gotoStep', { index: idx });
      });
    });
  }

  function refresh(): void {
    const b = window.brewState.brew ?? {};
    const set = (id: string, v: string): void => {
      const e = document.getElementById(id);
      if (e) e.textContent = v;
    };
    set('brew-step-name',    b.stepName ?? 'Idle');
    set('brew-step-num',     String(b.step ?? 0));
    set('brew-step-max',     String(b.maxSteps ?? 0));
    set('brew-step-elapsed', fmtMs(b.stepElapsed));
    set('brew-total',        fmtHms(b.secondsElapsed));
    renderSteps();
  }
  window.brewBus.on(refresh);
  setInterval(refresh, 500);
};

export {};
