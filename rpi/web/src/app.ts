// Tab routing + header refresh.

interface TabButton extends HTMLButtonElement { dataset: DOMStringMap }

(function (): void {
  const tabs = document.querySelectorAll<TabButton>('nav .tab');
  const views = document.querySelectorAll<HTMLElement>('main .view');
  tabs.forEach((t) => {
    t.addEventListener('click', () => {
      tabs.forEach((x) => x.classList.remove('active'));
      views.forEach((x) => x.classList.remove('active'));
      t.classList.add('active');
      const v = t.dataset.view;
      if (v) document.getElementById(`view-${v}`)?.classList.add('active');
    });
  });

  function fmtTemp(t: number | undefined | null): string {
    if (t === undefined || t === null || Number.isNaN(t)) return '—';
    return t.toFixed(1);
  }

  function refreshHeader(): void {
    const s = window.brewState;
    const setEl = (id: string, v: string): void => {
      const e = document.getElementById(id);
      if (e) e.textContent = v;
    };
    setEl('hlt-temp',     fmtTemp(s.temps?.HLT));
    setEl('mash-temp',    fmtTemp(s.temps?.MASH));
    setEl('flow-litres',  (s.flow?.boilLitres ?? 0).toFixed(3));
  }
  window.brewBus.on(refreshHeader);

  window.buildDashboardView('view-dashboard');
  window.buildManualView('view-manual');
  window.buildValvesView('view-valves');
  window.buildDiagnosticsView('view-diagnostics');
  window.buildParametersView('view-parameters');
  window.buildRecipesView('view-recipes');
  window.buildBrewView('view-brew');
})();

export {};
