/**
 * ============================================================================
 * LAB OPERATIONS, EMERGENCY FAULT TRIAGE & CALIBRATION ENGINE
 * Client Application & AI Copilot Controller — Zero Emojis
 * ============================================================================
 */

document.addEventListener('DOMContentLoaded', () => {
  // Global State
  const state = {
    assets: [],
    triageTasks: [],
    graphData: { nodes: [], edges: [], sources: [] },
    faultyReport: [],
    suppliersReport: [],
    calibrations: [],
    undoCount: 0,
    activeTab: 'tab-operations',
    activeSubtab: 'report-faulty',
    selectedGraphNode: null,
    animatingNodes: new Set(),
    animationTimer: null,
    inspectorMode: false
  };

  // Toast Notification System (replaces disruptive browser alert popups)
  function showToast(message, type = 'info', duration = 3500) {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.textContent = message;

    container.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateX(100%)';
      toast.style.transition = 'all 0.3s ease';
      setTimeout(() => toast.remove(), 300);
    }, duration);
  }

  // =========================================================================
  // SECTION NAVIGATION TABS & SUBTABS
  // =========================================================================
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));

      btn.classList.add('active');
      const targetId = btn.getAttribute('data-tab');
      const targetPane = document.getElementById(targetId);
      if (targetPane) targetPane.classList.add('active');
      state.activeTab = targetId;

      if (targetId === 'tab-network') {
        drawTopologyGraph();
      } else if (targetId === 'tab-reports') {
        loadSubtabReport(state.activeSubtab);
      } else if (targetId === 'tab-calibrations') {
        loadCalibrations();
      }
    });
  });

  document.querySelectorAll('.subtab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.subtab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.subtab-content').forEach(c => c.classList.remove('active'));

      btn.classList.add('active');
      const targetSubtab = btn.getAttribute('data-subtab');
      const targetContent = document.getElementById(targetSubtab);
      if (targetContent) targetContent.classList.add('active');
      state.activeSubtab = targetSubtab;

      loadSubtabReport(targetSubtab);
    });
  });

  // =========================================================================
  // DSA ARCHITECTURE INSPECTOR TOGGLE & MODALS
  // =========================================================================
  const btnToggleInspector = document.getElementById('btn-toggle-inspector');
  const inspectorStatusText = document.getElementById('inspector-status-text');
  const inspectorBanner = document.getElementById('inspector-banner');

  if (btnToggleInspector) {
    btnToggleInspector.addEventListener('click', () => {
      state.inspectorMode = !state.inspectorMode;
      if (state.inspectorMode) {
        document.body.classList.add('inspector-mode');
        inspectorStatusText.textContent = 'ON';
        btnToggleInspector.classList.remove('btn-outline-info');
        btnToggleInspector.classList.add('btn-primary');
        if (inspectorBanner) inspectorBanner.style.display = 'block';
        showToast('DSA Inspector Mode Active: Algorithm cards highlighted', 'info');
      } else {
        document.body.classList.remove('inspector-mode');
        inspectorStatusText.textContent = 'OFF';
        btnToggleInspector.classList.add('btn-outline-info');
        btnToggleInspector.classList.remove('btn-primary');
        if (inspectorBanner) inspectorBanner.style.display = 'none';
        showToast('DSA Inspector Mode Deactivated', 'info');
      }
    });
  }

  // DSA Spec Sheet Modal
  const dsaModal = document.getElementById('dsa-modal');
  const btnDsaSheet = document.getElementById('btn-dsa-sheet');
  const btnCloseDsaModal = document.getElementById('btn-close-dsa-modal');

  if (btnDsaSheet && dsaModal) {
    btnDsaSheet.addEventListener('click', () => dsaModal.style.display = 'flex');
  }
  if (btnCloseDsaModal && dsaModal) {
    btnCloseDsaModal.addEventListener('click', () => dsaModal.style.display = 'none');
  }

  // Official PDF Certificate Modal
  const pdfModal = document.getElementById('pdf-modal');
  const btnGeneratePdf = document.getElementById('btn-generate-pdf');
  const btnClosePdfModal = document.getElementById('btn-close-pdf-modal');
  const btnPrintCertificate = document.getElementById('btn-print-certificate');

  if (btnGeneratePdf && pdfModal) {
    btnGeneratePdf.addEventListener('click', () => {
      populateExecutivePdfCertificate();
      pdfModal.style.display = 'flex';
    });
  }
  if (btnClosePdfModal && pdfModal) {
    btnClosePdfModal.addEventListener('click', () => pdfModal.style.display = 'none');
  }
  if (btnPrintCertificate) {
    btnPrintCertificate.addEventListener('click', () => window.print());
  }

  // Close modals on backdrop click
  window.addEventListener('click', (e) => {
    if (e.target === dsaModal) dsaModal.style.display = 'none';
    if (e.target === pdfModal) pdfModal.style.display = 'none';
  });

  // =========================================================================
  // DATA FETCHING & SYNCHRONIZATION
  // =========================================================================
  async function refreshAll() {
    await Promise.all([
      loadAssets(),
      loadTriageQueue(),
      loadGraphData(),
      loadCalibrations()
    ]);
    if (state.activeTab === 'tab-reports') {
      loadSubtabReport(state.activeSubtab);
    }
  }

  async function loadAssets() {
    try {
      const res = await fetch('/api/assets');
      if (!res.ok) throw new Error('Failed to load assets');
      state.assets = await res.json();
      document.getElementById('val-asset-count').textContent = state.assets.length;
      renderAllAssetsTable();
    } catch (err) {
      console.error(err);
    }
  }

  async function loadTriageQueue() {
    try {
      const res = await fetch('/api/triage');
      if (!res.ok) throw new Error('Failed to load triage queue');
      state.triageTasks = await res.json();
      document.getElementById('val-triage-count').textContent = state.triageTasks.length;
      renderTriageTable();
    } catch (err) {
      console.error(err);
    }
  }

  async function loadGraphData() {
    try {
      const res = await fetch('/api/graph');
      if (!res.ok) throw new Error('Failed to load graph data');
      state.graphData = await res.json();

      const spofs = (state.graphData.sources || []).filter(s => s.criticality.includes('CRITICAL'));
      document.getElementById('val-spof-count').textContent = spofs.length;

      renderSpofSummary();
      drawTopologyGraph();
    } catch (err) {
      console.error(err);
    }
  }

  async function loadCalibrations() {
    try {
      const start = document.getElementById('calib-start-range')?.value || 0;
      const end = document.getElementById('calib-end-range')?.value || 60;
      const res = await fetch(`/api/reports/calibrations?start=${start}&end=${end}`);
      if (!res.ok) throw new Error('Failed to load calibrations');
      state.calibrations = await res.json();

      // Count due within 30 days
      const dueSoon = state.calibrations.filter(c => c.dueDate <= 30).length;
      document.getElementById('val-calib-count').textContent = dueSoon;

      renderCalibrationsTable();
    } catch (err) {
      console.error(err);
    }
  }

  // Subtab report switcher
  function loadSubtabReport(subtab) {
    if (subtab === 'report-faulty') loadFaultyReport();
    else if (subtab === 'report-suppliers') loadSuppliersReport();
    else if (subtab === 'report-predictive') loadPredictiveReport();
    else if (subtab === 'report-calib-subtab') loadCalibrations();
    else if (subtab === 'report-sources') renderSourcesTable();
    else if (subtab === 'report-all-assets') renderAllAssetsTable();
  }

  async function loadFaultyReport() {
    try {
      const sortCriterion = document.getElementById('select-faulty-sort')?.value || 'downtime';
      const res = await fetch(`/api/reports/faulty?sort=${sortCriterion}`);
      if (!res.ok) throw new Error('Failed to load faulty equipment report');
      state.faultyReport = await res.json();
      renderFaultyTable();
    } catch (err) {
      console.error(err);
    }
  }

  async function loadSuppliersReport() {
    try {
      const res = await fetch('/api/reports/suppliers');
      if (!res.ok) throw new Error('Failed to load suppliers report');
      state.suppliersReport = await res.json();
      renderSuppliersTable();
    } catch (err) {
      console.error(err);
    }
  }

  async function loadPredictiveReport() {
    try {
      const res = await fetch('/api/reports/faulty?sort=downtime');
      if (!res.ok) throw new Error('Failed to load data for predictive report');
      const list = await res.json();
      renderPredictiveTable(list);
    } catch (err) {
      console.error(err);
    }
  }

  // =========================================================================
  // TABLE RENDERING
  // =========================================================================
  function renderTriageTable() {
    const tbody = document.getElementById('triage-table-body');
    if (!tbody) return;

    if (!state.triageTasks || state.triageTasks.length === 0) {
      tbody.innerHTML = '<tr><td colspan="10" class="empty-state">No emergency triage tasks pending in queue.</td></tr>';
      return;
    }

    tbody.innerHTML = state.triageTasks.map((task, idx) => {
      const isRoot = idx === 0;
      const blastMultText = task.blastMultiplier > 1.0 ? ` (x${task.blastMultiplier.toFixed(2)})` : '';
      return `
        <tr class="${isRoot ? 'root-task' : ''}">
          <td><code>${task.taskId}</code></td>
          <td><strong>${task.assetCode}</strong></td>
          <td>${task.assetName}</td>
          <td><span class="badge ${task.severity >= 4 ? 'badge-risk' : 'badge-high'}">Sev ${task.severity}</span></td>
          <td>${task.daysOverdue}d</td>
          <td><code>${task.driftVelocity >= 0 ? '+' : ''}${task.driftVelocity.toFixed(3)} mm/s</code></td>
          <td><span class="badge ${task.blastRadiusCount > 1 ? 'badge-spof' : 'badge-terminal'}">${task.blastRadiusCount} assets</span></td>
          <td><span class="badge ${isRoot ? 'badge-root' : 'badge-risk'}">${task.riskScore.toFixed(1)}${blastMultText}</span></td>
          <td>${task.description || '-'}</td>
          <td>
            ${isRoot ? `<button class="btn btn-sm btn-danger" onclick="window.dispatchTopJob()">Dispatch Root</button>` : `<span class="badge badge-terminal">Rank #${idx + 1}</span>`}
          </td>
        </tr>
      `;
    }).join('');
  }

  function renderFaultyTable() {
    const tbody = document.getElementById('faulty-report-body');
    if (!tbody) return;

    if (!state.faultyReport || state.faultyReport.length === 0) {
      tbody.innerHTML = '<tr><td colspan="9" class="empty-state">No faulty assets recorded. All equipment operating within tolerance.</td></tr>';
      return;
    }

    tbody.innerHTML = state.faultyReport.map(asset => {
      const drift = asset.latestDriftVelocity || 0.0;
      const blast = asset.blastRadiusCount || 0;
      return `
        <tr>
          <td><code>${asset.assetCode}</code></td>
          <td><strong>${asset.name}</strong></td>
          <td>${asset.supplier || 'Standard Lab Corp'}</td>
          <td>${asset.location || '-'}</td>
          <td><span class="badge ${asset.totalFaultCount >= 2 ? 'badge-risk' : 'badge-high'}">${asset.totalFaultCount}</span></td>
          <td><strong>${asset.totalDowntimeHours.toFixed(1)}h</strong></td>
          <td><code>${drift >= 0 ? '+' : ''}${drift.toFixed(3)} mm/s</code></td>
          <td><span class="badge ${blast > 1 ? 'badge-spof' : 'badge-terminal'}">${blast} machines</span></td>
          <td>${asset.nextCalibrationDue}d</td>
        </tr>
      `;
    }).join('');
  }

  function renderSuppliersTable() {
    const tbody = document.getElementById('suppliers-report-body');
    if (!tbody) return;

    if (!state.suppliersReport || state.suppliersReport.length === 0) {
      tbody.innerHTML = '<tr><td colspan="8" class="empty-state">No supplier quality data available.</td></tr>';
      return;
    }

    tbody.innerHTML = state.suppliersReport.map(s => {
      let tierBadge = 'badge-tier-a';
      if (s.isLemon) tierBadge = 'badge-tier-c';
      else if (s.tier.includes('Tier B')) tierBadge = 'badge-tier-b';

      return `
        <tr>
          <td><strong>${s.supplierName}</strong></td>
          <td>${s.assetCount}</td>
          <td><span class="badge ${s.totalFaults >= 2 ? 'badge-risk' : 'badge-compliant'}">${s.totalFaults}</span></td>
          <td>${s.totalDowntime.toFixed(1)}h</td>
          <td><code>${s.avgDrift.toFixed(3)} mm/s</code></td>
          <td><strong>${s.score.toFixed(1)} / 100</strong></td>
          <td><span class="badge ${tierBadge}">${s.tier}</span></td>
          <td>${s.recommendation}</td>
        </tr>
      `;
    }).join('');
  }

  function renderPredictiveTable(list) {
    const tbody = document.getElementById('predictive-report-body');
    if (!tbody) return;

    if (!list || list.length === 0) {
      tbody.innerHTML = '<tr><td colspan="8" class="empty-state">No active equipment telemetry to compute predictive RUL.</td></tr>';
      return;
    }

    tbody.innerHTML = list.map(item => {
      const dev = item.latestDeviation || 0.0;
      const drift = item.latestDriftVelocity || 0.0;
      const margin = item.remainingMargin !== undefined ? item.remainingMargin : Math.max(0.0, 5.0 - Math.abs(dev));
      const ettf = item.ettfDays !== undefined ? item.ettfDays : (Math.abs(drift) > 0.0001 ? margin / Math.abs(drift) : 999.0);
      const urgency = item.failureUrgency || (ettf <= 3.0 ? 'CRITICAL IMMINENT' : (ettf <= 14.0 ? 'ACCELERATING DRIFT' : 'NOMINAL'));

      let urgencyBadge = 'badge-compliant';
      let actionText = 'Nominal operational status';
      if (urgency.includes('CRITICAL')) {
        urgencyBadge = 'badge-risk';
        actionText = 'URGENT: Schedule immediate recalibration before sensor breach';
      } else if (urgency.includes('ACCELERATING')) {
        urgencyBadge = 'badge-high';
        actionText = 'WARNING: Sensor drift accelerating; inspect bearing/detector';
      }

      const ettfText = ettf >= 900.0 ? '> 90 days (Stable)' : `${ettf.toFixed(1)} days`;

      return `
        <tr>
          <td><code>${item.assetCode}</code></td>
          <td><strong>${item.name}</strong></td>
          <td><code>${dev.toFixed(2)} mm</code></td>
          <td><code>${drift >= 0 ? '+' : ''}${drift.toFixed(3)} mm/s</code></td>
          <td>${margin.toFixed(2)} mm</td>
          <td><strong>${ettfText}</strong></td>
          <td><span class="badge ${urgencyBadge}">${urgency}</span></td>
          <td>${actionText}</td>
        </tr>
      `;
    }).join('');
  }

  function renderSourcesTable() {
    const tbody = document.getElementById('sources-report-body');
    if (!tbody) return;

    const sources = state.graphData.sources || [];
    if (sources.length === 0) {
      tbody.innerHTML = '<tr><td colspan="7" class="empty-state">No source dependency data computed.</td></tr>';
      return;
    }

    tbody.innerHTML = sources.map(src => {
      const isCritical = src.criticality.includes('CRITICAL');
      const isHigh = src.criticality.includes('HIGH');
      const badgeClass = isCritical ? 'badge-spof' : (isHigh ? 'badge-high' : 'badge-terminal');

      return `
        <tr>
          <td><code>${src.assetCode}</code></td>
          <td><strong>${src.name}</strong></td>
          <td>Main Facility</td>
          <td>${src.outDegree} downstream</td>
          <td>${src.inDegree} upstream</td>
          <td><strong>${src.totalBlastRadius} machines</strong></td>
          <td><span class="badge ${badgeClass}">${src.criticality}</span></td>
        </tr>
      `;
    }).join('');
  }

  function renderAllAssetsTable() {
    const tbody = document.getElementById('all-assets-report-body');
    if (!tbody) return;

    if (!state.assets || state.assets.length === 0) {
      tbody.innerHTML = '<tr><td colspan="7" class="empty-state">No equipment currently registered in Hash Table.</td></tr>';
      return;
    }

    tbody.innerHTML = state.assets.map(a => `
      <tr>
        <td><code>${a.assetCode}</code></td>
        <td><strong>${a.name}</strong></td>
        <td>${a.supplier || 'Standard Lab Corp'}</td>
        <td>${a.location || '-'}</td>
        <td>${a.totalFaultCount}</td>
        <td>${a.totalDowntimeHours.toFixed(1)}h</td>
        <td>${a.nextCalibrationDue}d</td>
      </tr>
    `).join('');
  }

  function renderCalibrationsTable() {
    const tbody = document.getElementById('calib-table-body');
    if (!tbody) return;

    if (!state.calibrations || state.calibrations.length === 0) {
      tbody.innerHTML = '<tr><td colspan="5" class="empty-state">No calibration records in specified AVL range.</td></tr>';
      return;
    }

    tbody.innerHTML = state.calibrations.map(c => {
      const isDueSoon = c.dueDate <= 15;
      const isDueMod = c.dueDate <= 30;
      const badgeClass = isDueSoon ? 'badge-overdue' : (isDueMod ? 'badge-due-soon' : 'badge-compliant');
      const statusText = isDueSoon ? 'CRITICAL IMMINENT' : (isDueMod ? 'DUE SOON' : 'COMPLIANT');

      return `
        <tr>
          <td><strong>Day ${c.dueDate}</strong></td>
          <td><code>${c.assetCode}</code></td>
          <td><strong>${c.assetName}</strong></td>
          <td>${c.location || '-'}</td>
          <td><span class="badge ${badgeClass}">${statusText}</span></td>
        </tr>
      `;
    }).join('');
  }

  function renderSpofSummary() {
    const container = document.getElementById('spof-summary-list');
    if (!container) return;

    const sources = state.graphData.sources || [];
    const critical = sources.filter(s => s.totalBlastRadius > 0);

    if (critical.length === 0) {
      container.innerHTML = '<div style="color: var(--text-muted); font-size: 0.8rem;">Zero single-point-of-failure vulnerabilities detected.</div>';
      return;
    }

    container.innerHTML = critical.map(s => {
      const isSpof = s.criticality.includes('CRITICAL');
      return `
        <div class="spof-row ${isSpof ? 'critical' : ''}">
          <div>
            <strong>${s.assetCode}</strong> (${s.name})
            <div style="font-size: 0.7rem; color: var(--text-secondary); margin-top: 2px;">
              Blast Radius: ${s.totalBlastRadius} machines | Direct: ${s.outDegree}
            </div>
          </div>
          <span class="badge ${isSpof ? 'badge-spof' : 'badge-high'}">${s.criticality}</span>
        </div>
      `;
    }).join('');
  }

  // =========================================================================
  // ACTIONS: ADD ASSET & SUBMIT REPORT
  // =========================================================================
  const formAddAsset = document.getElementById('form-add-asset');
  if (formAddAsset) {
    formAddAsset.addEventListener('submit', async (e) => {
      e.preventDefault();
      const code = document.getElementById('asset-code').value.trim();
      const name = document.getElementById('asset-name').value.trim();
      const supp = document.getElementById('asset-supplier').value.trim();
      const loc = document.getElementById('asset-loc').value.trim();
      const calib = parseInt(document.getElementById('asset-calib').value, 10) || 30;
      const downtime = parseFloat(document.getElementById('asset-downtime').value) || 0.0;
      const dependsOn = document.getElementById('asset-dependency').value.trim();

      try {
        const res = await fetch('/api/assets', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            assetCode: code,
            name: name,
            supplier: supp,
            location: loc,
            nextCalibrationDue: calib,
            totalDowntimeHours: downtime,
            dependsOn: dependsOn
          })
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || 'Failed to register asset');

        showToast(`Asset ${code} registered successfully into Hash Table & AVL Tree`, 'success');
        formAddAsset.reset();
        await refreshAll();
      } catch (err) {
        showToast(err.message, 'danger');
      }
    });
  }

  const formSubmitReport = document.getElementById('form-submit-report');
  if (formSubmitReport) {
    formSubmitReport.addEventListener('submit', async (e) => {
      e.preventDefault();
      const assetCode = document.getElementById('report-asset').value.trim();
      const severity = parseInt(document.getElementById('report-severity').value, 10) || 3;
      const overdue = parseInt(document.getElementById('report-overdue').value, 10) || 0;
      const deviation = parseFloat(document.getElementById('report-deviation').value) || 0.0;
      const desc = document.getElementById('report-desc').value.trim();

      try {
        const res = await fetch('/api/reports', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            reportId: `RPT-${Math.floor(1000 + Math.random() * 9000)}`,
            assetCode: assetCode,
            severity: severity,
            daysOverdue: overdue,
            measuredDeviation: deviation,
            timestamp: Math.floor(Date.now() / 1000),
            description: desc
          })
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || 'Failed to submit report');

        state.undoCount++;
        document.getElementById('undo-count').textContent = state.undoCount;

        showToast(`Report for ${assetCode} prioritized into Max-Heap`, 'success');
        formSubmitReport.reset();
        await refreshAll();
      } catch (err) {
        showToast(err.message, 'danger');
      }
    });
  }

  // 1-Click Quick Demo Sample Fillers
  const btnFillDemoAsset = document.getElementById('btn-fill-demo-asset');
  if (btnFillDemoAsset) {
    btnFillDemoAsset.addEventListener('click', () => {
      const demoNum = Math.floor(10 + Math.random() * 90);
      document.getElementById('asset-code').value = `LAB-HPLC-${demoNum}`;
      document.getElementById('asset-name').value = 'High-Performance Chromatograph';
      document.getElementById('asset-supplier').value = 'Agilent Technologies';
      document.getElementById('asset-loc').value = 'Chamber 402, Analytics';
      document.getElementById('asset-calib').value = '25';
      document.getElementById('asset-downtime').value = '4.5';
      document.getElementById('asset-dependency').value = 'LAB-UPS-01';
      showToast('Sample HPLC instrument populated into form', 'info');
    });
  }

  const btnFillDemoReport = document.getElementById('btn-fill-demo-report');
  if (btnFillDemoReport) {
    btnFillDemoReport.addEventListener('click', () => {
      document.getElementById('report-asset').value = 'LAB-SPEC-003';
      document.getElementById('report-severity').value = '5';
      document.getElementById('report-overdue').value = '12';
      document.getElementById('report-deviation').value = '6.8';
      document.getElementById('report-desc').value = 'Severe photodetector saturation & temperature spike';
      showToast('Sample critical fault ticket populated into form', 'info');
    });
  }

  // =========================================================================
  // ACTIONS: DISPATCH TOP, UNDO, CASCADE SIMULATION
  // =========================================================================
  window.dispatchTopJob = async function() {
    try {
      const res = await fetch('/api/dispatch', { method: 'POST' });
      const data = await res.json();
      if (!res.ok) throw new Error(data.message || 'Failed to dispatch top job');

      state.undoCount++;
      document.getElementById('undo-count').textContent = state.undoCount;

      showToast(`Dispatched Task ${data.dispatched.taskId} for ${data.dispatched.assetCode} (Risk: ${data.dispatched.riskScore.toFixed(1)})`, 'success');
      await refreshAll();
    } catch (err) {
      showToast(err.message, 'danger');
    }
  };

  const btnDispatch = document.getElementById('btn-dispatch');
  if (btnDispatch) btnDispatch.addEventListener('click', window.dispatchTopJob);

  const btnUndo = document.getElementById('btn-undo');
  if (btnUndo) {
    btnUndo.addEventListener('click', async () => {
      try {
        const res = await fetch('/api/undo', { method: 'POST' });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || 'Undo failed');

        state.undoCount = Math.max(0, data.remainingUndo || 0);
        document.getElementById('undo-count').textContent = state.undoCount;

        showToast(data.message, 'info');
        await refreshAll();
      } catch (err) {
        showToast(err.message, 'danger');
      }
    });
  }

  const btnSimulateRoot = document.getElementById('btn-simulate-root');
  if (btnSimulateRoot) {
    btnSimulateRoot.addEventListener('click', async () => {
      try {
        const res = await fetch('/api/simulate-cascade', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ assetCode: 'LAB-UPS-01' })
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || 'Simulation failed');

        state.undoCount++;
        document.getElementById('undo-count').textContent = state.undoCount;

        showToast(`Catastrophic failure simulated on LAB-UPS-01 (${data.blastRadiusCount} dependents halted)`, 'danger');

        // Trigger visual wave animation on canvas
        triggerBlastWave('LAB-UPS-01', data.affectedDependents || []);
        await refreshAll();
      } catch (err) {
        showToast(err.message, 'danger');
      }
    });
  }

  // Range Query Filter
  const btnQueryCalib = document.getElementById('btn-query-calib');
  if (btnQueryCalib) btnQueryCalib.addEventListener('click', loadCalibrations);

  const btnAllCalib = document.getElementById('btn-all-calib');
  if (btnAllCalib) {
    btnAllCalib.addEventListener('click', () => {
      document.getElementById('calib-start-range').value = 0;
      document.getElementById('calib-end-range').value = 999;
      loadCalibrations();
    });
  }

  // Export CSV
  const btnExportCsv = document.getElementById('btn-export-csv');
  if (btnExportCsv) {
    btnExportCsv.addEventListener('click', () => {
      let type = 'faulty';
      if (state.activeSubtab === 'report-sources') type = 'sources';
      else if (state.activeSubtab === 'report-suppliers') type = 'suppliers';
      window.location.href = `/api/reports/export-csv?type=${type}`;
    });
  }

  const btnExportCalibCsv = document.getElementById('btn-export-calib-csv');
  if (btnExportCalibCsv) {
    btnExportCalibCsv.addEventListener('click', () => {
      window.location.href = '/api/reports/export-csv?type=calibrations';
    });
  }

  // =========================================================================
  // MAINTENANCE SAFETY ADVISOR (WHAT-IF)
  // =========================================================================
  const formSafetyCheck = document.getElementById('form-safety-check');
  if (formSafetyCheck) {
    formSafetyCheck.addEventListener('submit', async (e) => {
      e.preventDefault();
      const targetAsset = document.getElementById('select-safety-asset').value;
      const box = document.getElementById('safety-verdict-box');
      if (!box) return;

      box.innerHTML = 'Evaluating dependency graph and active triage conflicts...';

      try {
        const res = await fetch(`/api/what-if/service?asset=${targetAsset}`);
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || 'Safety check failed');

        const isSafe = data.isSafe;
        box.className = `safety-box ${isSafe ? 'safe' : 'danger'}`;

        let affectedListHtml = '';
        if (data.affectedAssets && data.affectedAssets.length > 0) {
          affectedListHtml = `
            <div style="margin-top: 0.5rem;">
              <strong>Downstream Instruments Affected (${data.dependentCount}):</strong>
              <ul style="margin: 0.35rem 0 0 1.25rem;">
                ${data.affectedAssets.map(a => `<li><code>${a.code}</code> — ${a.name}</li>`).join('')}
              </ul>
            </div>
          `;
        }

        let conflictHtml = '';
        if (data.activeConflicts > 0) {
          conflictHtml = `
            <div style="margin-top: 0.5rem; color: #fca5a5;">
              <strong>Triage Conflicts Detected:</strong> ${data.conflictSummary}
            </div>
          `;
        }

        box.innerHTML = `
          <div style="font-size: 1rem; font-weight: 700; margin-bottom: 0.35rem;">
            Verdict: ${data.verdict}
          </div>
          <div>${data.guidance}</div>
          ${affectedListHtml}
          ${conflictHtml}
        `;
      } catch (err) {
        box.className = 'safety-box danger';
        box.textContent = err.message;
      }
    });
  }

  // =========================================================================
  // OFFICIAL EXECUTIVE PDF CERTIFICATE BUILDER
  // =========================================================================
  function populateExecutivePdfCertificate() {
    const certDate = document.getElementById('cert-date');
    if (certDate) certDate.textContent = new Date().toLocaleString();

    document.getElementById('cert-total-assets').textContent = state.assets.length;
    document.getElementById('cert-active-emergencies').textContent = state.triageTasks.length;
    document.getElementById('cert-spof-count').textContent = document.getElementById('val-spof-count').textContent;
    document.getElementById('cert-calib-due').textContent = document.getElementById('val-calib-count').textContent;

    // Supplier table
    const suppTbody = document.getElementById('cert-suppliers-table');
    if (suppTbody) {
      if (state.suppliersReport && state.suppliersReport.length > 0) {
        suppTbody.innerHTML = state.suppliersReport.map(s => `
          <tr>
            <td><strong>${s.supplierName}</strong></td>
            <td>${s.assetCount}</td>
            <td>${s.totalFaults}</td>
            <td>${s.totalDowntime.toFixed(1)}</td>
            <td><strong>${s.score.toFixed(1)}/100</strong></td>
            <td>${s.tier}</td>
            <td>${s.recommendation}</td>
          </tr>
        `).join('');
      } else {
        suppTbody.innerHTML = '<tr><td colspan="7">No supplier records available.</td></tr>';
      }
    }

    // High-downtime table
    const faultyTbody = document.getElementById('cert-faulty-table');
    if (faultyTbody) {
      if (state.faultyReport && state.faultyReport.length > 0) {
        faultyTbody.innerHTML = state.faultyReport.slice(0, 5).map(f => `
          <tr>
            <td><code>${f.assetCode}</code></td>
            <td><strong>${f.name}</strong></td>
            <td>${f.supplier || 'Standard Lab Corp'}</td>
            <td>${f.totalFaultCount}</td>
            <td>${f.totalDowntimeHours.toFixed(1)}h</td>
            <td>${f.blastRadiusCount} machines</td>
          </tr>
        `).join('');
      } else {
        faultyTbody.innerHTML = '<tr><td colspan="6">All equipment fully nominal.</td></tr>';
      }
    }

    // Calib table
    const calibTbody = document.getElementById('cert-calib-table');
    if (calibTbody) {
      const urgentCalib = (state.calibrations || []).filter(c => c.dueDate <= 30);
      if (urgentCalib.length > 0) {
        calibTbody.innerHTML = urgentCalib.map(c => `
          <tr>
            <td><strong>${c.dueDate} days</strong></td>
            <td><code>${c.assetCode}</code></td>
            <td><strong>${c.assetName}</strong></td>
            <td>${c.location || '-'}</td>
          </tr>
        `).join('');
      } else {
        calibTbody.innerHTML = '<tr><td colspan="4">No calibrations due within the 30-day compliance window.</td></tr>';
      }
    }
  }

  // =========================================================================
  // INTERACTIVE GRAPH CANVAS & BFS BLAST WAVE
  // =========================================================================
  const canvas = document.getElementById('graph-canvas');
  const ctx = canvas ? canvas.getContext('2d') : null;
  const nodePositions = {};

  function setupNodeCoordinates() {
    const nodes = state.graphData.nodes || [];
    if (nodes.length === 0 || !canvas) return;

    const width = canvas.width;
    const height = canvas.height;

    const predefined = {
      'LAB-UPS-01': { x: 160, y: height / 2, role: 'source' },
      'LAB-NET-SW': { x: 420, y: height / 2 - 80, role: 'hub' },
      'LAB-COOL-01': { x: 420, y: height / 2 + 80, role: 'hub' },
      'LAB-PC-017': { x: 740, y: height / 2 - 130, role: 'leaf' },
      'LAB-SPEC-003': { x: 740, y: height / 2 - 30, role: 'leaf' },
      'LAB-LASER-01': { x: 740, y: height / 2 + 80, role: 'leaf' },
      'LAB-CENT-009': { x: 960, y: height / 2, role: 'leaf' }
    };

    nodes.forEach((node, idx) => {
      if (predefined[node.id]) {
        nodePositions[node.id] = {
          ...node,
          x: predefined[node.id].x,
          y: predefined[node.id].y,
          radius: predefined[node.id].role === 'source' ? 24 : 18
        };
      } else {
        const angle = (idx / nodes.length) * 2 * Math.PI;
        nodePositions[node.id] = {
          ...node,
          x: width / 2 + Math.cos(angle) * (width * 0.35),
          y: height / 2 + Math.sin(angle) * (height * 0.35),
          radius: 18
        };
      }
    });
  }

  function drawTopologyGraph(animProgress = 0) {
    if (!ctx || !canvas) return;
    setupNodeCoordinates();

    ctx.fillStyle = '#f8fafc';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    const edges = state.graphData.edges || [];
    const sources = (state.graphData.sources || []).map(s => s.assetCode);

    // 1. Draw directed edges
    edges.forEach(edge => {
      const fromNode = nodePositions[edge.from];
      const toNode = nodePositions[edge.to];
      if (!fromNode || !toNode) return;

      const isAffectedEdge = (state.selectedGraphNode === edge.from || state.animatingNodes.has(edge.from)) &&
                             (state.animatingNodes.has(edge.to));

      ctx.beginPath();
      ctx.moveTo(fromNode.x, fromNode.y);
      ctx.lineTo(toNode.x, toNode.y);
      ctx.strokeStyle = isAffectedEdge ? '#dc2626' : '#cbd5e1';
      ctx.lineWidth = isAffectedEdge ? 3 : 1.5;
      if (isAffectedEdge) {
        ctx.setLineDash([6, 4]);
      }
      ctx.stroke();
      ctx.setLineDash([]);

      drawArrowHead(fromNode.x, fromNode.y, toNode.x, toNode.y, toNode.radius, isAffectedEdge ? '#dc2626' : '#64748b');
    });

    // 2. Draw nodes
    Object.values(nodePositions).forEach(node => {
      const isSelected = state.selectedGraphNode === node.id;
      const isFailed = isSelected && state.animatingNodes.size > 0;
      const isAffected = state.animatingNodes.has(node.id) && !isSelected;
      const isSource = sources.includes(node.id);

      if (isFailed || isAffected) {
        ctx.beginPath();
        ctx.arc(node.x, node.y, node.radius + 8 + Math.sin(animProgress) * 4, 0, 2 * Math.PI);
        ctx.fillStyle = isFailed ? 'rgba(220, 38, 38, 0.18)' : 'rgba(217, 119, 6, 0.18)';
        ctx.fill();
      }

      ctx.beginPath();
      ctx.arc(node.x, node.y, node.radius, 0, 2 * Math.PI);

      if (isFailed) {
        ctx.fillStyle = '#dc2626';
      } else if (isAffected) {
        ctx.fillStyle = '#d97706';
      } else if (isSource) {
        ctx.fillStyle = '#7c3aed';
      } else {
        ctx.fillStyle = '#2563eb';
      }
      ctx.fill();

      ctx.lineWidth = 2;
      ctx.strokeStyle = '#ffffff';
      ctx.stroke();

      ctx.fillStyle = '#ffffff';
      ctx.font = 'bold 10px sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(node.id, node.x, node.y);

      ctx.fillStyle = '#0f172a';
      ctx.font = '600 10px sans-serif';
      ctx.fillText(truncate(node.name, 18), node.x, node.y + node.radius + 13);
    });
  }

  function drawArrowHead(fromX, fromY, toX, toY, radius, color) {
    const angle = Math.atan2(toY - fromY, toX - fromX);
    const endX = toX - Math.cos(angle) * (radius + 2);
    const endY = toY - Math.sin(angle) * (radius + 2);

    ctx.beginPath();
    ctx.moveTo(endX, endY);
    ctx.lineTo(endX - 9 * Math.cos(angle - Math.PI / 6), endY - 9 * Math.sin(angle - Math.PI / 6));
    ctx.lineTo(endX - 9 * Math.cos(angle + Math.PI / 6), endY - 9 * Math.sin(angle + Math.PI / 6));
    ctx.closePath();
    ctx.fillStyle = color;
    ctx.fill();
  }

  function truncate(str, maxLen) {
    if (!str) return '';
    return str.length > maxLen ? str.slice(0, maxLen - 1) + '...' : str;
  }

  // Click on canvas to inspect blast radius
  if (canvas) {
    canvas.addEventListener('click', async (e) => {
      const rect = canvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      let clickedNode = null;
      for (const [id, pos] of Object.entries(nodePositions)) {
        const dist = Math.hypot(pos.x - x, pos.y - y);
        if (dist <= pos.radius + 4) {
          clickedNode = id;
          break;
        }
      }

      if (clickedNode) {
        state.selectedGraphNode = clickedNode;
        await simulateNodeBlast(clickedNode);
      }
    });
  }

  async function simulateNodeBlast(assetCode) {
    const resultBox = document.getElementById('blast-simulation-result');
    if (resultBox) resultBox.textContent = `Running BFS blast-radius calculation for ${assetCode}...`;

    try {
      const res = await fetch(`/api/blast-radius-demo?asset=${assetCode}`);
      const data = await res.json();
      if (!res.ok) throw new Error('Failed to run blast simulation');

      triggerBlastWave(assetCode, data.bfsAffected || []);

      if (resultBox) {
        resultBox.innerHTML = `
Source Equipment: <strong>${data.sourceAsset}</strong>
Breadth-First Search (BFS) Blast Radius: <strong>${data.bfsCount} downstream machines affected</strong>
Transitive Impact Path:
  ${(data.bfsAffected && data.bfsAffected.length > 0) ? data.bfsAffected.map(a => `-> [HALT] ${a}`).join('\n  ') : '(No downstream dependent instruments)'}
        `;
      }
    } catch (err) {
      if (resultBox) resultBox.textContent = err.message;
    }
  }

  function triggerBlastWave(sourceId, affectedIds) {
    state.selectedGraphNode = sourceId;
    state.animatingNodes = new Set(affectedIds);

    let progress = 0;
    if (state.animationTimer) clearInterval(state.animationTimer);

    state.animationTimer = setInterval(() => {
      progress += 0.2;
      drawTopologyGraph(progress);
      if (progress > 12) {
        clearInterval(state.animationTimer);
        state.animationTimer = null;
      }
    }, 50);
  }

  const btnRefreshGraph = document.getElementById('btn-refresh-graph');
  if (btnRefreshGraph) {
    btnRefreshGraph.addEventListener('click', () => {
      state.selectedGraphNode = null;
      state.animatingNodes.clear();
      drawTopologyGraph();
      const box = document.getElementById('blast-simulation-result');
      if (box) box.textContent = 'Select an equipment node above to calculate its transitive blast radius.';
    });
  }

  // =========================================================================
  // EMBEDDED LAB AI COPILOT (OFFLINE, LOCAL ENGINE ANALYTICS)
  // =========================================================================
  const chatbotWindow = document.getElementById('chatbot-window');
  const btnToggleChatbot = document.getElementById('btn-toggle-chatbot');
  const btnCloseChatbot = document.getElementById('btn-close-chatbot');
  const chatMessages = document.getElementById('chat-messages');
  const chatInputForm = document.getElementById('chat-input-form');
  const chatInputText = document.getElementById('chat-input-text');

  if (btnToggleChatbot && chatbotWindow) {
    btnToggleChatbot.addEventListener('click', () => {
      const isVisible = chatbotWindow.style.display !== 'none';
      chatbotWindow.style.display = isVisible ? 'none' : 'flex';
    });
  }

  if (btnCloseChatbot && chatbotWindow) {
    btnCloseChatbot.addEventListener('click', () => {
      chatbotWindow.style.display = 'none';
    });
  }

  // Chat message appender
  function appendChatMessage(text, sender = 'bot', isCode = false) {
    if (!chatMessages) return;
    const bubble = document.createElement('div');
    bubble.className = `chat-bubble ${sender}`;

    if (isCode) {
      bubble.innerHTML = `<pre>${text}</pre>`;
    } else {
      bubble.innerHTML = text.replace(/\n/g, '<br>');
    }

    chatMessages.appendChild(bubble);
    chatMessages.scrollTop = chatMessages.scrollHeight;
  }

  // Quick Action Chips in Chat
  document.querySelectorAll('.chat-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      const query = chip.getAttribute('data-query');
      if (query) {
        processChatQuery(query);
      }
    });
  });

  if (chatInputForm) {
    chatInputForm.addEventListener('submit', (e) => {
      e.preventDefault();
      const query = chatInputText.value.trim();
      if (!query) return;
      chatInputText.value = '';
      processChatQuery(query);
    });
  }

  async function processChatQuery(query) {
    appendChatMessage(query, 'user');
    const q = query.toLowerCase();

    // 1. Worst suppliers / Bad suppliers
    if (q.includes('supplier') || q.includes('lemon') || q.includes('vendor') || q.includes('worst')) {
      try {
        const res = await fetch('/api/reports/suppliers');
        const suppliers = await res.json();
        const lemon = suppliers.find(s => s.isLemon);

        let reply = `Supplier Quality Analysis (via Lemon Scorecard):\n`;
        suppliers.forEach(s => {
          reply += `• ${s.supplierName}: Score ${s.score.toFixed(1)}/100 (${s.tier})\n  Faults: ${s.totalFaults}, Drift: ${s.avgDrift.toFixed(3)} mm/s\n`;
        });

        if (lemon) {
          reply += `\nDEFECTIVE VENDOR ALERT: "${lemon.supplierName}" has a reliability score of only ${lemon.score.toFixed(1)}/100 with ${lemon.totalFaults} logged faults.\nRecommendation: File an immediate warranty claim and pause procurement.`;
        }
        appendChatMessage(reply, 'bot');
      } catch (err) {
        appendChatMessage('Failed to retrieve supplier reliability data.', 'bot');
      }
      return;
    }

    // 2. Predict next failure / Next expected fault / RUL
    if (q.includes('next') || q.includes('predict') || q.includes('expect') || q.includes('rul') || q.includes('fail')) {
      try {
        const res = await fetch('/api/reports/faulty?sort=downtime');
        const list = await res.json();
        const critical = list.find(item => item.failureUrgency === 'CRITICAL IMMINENT') || list[0];

        if (critical) {
          const dev = critical.latestDeviation || 0.0;
          const drift = critical.latestDriftVelocity || 0.0;
          const margin = critical.remainingMargin || 0.0;
          const ettf = critical.ettfDays || 1.0;

          let reply = `Predictive RUL Failure Forecast:\n` +
            `Asset: ${critical.name} (${critical.assetCode})\n` +
            `Current Deviation: ${dev.toFixed(2)} mm (Tolerance: 5.0 mm)\n` +
            `Drift Velocity: ${drift >= 0 ? '+' : ''}${drift.toFixed(3)} mm/s\n` +
            `Safe Margin Remaining: ${margin.toFixed(2)} mm\n` +
            `Estimated Time to Critical Failure: ${ettf <= 3.0 ? 'IMMINENT (< 3 days)' : ettf.toFixed(1) + ' days'}\n\n` +
            `Trend Progression:\n` +
            `[Baseline 0.0mm] ───> [Dev ${dev.toFixed(2)}mm] ───> [BREACH 5.0mm]`;
          appendChatMessage(reply, 'bot', true);
        } else {
          appendChatMessage('All laboratory equipment is operating within nominal safe margins.', 'bot');
        }
      } catch (err) {
        appendChatMessage('Unable to compute predictive failure forecast.', 'bot');
      }
      return;
    }

    // 3. Most faults / Highest downtime
    if (q.includes('most fault') || q.includes('downtime') || q.includes('who has')) {
      try {
        const res = await fetch('/api/reports/faulty?sort=faults');
        const list = await res.json();
        if (list.length > 0) {
          const top = list[0];
          let reply = `Fault Frequency Leader (via custom Merge Sort):\n` +
            `Equipment: ${top.name} (${top.assetCode})\n` +
            `Total Logged Faults: ${top.totalFaultCount}\n` +
            `Accumulated Downtime: ${top.totalDowntimeHours} hours\n` +
            `Supplier: ${top.supplier || 'Standard Lab Corp'}\n` +
            `Blast Radius: ${top.blastRadiusCount} machines`;
          appendChatMessage(reply, 'bot');
        } else {
          appendChatMessage('No faulty equipment records on file.', 'bot');
        }
      } catch (err) {
        appendChatMessage('Failed to fetch downtime audit.', 'bot');
      }
      return;
    }

    // 4. Safe to service / What-if maintenance
    if (q.includes('service') || q.includes('safe') || q.includes('chiller') || q.includes('cool')) {
      try {
        const target = q.includes('ups') ? 'LAB-UPS-01' : 'LAB-COOL-01';
        const res = await fetch(`/api/what-if/service?asset=${target}`);
        const data = await res.json();
        let reply = `Maintenance Window Evaluation for ${data.assetName} (${data.assetCode}):\n` +
          `Verdict: ${data.verdict}\n` +
          `Impact: Taking this offline halts ${data.dependentCount} downstream machines.\n` +
          `Advice: ${data.guidance}`;
        appendChatMessage(reply, 'bot');
      } catch (err) {
        appendChatMessage('Could not execute safety advisor query.', 'bot');
      }
      return;
    }

    // 5. Dispatch top job action
    if (q.includes('dispatch') || q.includes('resolve')) {
      try {
        const res = await fetch('/api/dispatch', { method: 'POST' });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || 'Queue empty');

        state.undoCount++;
        document.getElementById('undo-count').textContent = state.undoCount;

        let reply = `ACTION EXECUTED: Dispatched top urgency job ${data.dispatched.taskId} from Max-Heap root.\n` +
          `Asset: ${data.dispatched.assetCode}\n` +
          `Resolved Risk Score: ${data.dispatched.riskScore.toFixed(1)}\n` +
          `Remaining jobs in heap: ${state.triageTasks.length - 1}`;
        appendChatMessage(reply, 'bot');
        await refreshAll();
      } catch (err) {
        appendChatMessage(`Dispatch Error: ${err.message}`, 'bot');
      }
      return;
    }

    // 6. Explain drift / latest repaired fault
    if (q.includes('drift') || q.includes('latest') || q.includes('tell us') || q.includes('repair')) {
      let reply = `Drift Velocity & Trend Explainability:\n` +
        `• Measurement Drift is computed in the Doubly Linked List via linear interpolation: Delta(Dev) / Delta(Time).\n` +
        `• A positive drift (+0.05 mm/s) indicates progressive mechanical wear or photodetector degradation.\n` +
        `• A negative drift (-0.50 mm/s) indicates sudden rotor wobble or structural shock.\n` +
        `• When recalibration is performed, deviation resets toward baseline, proving the effectiveness of the calibration cycle.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // Default Fallback
    appendChatMessage(`I parsed your request regarding "${query}". You can query me about:\n• "Which suppliers are bad?" (Lemon Scorecard)\n• "Predict next failure" (RUL Forecast)\n• "Who has the most faults?" (Downtime Audit)\n• "Can I safely service LAB-COOL-01?" (Graph BFS Impact)\n• "Dispatch top job" (Max-Heap Action)`, 'bot');
  }

  // =========================================================================
  // INITIALIZATION
  // =========================================================================
  refreshAll();
  setInterval(() => {
    // Background polling for live sync
    loadTriageQueue();
  }, 10000);
});
