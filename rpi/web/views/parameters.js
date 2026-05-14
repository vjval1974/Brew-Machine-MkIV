// Parameters editor — port of parameters.c's UI. Replaces the numpad-style
// data entry with a list + on-screen keypad. Press SUBMIT to commit.
window.buildParametersView = function (id) {
  const root = document.getElementById(id);

  // Keep this list in sync with `UserParametersList[]` in parameters.c.
  const FIELDS = [
    { key: 'iGrindTime',         type: 'int',   label: 'Milling Time (min)' },
    { key: 'fStrikeTemp',        type: 'float', label: 'Strike Temp (°C)' },
    { key: 'fStrikeLitres',      type: 'float', label: 'Strike Litres' },
    { key: 'fSpargeLitres',      type: 'float', label: 'Sparge Litres' },
    { key: 'iMashTime',          type: 'int',   label: 'Mash Time (min)' },
    { key: 'iSpargeTime',        type: 'int',   label: 'Sparge Time (min)' },
    { key: 'uiBoilTime',         type: 'int',   label: 'Boil Time (min)' },
    { key: 'uiHopTimes.0',       type: 'int',   label: 'Hop Time 1' },
    { key: 'uiHopTimes.1',       type: 'int',   label: 'Hop Time 2' },
    { key: 'uiHopTimes.2',       type: 'int',   label: 'Hop Time 3' },
    { key: 'uiHopTimes.3',       type: 'int',   label: 'Hop Time 4' },
    { key: 'uiHopTimes.4',       type: 'int',   label: 'Hop Time 5' },
    { key: 'uiHopTimes.5',       type: 'int',   label: 'Hop Time 6' },
    { key: 'uiChillTime',        type: 'int',   label: 'Chill Time (min)' },
    { key: 'uiPumpToFermenterTime', type: 'int',label: 'Pump To Fermenter (min)' },
    { key: 'fSpargeTemp',        type: 'float', label: 'Sparge Temp 1 (°C)' },
    { key: 'fSpargeTemp2',       type: 'float', label: 'Sparge Temp 2 (°C)' },
    { key: 'fSpargeTemp3',       type: 'float', label: 'Sparge Temp 3 (°C)' },
    { key: 'iMashOutTime',       type: 'int',   label: 'Mash Out Time (min)' },
    { key: 'fMashOutLitres',     type: 'float', label: 'Mash Out Litres' },
    { key: 'fMashOutTemp',       type: 'float', label: 'Mash Out Temp (°C)' },
    { key: 'iMashStage2Time',    type: 'int',   label: 'Mash Stage 2 Time (min)' },
    { key: 'fMashStage2Litres',  type: 'float', label: 'Mash Stage 2 Litres' },
    { key: 'fMashStage2Temp',    type: 'float', label: 'Mash Stage 2 Temp (°C)' },
    { key: 'uiCurrentMashStage', type: 'int',   label: 'Current Mash Stage' },
    { key: 'uiHopDropperStopDelayms', type: 'int', label: 'Hop Dropper Delay (ms)' },
    { key: 'fGrainWeightKilos',  type: 'float', label: 'Grain Weight (kg)' },
  ];

  let selectedIdx = 0;
  let input = '';

  root.innerHTML = `
    <div class="grid cols-2">
      <div class="card">
        <h3>Parameters</h3>
        <div class="param-list" id="param-list"></div>
      </div>
      <div class="card">
        <h3>Editor</h3>
        <div class="kv">
          <span>Selected</span><b id="param-sel-label">—</b>
          <span>Current</span><b id="param-sel-cur">—</b>
        </div>
        <div style="margin: 12px 0;">
          <input class="numeric" id="param-input" readonly value="">
        </div>
        <div class="numpad">
          <button class="btn">1</button><button class="btn">2</button><button class="btn">3</button>
          <button class="btn">4</button><button class="btn">5</button><button class="btn">6</button>
          <button class="btn">7</button><button class="btn">8</button><button class="btn">9</button>
          <button class="btn">0</button><button class="btn">.</button><button class="btn off" data-key="C">C</button>
        </div>
        <div class="row" style="margin-top: 10px;">
          <button class="btn primary" id="param-prev">Prev</button>
          <button class="btn primary" id="param-next">Next</button>
          <button class="btn on"      id="param-submit">Submit</button>
        </div>
      </div>
    </div>
  `;

  const listEl = document.getElementById('param-list');
  const inputEl = document.getElementById('param-input');

  function getValue(s, dottedKey) {
    if (!dottedKey.includes('.')) return s[dottedKey];
    const [k, i] = dottedKey.split('.');
    return s?.[k]?.[Number(i)];
  }

  function renderList() {
    listEl.innerHTML = FIELDS.map((f, i) => {
      const v = getValue(window.brewState.parameters || {}, f.key);
      const sel = i === selectedIdx ? 'selected' : '';
      return `<div class="param-row ${sel}" data-idx="${i}">
        <span>${f.label}</span><b>${v ?? '—'}</b>
      </div>`;
    }).join('');
    listEl.querySelectorAll('.param-row').forEach((row) => {
      row.addEventListener('click', () => {
        selectedIdx = Number(row.dataset.idx);
        renderList(); renderSelected();
      });
    });
  }
  function renderSelected() {
    const f = FIELDS[selectedIdx];
    const v = getValue(window.brewState.parameters || {}, f.key);
    document.getElementById('param-sel-label').textContent = f.label;
    document.getElementById('param-sel-cur').textContent   = (v ?? '—') + '';
  }

  root.querySelectorAll('.numpad .btn').forEach((b) => {
    b.addEventListener('click', () => {
      const c = b.dataset.key || b.textContent;
      if (c === 'C') { input = ''; }
      else if (c === '.' && input.includes('.')) { /* ignore */ }
      else { input += c; }
      inputEl.value = input;
    });
  });
  document.getElementById('param-prev').onclick = () => {
    selectedIdx = (selectedIdx - 1 + FIELDS.length) % FIELDS.length;
    renderList(); renderSelected();
  };
  document.getElementById('param-next').onclick = () => {
    selectedIdx = (selectedIdx + 1) % FIELDS.length;
    renderList(); renderSelected();
  };
  document.getElementById('param-submit').onclick = () => {
    if (input === '') return;
    const f = FIELDS[selectedIdx];
    let v = f.type === 'int' ? parseInt(input, 10) : parseFloat(input);
    if (isNaN(v)) return;
    // Flatten dotted key like uiHopTimes.0 → patch a copy of the array
    if (f.key.includes('.')) {
      const [k, i] = f.key.split('.');
      const arr = [...(window.brewState.parameters?.[k] || [])];
      arr[Number(i)] = v;
      window.brewSend('params.setMany', { values: { [k]: arr } });
    } else {
      window.brewSend('params.set', { key: f.key, value: v });
    }
    input = ''; inputEl.value = '';
  };

  window.brewBus.on(() => { renderList(); renderSelected(); });
  renderList(); renderSelected();
};
