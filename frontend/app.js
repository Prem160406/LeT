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
    inspectorMode: false,
    activeGraphKey: 'ALL',
    isolateGraph: false
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
      populateAssetDropdowns();
    } catch (err) {
      console.error(err);
    }
  }

  function populateAssetDropdowns() {
    const reportSelect = document.getElementById('report-asset');
    const depSelect = document.getElementById('asset-dependency');
    const safetySelect = document.getElementById('select-safety-asset');
    const assets = state.assets || [];

    if (reportSelect) {
      const currentVal = reportSelect.value;
      reportSelect.innerHTML = `
        <option value="" disabled ${!currentVal ? 'selected' : ''}>-- Select Equipment --</option>
        ${assets.map(a => `<option value="${a.assetCode}">${a.assetCode} — ${a.name}</option>`).join('')}
        <option value="__custom__">+ Enter custom equipment code...</option>
      `;
      if (currentVal && (assets.some(a => a.assetCode === currentVal) || currentVal === '__custom__')) {
        reportSelect.value = currentVal;
      }
    }

    if (depSelect) {
      const currentVal = depSelect.value;
      depSelect.innerHTML = `
        <option value="">(None - Independent Backbone Equipment)</option>
        ${assets.map(a => `<option value="${a.assetCode}">${a.assetCode} — ${a.name}</option>`).join('')}
      `;
      if (currentVal) depSelect.value = currentVal;
    }

    if (safetySelect && assets.length > 0) {
      const currentVal = safetySelect.value;
      safetySelect.innerHTML = assets.map(a => `<option value="${a.assetCode}">${a.assetCode} — ${a.name}</option>`).join('');
      if (currentVal && assets.some(a => a.assetCode === currentVal)) {
        safetySelect.value = currentVal;
      }
    }

    const edgeFromSelect = document.getElementById('select-edge-from');
    const edgeToSelect = document.getElementById('select-edge-to');
    if (edgeFromSelect) {
      const cur = edgeFromSelect.value;
      edgeFromSelect.innerHTML = `
        <option value="" disabled ${!cur ? 'selected' : ''}>-- Select Provider --</option>
        ${assets.map(a => `<option value="${a.assetCode}">${a.assetCode} — ${a.name}</option>`).join('')}
      `;
      if (cur) edgeFromSelect.value = cur;
    }
    if (edgeToSelect) {
      const cur = edgeToSelect.value;
      edgeToSelect.innerHTML = `
        <option value="" disabled ${!cur ? 'selected' : ''}>-- Select Dependent --</option>
        ${assets.map(a => `<option value="${a.assetCode}">${a.assetCode} — ${a.name}</option>`).join('')}
      `;
      if (cur) edgeToSelect.value = cur;
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
  const reportSelect = document.getElementById('report-asset');
  const customInput = document.getElementById('report-asset-custom');

  if (reportSelect && customInput) {
    reportSelect.addEventListener('change', () => {
      if (reportSelect.value === '__custom__') {
        customInput.style.display = 'block';
        customInput.required = true;
        customInput.focus();
      } else {
        customInput.style.display = 'none';
        customInput.required = false;
      }
    });
  }

  if (formSubmitReport) {
    formSubmitReport.addEventListener('submit', async (e) => {
      e.preventDefault();
      let assetCode = document.getElementById('report-asset').value.trim();
      if (assetCode === '__custom__' && customInput) {
        assetCode = customInput.value.trim();
      }
      if (!assetCode) {
        showToast('Please select or specify target equipment', 'danger');
        return;
      }
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

        showToast(`Report for ${assetCode} prioritized into Emergency Queue`, 'success');
        formSubmitReport.reset();
        if (customInput) customInput.style.display = 'none';
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
      const depSelect = document.getElementById('asset-dependency');
      if (depSelect) depSelect.value = 'LAB-UPS-01';
      showToast('Sample HPLC instrument populated into form', 'info');
    });
  }

  const btnFillDemoReport = document.getElementById('btn-fill-demo-report');
  if (btnFillDemoReport) {
    btnFillDemoReport.addEventListener('click', () => {
      const select = document.getElementById('report-asset');
      if (select) {
        select.value = 'LAB-SPEC-003';
        if (customInput) customInput.style.display = 'none';
      }
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
      else if (state.activeSubtab === 'report-calib-subtab') type = 'calibrations';
      else if (state.activeSubtab === 'report-all-assets') type = 'all-assets';
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

  const formAddEdge = document.getElementById('form-add-edge');
  if (formAddEdge) {
    formAddEdge.addEventListener('submit', async (e) => {
      e.preventDefault();
      const from = document.getElementById('select-edge-from').value;
      const to = document.getElementById('select-edge-to').value;
      if (!from || !to) {
        showToast('Please select both provider and dependent equipment', 'danger');
        return;
      }
      if (from === to) {
        showToast('Self-loop dependencies are not permitted', 'danger');
        return;
      }
      try {
        const res = await fetch('/api/graph/edge', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ from, to })
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || 'Failed to add dependency');
        showToast(`Dependency established: ${from} ──> ${to}`, 'success');
        await refreshAll();
      } catch (err) {
        showToast(err.message, 'danger');
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
  // INTERACTIVE VECTOR GRAPH (SVG + CSS BUBBLES) & BFS BLAST RADIUS
  // =========================================================================
  const nodePositions = {};

  function getShortBadge(id) {
    if (!id) return 'DEV';
    const clean = id.replace(/^LAB-?/i, '');
    if (clean.length > 5) return clean.substring(0, 5).toUpperCase();
    return clean.toUpperCase();
  }

  function setupNodeCoordinates() {
    const nodes = state.graphData.nodes || [];
    if (nodes.length === 0) return;

    const predefined = {
      'LAB-UPS-01': { x: 130, y: 210, role: 'source', radius: 34, badge: 'UPS', subLabel: 'Main UPS 10kVA', systems: ['SYS-PWR'] },
      'LAB-NET-SW': { x: 380, y: 120, role: 'hub', radius: 30, badge: 'NET', subLabel: 'Gigabit Switch', systems: ['SYS-PWR', 'SYS-NET'] },
      'LAB-COOL-01': { x: 380, y: 300, role: 'hub', radius: 30, badge: 'COOL', subLabel: 'Water Chiller', systems: ['SYS-PWR', 'SYS-OPT'] },
      'LAB-PC-017': { x: 680, y: 70, role: 'leaf', radius: 26, badge: 'PC-17', subLabel: 'Rig Alpha', systems: ['SYS-PWR', 'SYS-NET', 'SYS-ANA'] },
      'LAB-SPEC-003': { x: 680, y: 170, role: 'leaf', radius: 26, badge: 'SPEC', subLabel: 'UV-Vis Spec', systems: ['SYS-PWR', 'SYS-NET', 'SYS-ANA'] },
      'LAB-LASER-01': { x: 680, y: 270, role: 'leaf', radius: 26, badge: 'LASER', subLabel: 'Nd:YAG Pulse', systems: ['SYS-PWR', 'SYS-OPT'] },
      'LAB-CENT-009': { x: 890, y: 210, role: 'leaf', radius: 26, badge: 'CENT', subLabel: 'Centrifuge 30k', systems: ['SYS-ANA'] }
    };

    nodes.forEach((node, idx) => {
      if (predefined[node.id]) {
        nodePositions[node.id] = {
          ...node,
          x: predefined[node.id].x,
          y: predefined[node.id].y,
          radius: predefined[node.id].radius,
          badge: predefined[node.id].badge,
          subLabel: predefined[node.id].subLabel,
          systems: predefined[node.id].systems
        };
      } else {
        const angle = (idx / nodes.length) * 2 * Math.PI;
        nodePositions[node.id] = {
          ...node,
          x: Math.round(500 + Math.cos(angle) * 360),
          y: Math.round(210 + Math.sin(angle) * 150),
          radius: 26,
          badge: getShortBadge(node.id),
          subLabel: truncate(node.name, 16),
          systems: ['ALL', 'SYS-ANA']
        };
      }
    });
  }

  const subsystemNames = {
    'ALL': 'Full Laboratory Topology',
    'SYS-PWR': 'Primary Power Grid (UPS Root)',
    'SYS-NET': 'High-Speed Data Bus (Switch Root)',
    'SYS-OPT': 'Thermal & Laser Optics Loop (Chiller Root)',
    'SYS-ANA': 'Analytical Equipment & Workstations'
  };

  function drawTopologyGraph() {
    const svg = document.getElementById('graph-svg');
    const edgesGroup = document.getElementById('svg-edges');
    const nodesGroup = document.getElementById('svg-nodes');
    if (!svg || !edgesGroup || !nodesGroup) return;

    setupNodeCoordinates();

    const edges = state.graphData.edges || [];
    const sources = (state.graphData.sources || []).map(s => s.assetCode);
    const activeKey = state.activeGraphKey || 'ALL';
    const isIsolate = !!state.isolateGraph;

    edgesGroup.innerHTML = '';
    nodesGroup.innerHTML = '';

    let visibleNodeCount = 0;
    let visibleLinkCount = 0;

    // 1. Draw directed edges
    edges.forEach(edge => {
      const fromNode = nodePositions[edge.from];
      const toNode = nodePositions[edge.to];
      if (!fromNode || !toNode) return;

      const isAffectedEdge = (state.selectedGraphNode === edge.from || state.animatingNodes.has(edge.from)) &&
                             (state.animatingNodes.has(edge.to));

      const inSubsystem = (activeKey === 'ALL') ||
                          (fromNode.systems && fromNode.systems.includes(activeKey) &&
                           toNode.systems && toNode.systems.includes(activeKey));

      if (!inSubsystem && isIsolate) {
        return; // Clutter-free: completely remove edge from view
      }

      visibleLinkCount++;

      const angle = Math.atan2(toNode.y - fromNode.y, toNode.x - fromNode.x);
      const startX = fromNode.x + Math.cos(angle) * (fromNode.radius + 2);
      const startY = fromNode.y + Math.sin(angle) * (fromNode.radius + 2);
      const endX = toNode.x - Math.cos(angle) * (toNode.radius + 6);
      const endY = toNode.y - Math.sin(angle) * (toNode.radius + 6);

      const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      line.setAttribute('x1', startX.toFixed(1));
      line.setAttribute('y1', startY.toFixed(1));
      line.setAttribute('x2', endX.toFixed(1));
      line.setAttribute('y2', endY.toFixed(1));

      let lineClass = `svg-edge ${isAffectedEdge ? 'affected' : ''}`;
      if (!inSubsystem) lineClass += ' dimmed';
      line.setAttribute('class', lineClass);
      line.setAttribute('marker-end', isAffectedEdge ? 'url(#arrow-affected)' : 'url(#arrow-normal)');
      edgesGroup.appendChild(line);
    });

    // 2. Draw nodes (CSS Bubbles)
    Object.values(nodePositions).forEach(node => {
      const isSelected = state.selectedGraphNode === node.id;
      const isFailed = isSelected && state.animatingNodes.size > 0;
      const isAffected = state.animatingNodes.has(node.id) && !isSelected;
      const isSource = sources.includes(node.id);

      const inSubsystem = (activeKey === 'ALL') || (node.systems && node.systems.includes(activeKey));

      if (!inSubsystem && isIsolate) {
        return; // Clutter-free: completely hide outside nodes
      }

      visibleNodeCount++;

      let statusClass = 'normal';
      if (isFailed) statusClass = 'failed';
      else if (isAffected) statusClass = 'affected';
      else if (isSelected) statusClass = 'selected';
      else if (isSource) statusClass = 'source';

      if (!inSubsystem) statusClass += ' dimmed';

      const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      g.setAttribute('class', `node-bubble ${statusClass}`);
      g.setAttribute('data-id', node.id);
      g.setAttribute('transform', `translate(${node.x}, ${node.y})`);

      // Browser tooltip
      const title = document.createElementNS('http://www.w3.org/2000/svg', 'title');
      title.textContent = `${node.id} — ${node.name}\nSubsystem: ${(node.systems || []).join(', ')}\nClick to evaluate downstream blast radius`;
      g.appendChild(title);

      // Pulse halo circle for smooth CSS keyframe animation
      const pulse = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
      pulse.setAttribute('class', 'bubble-pulse');
      pulse.setAttribute('r', node.radius + 10);
      g.appendChild(pulse);

      // Main sharp bubble circle (scales smoothly on hover with zero vibration)
      const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
      circle.setAttribute('class', 'bubble-circle');
      circle.setAttribute('r', node.radius);
      circle.setAttribute('cx', '0');
      circle.setAttribute('cy', '0');
      g.appendChild(circle);

      // Short clean badge centered inside bubble (e.g. UPS, NET, SPEC, COOL)
      const codeText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      codeText.setAttribute('class', 'bubble-code');
      codeText.setAttribute('y', '0');
      codeText.textContent = node.badge || getShortBadge(node.id);
      g.appendChild(codeText);

      // Asset code text directly under the bubble
      const nameText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      nameText.setAttribute('class', 'bubble-name');
      nameText.setAttribute('y', node.radius + 15);
      nameText.textContent = node.id;
      g.appendChild(nameText);

      // Short equipment description directly under asset code
      const subText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      subText.setAttribute('class', 'bubble-sub');
      subText.setAttribute('y', node.radius + 28);
      subText.textContent = node.subLabel || truncate(node.name, 16);
      g.appendChild(subText);

      // Click event for blast radius inspection
      g.addEventListener('click', async () => {
        state.selectedGraphNode = node.id;
        await simulateNodeBlast(node.id);
      });

      nodesGroup.appendChild(g);
    });

    // Update Reference Key Status Strip
    const statusName = document.getElementById('ref-status-name');
    if (statusName) statusName.textContent = subsystemNames[activeKey] || activeKey;
    const statusCode = document.getElementById('ref-status-code');
    if (statusCode) statusCode.textContent = activeKey;
    const statusCount = document.getElementById('ref-status-count');
    if (statusCount) statusCount.textContent = visibleNodeCount;
    const statusLinks = document.getElementById('ref-status-links');
    if (statusLinks) statusLinks.textContent = visibleLinkCount;
  }

  function truncate(str, maxLen) {
    if (!str) return '';
    return str.length > maxLen ? str.slice(0, maxLen - 1) + '...' : str;
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
    drawTopologyGraph();
  }

  const btnRefreshGraph = document.getElementById('btn-refresh-graph');
  if (btnRefreshGraph) {
    btnRefreshGraph.addEventListener('click', () => {
      state.selectedGraphNode = null;
      state.animatingNodes.clear();
      state.activeGraphKey = 'ALL';
      state.isolateGraph = false;

      const chk = document.getElementById('chk-isolate-graph');
      if (chk) chk.checked = false;

      const sel = document.getElementById('select-graph-subsystem');
      if (sel) sel.value = 'ALL';

      document.querySelectorAll('.ref-chip').forEach(c => {
        c.classList.toggle('active', c.getAttribute('data-graph-key') === 'ALL');
      });

      drawTopologyGraph();
      const box = document.getElementById('blast-simulation-result');
      if (box) box.textContent = 'Click any equipment bubble above to evaluate its downstream dependents.';
    });
  }

  // Reference Key Chips & Dropdown Listeners
  document.querySelectorAll('.ref-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      const key = chip.getAttribute('data-graph-key');
      if (!key) return;
      document.querySelectorAll('.ref-chip').forEach(c => c.classList.remove('active'));
      chip.classList.add('active');

      const select = document.getElementById('select-graph-subsystem');
      if (select) select.value = key;

      state.activeGraphKey = key;
      drawTopologyGraph();
    });
  });

  const selectGraphSub = document.getElementById('select-graph-subsystem');
  if (selectGraphSub) {
    selectGraphSub.addEventListener('change', () => {
      const key = selectGraphSub.value;
      state.activeGraphKey = key;

      document.querySelectorAll('.ref-chip').forEach(c => {
        c.classList.toggle('active', c.getAttribute('data-graph-key') === key);
      });

      drawTopologyGraph();
    });
  }

  const chkIsolate = document.getElementById('chk-isolate-graph');
  if (chkIsolate) {
    chkIsolate.addEventListener('change', () => {
      state.isolateGraph = chkIsolate.checked;
      drawTopologyGraph();
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
    const q = query.toLowerCase().trim();

    // 1. Try server-side native C++ Copilot Engine
    try {
      const res = await fetch('/api/chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ query })
      });
      if (res.ok) {
        const data = await res.json();
        if (data && data.reply) {
          appendChatMessage(data.reply, 'bot', data.isCode);
          return;
        }
      }
    } catch (err) {
      // Fall through to client-side intent resolution
    }

    // 2. Client-side intent resolution (offline fallback)
    const cleanQ = q.replace(/[?!.,;:()'"]/g, ' ').replace(/\s+/g, ' ').trim();

    // 0. Incomplete / Ambiguous queries (e.g. "which", "which is", "what", "help")
    const shortAmbiguous = ['which', 'which is', 'which are', 'which one', 'what', 'what is', 'what are', 'who', 'who is', 'help'];
    if (shortAmbiguous.includes(cleanQ) || cleanQ.length <= 2) {
      let reply = `I can assist with both live laboratory diagnostics and deep "Under the Hood" Data Structures & Algorithms:\n\n` +
        `UNDER THE HOOD & DSA ARCHITECTURE:\n` +
        `• "How does this work under the hood?" — 5-stage real-time pipeline\n` +
        `• "What data structures are used?" — All 8 custom C++ structures (Zero STL)\n` +
        `• "How is risk score calculated?" — Exact mathematical formula & blast multiplier\n` +
        `• "Explain Big-O time complexity" — Algorithmic bounds breakdown\n` +
        `• "How does Max-Heap / Hash Table / AVL Tree work?" — Data structure internals\n\n` +
        `LIVE FLEET DIAGNOSTICS:\n` +
        `• "Which is most faulty?" / "Which is least faulty?"\n` +
        `• "Which has the highest downtime?" (Merge Sort)\n` +
        `• "Which is highest risk?" (Max-Heap triage root)\n` +
        `• "Which machine is a Single Point of Failure?" (Graph BFS)\n` +
        `• "Which supplier is the worst (Lemon Index)?"\n` +
        `• "Predict next failure" (Remaining Useful Life Forecast)`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // Graph Addition & Dynamic Dependencies
    if (cleanQ.includes('add to graph') || cleanQ.includes('add graph') || cleanQ.includes('new addition') || cleanQ.includes('add equipment') || cleanQ.includes('how to add') || cleanQ.includes('connect equipment')) {
      let reply = `How Graph Additions & New Dependencies Work:\n\n` +
        `1. Registering New Equipment (Tab 1):\n` +
        `   In the "Register Equipment" form, pick an "Upstream Dependency (Relies On)":\n` +
        `   • If an upstream dependency is selected, a directed edge (Provider ──> Dependent) is created automatically.\n` +
        `   • If (None) is selected, the equipment is registered as a standalone root backbone node.\n\n` +
        `2. Connecting Existing Equipment (Tab 2):\n` +
        `   Use the "Connect Dependency Link" form under the Equipment Dependency Map:\n` +
        `   • Select Provider (e.g. LAB-UPS-01)\n` +
        `   • Select Dependent (e.g. LAB-CENT-009)\n` +
        `   • Click "Add Dependency Connection" — the graph immediately renders the new link and recalculates blast radius & SPOFs!\n\n` +
        `3. Dynamic Subsystem Filtering:\n` +
        `   New nodes dynamically inherit downstream reachability and appear in the Sub-Graph Reference Keys.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: Architecture & Pipeline
    if (cleanQ.includes('under the hood') || cleanQ.includes('how does it work') || cleanQ.includes('how it works') ||
        cleanQ.includes('architecture') || cleanQ.includes('pipeline') || cleanQ.includes('internals') || cleanQ.includes('how does this work')) {
      let reply = `Under the Hood: 5-Stage Real-Time Pipeline Architecture\n\n` +
        `1. Circular Queue [O(1) Intake]:\n` +
        `   Bounded ring buffer ingests sensor alerts & engineer tickets without memory spikes or heap reallocations.\n\n` +
        `2. Hash Table [O(1) Avg Lookup]:\n` +
        `   Asset code mapped to hardware records using separate chaining collision resolution & dynamic prime rehashing.\n\n` +
        `3. Doubly Linked List [O(1) History & Drift]:\n` +
        `   Chronological telemetry chain appends readings; computes physical Drift Velocity: Delta(Dev) / Delta(Time).\n\n` +
        `4. Directed Graph [BFS O(V + E) Blast Radius]:\n` +
        `   Traverses downstream power/data dependencies to identify Single Points of Failure (SPOFs) and cascade multipliers.\n\n` +
        `5. Binary Max-Heap [O(log N) Priority Queue]:\n` +
        `   Ranks tasks by Effective Risk Score = BaseRisk × BlastMultiplier. Highest-risk emergency stays at root for O(1) peek and O(log N) dispatch.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: Data Structures Overview / Zero STL
    if (cleanQ.includes('data structure') || cleanQ.includes('dsa') || cleanQ.includes('structures') || cleanQ.includes('zero stl') || cleanQ.includes('from scratch')) {
      let reply = `Custom Data Structures Implemented (100% From Scratch, Zero STL Containers):\n\n` +
        `1. Circular Queue: Bounded O(1) buffer with FIFO wraparound & batch dequeue.\n` +
        `2. Hash Table: O(1) average lookup with separate chaining linked lists & dynamic rehashing.\n` +
        `3. Doubly Linked List: O(1) append & bidirectional traversal for sensor telemetry & drift velocity.\n` +
        `4. AVL Tree: O(log N) self-balancing BST ordered by calendar due dates with O(log N + K) range queries.\n` +
        `5. Binary Max-Heap: O(1) root access and O(log N) bubble-up/down for emergency triage.\n` +
        `6. Directed Graph: Adjacency list representation with Breadth-First Search (BFS) for blast-radius calculation.\n` +
        `7. Undo Stack: O(1) LIFO transaction rollback for fault reports & technician dispatches.\n` +
        `8. Merge Sort: Guaranteed O(N log N) divide-and-conquer stable sorting for audits.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: Big-O Time & Space Complexity
    if (cleanQ.includes('complexity') || cleanQ.includes('big o') || cleanQ.includes('big-o') || cleanQ.includes('time complexity')) {
      let reply = `Theoretical & Practical Complexity Bounds (Big-O Analysis):\n\n` +
        `• Fault Report Intake: O(1) time, O(Capacity) space (Circular Queue)\n` +
        `• Asset Record Lookup: O(1) average time, O(N) space (Hash Table)\n` +
        `• Telemetry History Append: O(1) time (Doubly Linked List tail pointer)\n` +
        `• Drift Velocity Calculation: O(1) time (Linear interpolation between consecutive nodes)\n` +
        `• Blast Radius Impact: O(V + E) time (Graph BFS downstream reachability)\n` +
        `• Priority Queue Insertion: O(log N) time (Max-Heap bubble-up)\n` +
        `• Top Urgency Dispatch: O(log N) time (Max-Heap extract root & heapify)\n` +
        `• Calibration Window Range Query: O(log N + K) time (AVL Tree)\n` +
        `• Audit Compliance Sorting: O(N log N) time, O(N) temp space (Merge Sort)\n` +
        `• Transaction Undo Rollback: O(1) time, O(History) space (Undo Stack)`;
      appendChatMessage(reply, 'bot', true);
      return;
    }

    // UNDER THE HOOD: Risk Score Formula
    if (cleanQ.includes('risk score') || cleanQ.includes('risk formula') || cleanQ.includes('calculate risk') || cleanQ.includes('how is risk') || cleanQ.includes('how is priority') || cleanQ.includes('priority formula') || cleanQ.includes('how is it calculated')) {
      let reply = `Emergency Prioritization Risk Formula:\n\n` +
        `Base Risk = (Severity × 5.0) + (DaysOverdue × 1.5) + (|DriftVelocity| × 10.0)\n\n` +
        `Blast Multiplier = 1.0 + (DownstreamDependents × 0.20)\n\n` +
        `Effective Risk Score = Base Risk × Blast Multiplier\n\n` +
        `Why each term matters:\n` +
        `• Severity (1-5): Reflects immediate hardware breakdown danger.\n` +
        `• Days Overdue: Penalizes deferred preventive maintenance.\n` +
        `• Drift Velocity (mm/s): Catches rapidly accelerating wear before sensor tolerance breach.\n` +
        `• Blast Multiplier: Elevates root infrastructure (e.g. UPS) whose failure halts multiple dependent instruments.`;
      appendChatMessage(reply, 'bot', true);
      return;
    }

    // UNDER THE HOOD: Circular Queue
    if ((cleanQ.includes('circular') && cleanQ.includes('queue')) || (cleanQ.includes('queue') && !cleanQ.includes('triage') && !cleanQ.includes('priority') && !cleanQ.includes('emergency'))) {
      let reply = `Circular Queue (Under the Hood):\n\n` +
        `• Fixed-size contiguous array buffer with front, rear, and count indices.\n` +
        `• Wraparound Logic: rear = (rear + 1) % capacity. Eliminates costly element shifting.\n` +
        `• Bounded Memory Invariant: When count == capacity, new fault arrivals are rejected O(1) or flagged to prevent memory exhaustion.\n` +
        `• Batch Dequeue: Supports O(K) bulk retrieval of K elements in strict FIFO order across circular boundaries.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: Hash Table
    if (cleanQ.includes('hash table') || cleanQ.includes('hash map') || cleanQ.includes('rehashing') || (cleanQ.includes('collision') && !cleanQ.includes('packet'))) {
      let reply = `Hash Table with Separate Chaining (Under the Hood):\n\n` +
        `• Key: Asset Code string (e.g. "LAB-PC-017") hashed via polynomial accumulation modulo bucket count.\n` +
        `• Collision Handling: Linked list chaining per bucket. Avoids clustering seen in linear probing.\n` +
        `• Dynamic Rehashing: Automatically doubles capacity to the next prime when Load Factor exceeds 0.75.\n` +
        `• Average Search, Insert, and Deletion: O(1) expected time.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: Doubly Linked List
    if (cleanQ.includes('doubly linked') || (cleanQ.includes('linked') && cleanQ.includes('list'))) {
      let reply = `Doubly Linked List (Under the Hood):\n\n` +
        `• Every asset maintains a bi-directional chain of historical fault & calibration events.\n` +
        `• Structure: Each node stores prev, next, timestamp, deviation (mm), drift velocity, and description.\n` +
        `• O(1) Append: Fast tail pointer insertion without iterating through historical records.\n` +
        `• Drift Velocity Formula: (CurrentDev - PrevDev) / (CurrentTime - PrevTime). Negative values detect sudden rotor shock; positive values indicate progressive wear.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: AVL Tree
    if (cleanQ.includes('avl') || (cleanQ.includes('tree') && !cleanQ.includes('heap')) || cleanQ.includes('rotations')) {
      let reply = `Self-Balancing AVL Tree (Under the Hood):\n\n` +
        `• Strict height-balanced BST ordered by calendar Due Date (days from today).\n` +
        `• Balance Invariant: |Height(Left) - Height(Right)| <= 1 at every node.\n` +
        `• Rebalancing Rotations: Executes single (LL, RR) or double (LR, RL) rotations in O(1) during insertion/deletion.\n` +
        `• Range Query: Finds all calibrations within [startDay, endDay] in O(log N + K) time.\n` +
        `• Search Operations: findMin() [earliest due], findMax() [furthest due], inorderPredecessor(), and inorderSuccessor().`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: Binary Max-Heap
    if (cleanQ.includes('max heap') || cleanQ.includes('max-heap') || cleanQ.includes('heapify') || (cleanQ.includes('binary') && cleanQ.includes('heap'))) {
      let reply = `Binary Max-Heap Priority Queue (Under the Hood):\n\n` +
        `• Contiguous dynamic array storing complete binary tree where Parent.Risk >= Child.Risk.\n` +
        `• Root Position (Index 0): Always guarantees O(1) instant access to the single highest-urgency emergency in the lab.\n` +
        `• Bubble-Up Insertion: O(log N) compares new task with parent at index (i - 1) / 2.\n` +
        `• Extract-Max (Dispatch): O(log N) swaps root with last element, pops, and heapifies down comparing children at 2*i + 1 and 2*i + 2.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: Undo Stack
    if ((cleanQ.includes('undo') || cleanQ.includes('stack') || cleanQ.includes('rollback')) && !cleanQ.includes('dispatch')) {
      let reply = `Undo Stack (Under the Hood):\n\n` +
        `• LIFO (Last-In-First-Out) transaction stack providing O(1) rollback for technician actions.\n` +
        `• Transaction Records: Stores action type (Fault Enqueued, Job Dispatched, Batch Dequeued), target asset, and snapshot data.\n` +
        `• Reversibility: Dispatched jobs can be pushed back into the Max-Heap; erroneous fault tickets can be cleanly purged from telemetry.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // UNDER THE HOOD: Merge Sort
    if (cleanQ.includes('merge sort') || (cleanQ.includes('sort') && !cleanQ.includes('fault') && !cleanQ.includes('downtime') && !cleanQ.includes('calib'))) {
      let reply = `Merge Sort (Under the Hood):\n\n` +
        `• Divide-and-Conquer algorithm providing guaranteed O(N log N) worst-case sorting (unlike QuickSort's O(N^2) worst case).\n` +
        `• Stability: Preserves the relative order of equipment with identical downtime or fault counts.\n` +
        `• Standalone Scratch Implementation: Zero std::sort or STL vectors. Uses recursive array splitting and auxiliary buffer merging.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // Polarity helpers
    const isLeast = /\b(least|lowest|minimum|min|fewest|safest|healthiest|cleanest)\b/.test(cleanQ);
    const isMost = /\b(most|highest|maximum|max|worst|top|greatest|leader|critical)\b/.test(cleanQ);

    // 1. Fault Frequency: Most Faulty vs. Least Faulty
    const isFaultQuery = /\b(fault|faults|faulty|defects|issues|failure count)\b/.test(cleanQ);
    if (isFaultQuery) {
      try {
        const res = await fetch('/api/reports/faulty?sort=faults');
        const list = await res.json();
        if (!list || list.length === 0) {
          appendChatMessage('No equipment records found in the registry.', 'bot');
          return;
        }

        if (isLeast) {
          // Least faulty: find assets with 0 faults or the lowest fault count
          const zeroFaults = list.filter(item => item.totalFaultCount === 0);
          const leastItem = list[list.length - 1];

          if (zeroFaults.length > 0) {
            let reply = `Least Faulty Equipment (Nominal Clean Fleet):\n` +
              `Found ${zeroFaults.length} equipment unit(s) with 0 logged faults:\n`;
            zeroFaults.forEach(item => {
              reply += `• ${item.name} (${item.assetCode}) — Downtime: ${item.totalDowntimeHours}h | Status: NOMINAL\n`;
            });
            appendChatMessage(reply, 'bot');
          } else if (leastItem) {
            appendChatMessage(`Least Faulty Equipment: ${leastItem.name} (${leastItem.assetCode}) with only ${leastItem.totalFaultCount} logged fault(s) and ${leastItem.totalDowntimeHours}h downtime.`, 'bot');
          }
        } else {
          // Most faulty (default for general fault queries unless "least" specified)
          const top = list[0];
          let reply = `Most Faulty Equipment (Fault Frequency Leader via Merge Sort):\n` +
            `Equipment: ${top.name} (${top.assetCode})\n` +
            `Total Logged Faults: ${top.totalFaultCount}\n` +
            `Accumulated Downtime: ${top.totalDowntimeHours} hours\n` +
            `Location: ${top.location || 'Main Lab'}\n` +
            `Supplier: ${top.supplier || 'GenericLab OEM'}\n` +
            `Downstream Blast Impact: ${top.blastRadiusCount} machines`;
          appendChatMessage(reply, 'bot');
        }
      } catch (err) {
        appendChatMessage('Failed to fetch equipment fault audit.', 'bot');
      }
      return;
    }

    // 2. Downtime: Highest vs. Lowest Downtime
    const isDowntimeQuery = /\b(downtime|offline|outage|hours)\b/.test(cleanQ);
    if (isDowntimeQuery) {
      try {
        const res = await fetch('/api/reports/faulty?sort=downtime');
        const list = await res.json();
        if (!list || list.length === 0) {
          appendChatMessage('No downtime audit records available.', 'bot');
          return;
        }

        if (isLeast) {
          const least = list[list.length - 1];
          appendChatMessage(`Lowest Downtime Equipment: ${least.name} (${least.assetCode}) with only ${least.totalDowntimeHours}h accumulated downtime (${least.totalFaultCount} faults).`, 'bot');
        } else {
          const top = list[0];
          appendChatMessage(`Highest Downtime Equipment (Downtime Leader via Merge Sort):\n` +
            `Equipment: ${top.name} (${top.assetCode})\n` +
            `Accumulated Downtime: ${top.totalDowntimeHours} hours\n` +
            `Faults: ${top.totalFaultCount} | Supplier: ${top.supplier || 'Standard Lab'}`, 'bot');
        }
      } catch (err) {
        appendChatMessage('Failed to fetch downtime audit.', 'bot');
      }
      return;
    }

    // 3. Risk / Triage: Highest Risk vs. Lowest Risk in Max-Heap
    const isRiskQuery = /\b(risk|urgency|urgent|emergency|priorit|triage)\b/.test(cleanQ);
    if (isRiskQuery) {
      try {
        const res = await fetch('/api/triage');
        const tasks = await res.json();
        if (!tasks || tasks.length === 0) {
          appendChatMessage('Triage Queue is currently empty. No emergency jobs pending.', 'bot');
          return;
        }

        if (isLeast) {
          const lowest = tasks[tasks.length - 1];
          appendChatMessage(`Lowest Risk Triage Task: [${lowest.taskId}] ${lowest.assetName} (${lowest.assetCode})\n` +
            `Computed Risk Score: ${lowest.riskScore.toFixed(2)} | Severity: ${lowest.severity} | Overdue: ${lowest.daysOverdue}d`, 'bot');
        } else {
          const highest = tasks[0];
          appendChatMessage(`Highest Urgency Job (Max-Heap Root — Dispatches First):\n` +
            `Task ID: ${highest.taskId} — ${highest.assetName} (${highest.assetCode})\n` +
            `Effective Risk Score: ${highest.riskScore.toFixed(2)} (Base Risk factored with ${highest.blastMultiplier}x Blast Multiplier)\n` +
            `Severity: ${highest.severity}/5 | Overdue: ${highest.daysOverdue} days | Drift Velocity: ${highest.driftVelocity} mm/s\n` +
            `Description: ${highest.description}`, 'bot');
        }
      } catch (err) {
        appendChatMessage('Failed to query live Max-Heap triage queue.', 'bot');
      }
      return;
    }

    // 4. Supplier Quality / Lemon Scorecard (Best vs. Worst)
    const isSupplierQuery = /\b(supplier|suppliers|vendor|vendors|lemon|lemons|manufacturer)\b/.test(cleanQ) || (isMost && /\b(quality|reliable)\b/.test(cleanQ));
    if (isSupplierQuery) {
      try {
        const res = await fetch('/api/reports/suppliers');
        const suppliers = await res.json();
        if (!suppliers || suppliers.length === 0) {
          appendChatMessage('No supplier data on file.', 'bot');
          return;
        }

        const sortedSuppliers = [...suppliers].sort((a, b) => b.score - a.score);
        const best = sortedSuppliers[0];
        const lemon = suppliers.find(s => s.isLemon) || sortedSuppliers[sortedSuppliers.length - 1];

        if (isLeast || cleanQ.includes('best') || cleanQ.includes('top') || cleanQ.includes('highest')) {
          appendChatMessage(`Top Rated Supplier (Tier A Certified):\n` +
            `• ${best.supplierName}: Score ${best.score.toFixed(1)}/100 (Tier ${best.tier})\n` +
            `  Logged Faults: ${best.totalFaults} | Avg Drift: ${best.avgDrift.toFixed(3)} mm/s | Downtime: ${best.avgDowntime}h\n` +
            `  Recommendation: ${best.recommendation}`, 'bot');
        } else {
          let reply = `Supplier Quality Analysis (via Lemon Scorecard):\n`;
          suppliers.forEach(s => {
            reply += `• ${s.supplierName}: Score ${s.score.toFixed(1)}/100 (${s.tier})\n  Faults: ${s.totalFaults}, Drift: ${s.avgDrift.toFixed(3)} mm/s\n`;
          });
          if (lemon && lemon.isLemon) {
            reply += `\nDEFECTIVE VENDOR ALERT: "${lemon.supplierName}" has a reliability score of only ${lemon.score.toFixed(1)}/100 with ${lemon.totalFaults} logged faults.\nRecommendation: File an immediate warranty claim and pause procurement.`;
          }
          appendChatMessage(reply, 'bot');
        }
      } catch (err) {
        appendChatMessage('Failed to retrieve supplier reliability data.', 'bot');
      }
      return;
    }

    // 5. Predict Next Failure / Remaining Useful Life (RUL)
    const isFailureQuery = /\b(next|predict|expect|rul|ettf|fail|breach|tolerance)\b/.test(cleanQ);
    if (isFailureQuery) {
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

    // 6. Single Point of Failure (SPOF) / Blast Radius / Dependency Graph
    const isSpofQuery = /\b(spof|single point|blast|cascade|critical asset|source|depend)\b/.test(cleanQ);
    if (isSpofQuery) {
      try {
        const res = await fetch('/api/reports/sources');
        const data = await res.json();
        const sources = data.sources || [];
        const spofs = sources.filter(s => s.criticality.includes('CRITICAL') || s.totalBlastRadius >= 3);

        if (spofs.length > 0) {
          let reply = `Critical Single Points of Failure (via Graph BFS):\n`;
          spofs.forEach(s => {
            reply += `• [${s.assetCode}] ${s.name}: Blast Radius = ${s.totalBlastRadius} machines (${s.criticality})\n`;
          });
          reply += `Recommendation: Prioritize redundant power/network links to eliminate failure cascades.`;
          appendChatMessage(reply, 'bot');
        } else if (sources.length > 0) {
          const top = sources[0];
          appendChatMessage(`Top Dependency Source: [${top.assetCode}] ${top.name} with blast radius affecting ${top.totalBlastRadius} machines.`, 'bot');
        } else {
          appendChatMessage('No dependency cascades found in the graph.', 'bot');
        }
      } catch (err) {
        appendChatMessage('Failed to query dependency graph blast radius.', 'bot');
      }
      return;
    }

    // 7. Calibration Due / Schedule
    const isCalibQuery = /\b(calib|calibration|due|schedule|overdue)\b/.test(cleanQ);
    if (isCalibQuery) {
      try {
        const res = await fetch('/api/calibrations?days=30');
        const items = await res.json();
        if (items && items.length > 0) {
          let reply = `Upcoming Calibrations (via AVL Tree Range Query):\n`;
          items.forEach(c => {
            reply += `• [${c.assetCode}] ${c.equipmentName}: Due in ${c.daysUntilDue} day(s) (${c.status})\n`;
          });
          appendChatMessage(reply, 'bot');
        } else {
          appendChatMessage('No assets require calibration within the next 30 days.', 'bot');
        }
      } catch (err) {
        appendChatMessage('Failed to query AVL calibration index.', 'bot');
      }
      return;
    }

    // 8. Safe to service / What-if maintenance
    if (cleanQ.includes('service') || cleanQ.includes('safe') || cleanQ.includes('chiller') || cleanQ.includes('cool')) {
      try {
        const target = cleanQ.includes('ups') ? 'LAB-UPS-01' : 'LAB-COOL-01';
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

    // 9. Dispatch top job action
    if (cleanQ.includes('dispatch') || cleanQ.includes('resolve') || cleanQ.includes('pop job')) {
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

    // 10. Explain drift / measurement deviation
    if (cleanQ.includes('drift') || cleanQ.includes('tell us') || cleanQ.includes('repair') || cleanQ.includes('formula')) {
      let reply = `Drift Velocity & Trend Explainability:\n` +
        `• Measurement Drift is computed in the Doubly Linked List via linear interpolation: Delta(Dev) / Delta(Time).\n` +
        `• A positive drift (+0.05 mm/s) indicates progressive mechanical wear or photodetector degradation.\n` +
        `• A negative drift (-0.50 mm/s) indicates sudden rotor wobble or structural shock.\n` +
        `• When recalibration is performed, deviation resets toward baseline, proving the effectiveness of the calibration cycle.`;
      appendChatMessage(reply, 'bot');
      return;
    }

    // 11. Lab Overview / Fleet Summary
    if (cleanQ.includes('overview') || cleanQ.includes('summary') || cleanQ.includes('status') || cleanQ.includes('health')) {
      try {
        const res = await fetch('/api/assets');
        const assets = await res.json();
        const total = assets.length;
        const faulty = assets.filter(a => a.totalFaultCount > 0).length;
        appendChatMessage(`Lab Operations Overview:\n` +
          `• Total Assets in Registry: ${total} (Indexed in Hash Table & AVL Tree)\n` +
          `• Faulty Equipment: ${faulty} unit(s) requiring attention\n` +
          `• Active Triage Tasks: ${state.triageTasks ? state.triageTasks.length : 0} job(s) in Max-Heap\n` +
          `• Single Points of Failure: LAB-UPS-01 (Blast Radius: 5 machines)\n` +
          `Ask me "Which is most faulty?" or "Predict next failure" for details!`, 'bot');
      } catch (err) {
        appendChatMessage('Failed to compile lab operations summary.', 'bot');
      }
      return;
    }

    // 12. Intelligent Fallback with Specific Suggestions
    appendChatMessage(`I parsed your request regarding "${query}".\n\n` +
      `Here are specific questions I can answer directly:\n` +
      `• "Which is most faulty?" (Fault frequency leader via Merge Sort)\n` +
      `• "Which is least faulty?" (Clean equipment with 0 faults)\n` +
      `• "Which is highest risk?" (Max-Heap triage root)\n` +
      `• "Which machine is a Single Point of Failure?" (Graph BFS SPOF)\n` +
      `• "Which supplier is the worst?" (Lemon Scorecard)\n` +
      `• "Predict next failure" (Remaining Useful Life Forecast)\n` +
      `• "Can I safely service LAB-COOL-01?" (Maintenance Safety Advisor)\n` +
      `• "Dispatch top job" (Max-Heap dispatch action)`, 'bot');
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
