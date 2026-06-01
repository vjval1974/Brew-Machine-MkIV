// Diagnostics view: live sensor readouts + 1-Wire calibration + mock-hardware controls.

interface ScannedDevice {
  rom:        string;
  temp:       number | null;
  assignedTo: string[];
}
interface OnewireResponse {
  sensors: Record<string, string>;     // logical name → ROM
  devices: ScannedDevice[];
}

let owState: OnewireResponse = { sensors: {}, devices: [] };
let owLoading = false;

function escape(s: string): string {
  return s.replace(/[&<>"']/g, (c) =>
    c === '&' ? '&amp;' :
    c === '<' ? '&lt;' :
    c === '>' ? '&gt;' :
    c === '"' ? '&quot;' :
                '&#39;');
}

function fmtTemp(t: number | null | undefined): string {
  if (t == null || Number.isNaN(t)) return '—';
  return `${t.toFixed(2)} °C`;
}

async function refreshOnewire(): Promise<void> {
  if (owLoading) return;
  owLoading = true;
  try {
    const res = await fetch('/api/onewire');
    owState = (await res.json()) as OnewireResponse;
    renderOnewire();
  } catch (err) {
    console.error('onewire fetch:', err);
  } finally {
    owLoading = false;
  }
}

async function scanOnewire(): Promise<void> {
  owLoading = true;
  renderOnewire();                       // show spinner state immediately
  try {
    const res = await fetch('/api/onewire/scan', { method: 'POST' });
    owState = (await res.json()) as OnewireResponse;
  } catch (err) {
    console.error('onewire scan:', err);
  } finally {
    owLoading = false;
    renderOnewire();
  }
}

function renderOnewire(): void {
  const root = document.getElementById('diag-onewire-body');
  if (!root) return;

  // Two columns: current assignments + discovered devices.
  const assignments = Object.entries(owState.sensors);
  const knownLogicalNames = assignments.map(([n]) => n);
  if (knownLogicalNames.length === 0) knownLogicalNames.push('HLT', 'MASH');

  const assignedHtml = assignments.length === 0
    ? '<p style="color:#8b949e;font-size:13px;">No sensor assignments configured.</p>'
    : `<div class="kv">${assignments.map(([name, rom]) => `
        <span>${escape(name)}</span>
        <b style="font-family:ui-monospace,monospace;font-size:12px;color:#79c0ff;">${escape(rom)}</b>
      `).join('')}</div>
      <div class="row" style="margin-top:8px;">
        <button class="btn off" id="diag-ow-reset" style="min-height:36px;font-size:12px;">Reset all to defaults</button>
      </div>`;

  const devicesHtml = owState.devices.length === 0
    ? (owLoading
        ? '<p style="color:#8b949e;font-size:13px;">scanning…</p>'
        : '<p style="color:#8b949e;font-size:13px;">No devices found. Wire a DS18B20 to the 1-Wire bus and click <b>Scan bus</b>.</p>')
    : `<table style="width:100%;border-collapse:collapse;font-size:13px;">
        <thead>
          <tr style="color:#ffb454;text-align:left;border-bottom:1px solid #1f2733;">
            <th style="padding:4px 6px;">ROM</th>
            <th style="padding:4px 6px;text-align:right;">Temp</th>
            <th style="padding:4px 6px;">Assigned</th>
            <th style="padding:4px 6px;">Assign to →</th>
          </tr>
        </thead>
        <tbody>
        ${owState.devices.map((d) => `
          <tr style="border-bottom:1px solid #1f2733;">
            <td style="padding:6px;font-family:ui-monospace,monospace;font-size:11px;color:#79c0ff;">${escape(d.rom)}</td>
            <td style="padding:6px;text-align:right;color:${d.temp == null ? '#ff7b72' : '#e6edf3'};">${fmtTemp(d.temp)}</td>
            <td style="padding:6px;color:#7ee787;">${d.assignedTo.map(escape).join(', ') || '—'}</td>
            <td style="padding:6px;">
              ${knownLogicalNames.map((n) => `
                <button class="btn ${d.assignedTo.includes(n) ? 'on' : 'primary'}"
                        data-assign-rom="${escape(d.rom)}" data-assign-name="${escape(n)}"
                        style="min-height:30px;font-size:12px;padding:4px 8px;margin-right:4px;">
                  ${escape(n)}
                </button>
              `).join('')}
            </td>
          </tr>
        `).join('')}
        </tbody>
      </table>`;

  root.innerHTML = `
    <div style="margin-bottom:10px;">
      <h4 style="margin:0 0 6px 0;color:#c0caf5;font-size:13px;text-transform:uppercase;letter-spacing:.05em;">Current assignments</h4>
      ${assignedHtml}
    </div>
    <div>
      <div style="display:flex;align-items:center;gap:8px;margin-bottom:6px;">
        <h4 style="margin:0;color:#c0caf5;font-size:13px;text-transform:uppercase;letter-spacing:.05em;flex:1;">Discovered devices ${owLoading ? '<span style="color:#ffb454;">(scanning…)</span>' : ''}</h4>
        <button class="btn primary" id="diag-ow-scan" style="min-height:34px;font-size:13px;">Scan bus</button>
      </div>
      ${devicesHtml}
    </div>
  `;

  document.getElementById('diag-ow-scan')?.addEventListener('click', () => { void scanOnewire(); });
  document.getElementById('diag-ow-reset')?.addEventListener('click', () => {
    if (confirm('Clear ALL sensor overrides and revert to pinmap defaults?')) {
      void fetch('/api/onewire/overrides', { method: 'DELETE' }).then(refreshOnewire);
    }
  });
  root.querySelectorAll<HTMLButtonElement>('[data-assign-rom]').forEach((b) => {
    b.addEventListener('click', async () => {
      const rom  = b.dataset.assignRom!;
      const name = b.dataset.assignName!;
      await fetch(`/api/onewire/sensor/${encodeURIComponent(name)}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ rom }),
      });
      void refreshOnewire();
    });
  });
}

window.buildDiagnosticsView = function (id: string): void {
  const root = document.getElementById(id);
  if (!root) return;
  root.innerHTML = `
    <div class="grid cols-2">
      <div class="card">
        <h3>Temperature sensors (DS18B20)</h3>
        <div class="kv">
          <span id="diag-hlt-label">HLT</span><b id="diag-hlt-temp">—</b>
          <span id="diag-mash-label">MASH</span><b id="diag-mash-temp">—</b>
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

      <div class="card" style="grid-column: 1 / -1;">
        <h3>1-Wire bus (DS18B20 discovery + assignment)</h3>
        <p style="font-size:12px;color:#8b949e;margin:0 0 8px 0;">
          Reads <code style="color:#79c0ff;">/sys/bus/w1/devices</code>. Wire a new probe, click
          <b>Scan bus</b> to list ROMs + their live temps, then click an HLT/MASH button to assign.
          Assignments persist to <code style="color:#79c0ff;">data/onewire-overrides.json</code>
          and take effect immediately — the temperature poller picks up the new ROM on its next tick.
        </p>
        <div id="diag-onewire-body"><p style="color:#8b949e;">loading…</p></div>
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
    // Include the current ROM (truncated) in the temperature card so the
    // operator can see at a glance which physical probe is being read.
    const hltRom  = owState.sensors.HLT;
    const mashRom = owState.sensors.MASH;
    set('diag-hlt-label',      hltRom  ? `HLT (${hltRom.slice(0, 10)}…)`  : 'HLT');
    set('diag-mash-label',     mashRom ? `MASH (${mashRom.slice(0, 10)}…)` : 'MASH');
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

  // Kick the first 1-Wire fetch immediately so the card is populated.
  void refreshOnewire();
};

export {};
