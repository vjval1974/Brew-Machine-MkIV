// Tab routing + global header refresh.
(function () {
  const tabs = document.querySelectorAll('nav .tab');
  const views = document.querySelectorAll('main .view');
  tabs.forEach((t) => {
    t.addEventListener('click', () => {
      tabs.forEach((x) => x.classList.remove('active'));
      views.forEach((x) => x.classList.remove('active'));
      t.classList.add('active');
      document.getElementById(`view-${t.dataset.view}`).classList.add('active');
    });
  });

  function refreshHeader() {
    const s = window.brewState;
    document.getElementById('hlt-temp').textContent  = fmtTemp(s.temps?.HLT);
    document.getElementById('mash-temp').textContent = fmtTemp(s.temps?.MASH);
    document.getElementById('flow-litres').textContent =
      (s.flow?.boilLitres || 0).toFixed(3);
  }
  function fmtTemp(t) {
    if (t === undefined || t === null || isNaN(t)) return '—';
    return t.toFixed(1);
  }
  window.brewBus.on(refreshHeader);

  // Build each view on load.
  window.buildDashboardView('view-dashboard');
  window.buildManualView('view-manual');
  window.buildValvesView('view-valves');
  window.buildDiagnosticsView('view-diagnostics');
  window.buildParametersView('view-parameters');
  window.buildBrewView('view-brew');
})();
