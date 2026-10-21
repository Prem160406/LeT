document.addEventListener('DOMContentLoaded', () => {
  // Navigation Tabs
  const navTabs = document.querySelectorAll('.tab-btn');
  const tabPanes = document.querySelectorAll('.tab-pane');

  navTabs.forEach(tab => {
    tab.addEventListener('click', () => {
      navTabs.forEach(t => t.classList.remove('active'));
      tabPanes.forEach(p => p.classList.remove('active'));
      tab.classList.add('active');
      const target = document.getElementById(tab.dataset.tab);
      if (target) target.classList.add('active');

      if (tab.dataset.tab === 'tab-network') {
        loadGraphData();
      } else if (tab.dataset.tab === 'tab-reports') {
        loadReportsData();
      } else if (tab.dataset.tab === 'tab-calibrations') {
        loadCalibrationSchedule();
      }
    });
  });

  // Report Subtabs
  const subtabBtns = document.querySelectorAll('.subtab-btn');
  const subtabContents = document.querySelectorAll('.subtab-content');

  subtabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      subtabBtns.forEach(b => b.classList.remove('active'));
      subtabContents.forEach(c => c.classList.remove('active'));
      btn.classList.add('active');
      const target = document.getElementById(btn.dataset.subtab);
      if (target) target.classList.add('active');
    });
  });

  // Core Elements
  const formAddAsset = document.getElementById('form-add-asset');
  const formSubmitReport = document.getElementById('form-submit-report');
  const btnRefreshTriage = document.getElementById('btn-refresh-triage');
  const btnUndo = document.getElementById('btn-undo');
  const btnDispatch = document.getElementById('btn-dispatch');
  const btnSimulateRoot = document.getElementById('btn-simulate-root');
  const triageBody = document.getElementById('triage-body');

  const btnFillDemoAsset = document.getElementById('btn-fill-demo-asset');
  const btnFillDemoReport = document.getElementById('btn-fill-demo-report');

  const valAssetCount = document.getElementById('val-asset-count');
  const valTriageCount = document.getElementById('val-triage-count');
  const valSpofCount = document.getElementById('val-spof-count');
  const valCalibCount = document.getElementById('val-calib-count');
  const undoCountSpan = document.getElementById('undo-count');

  // Report Elements
  const faultyReportBody = document.getElementById('faulty-report-body');
  const sourcesReportBody = document.getElementById('sources-report-body');
  const inventoryReportBody = document.getElementById('inventory-report-body');
  const calibScheduleBody = document.getElementById('calib-schedule-body');
  const selectFaultySort = document.getElementById('select-faulty-sort');
  const btnRefreshFaulty = document.getElementById('btn-refresh-faulty-report');
  const btnExportCsv = document.getElementById('btn-export-csv');
  const btnExportCalibCsv = document.getElementById('btn-export-calib-csv');
  const btnQueryCalib = document.getElementById('btn-query-calib');
  const btnAllCalib = document.getElementById('btn-all-calib');

  // Graph Canvas Elements
  const canvas = document.getElementById('graph-canvas');
  const ctx = canvas.getContext('2d');
  const blastInput = document.getElementById('blast-asset-input');
  const btnInspectBlast = document.getElementById('btn-inspect-blast');
  const blastDetailsBox = document.getElementById('blast-details-box');
  const spofSummaryList = document.getElementById('spof-summary-list');
  const btnRefreshGraph = document.getElementById('btn-refresh-graph');

  // Graph state
  let graphNodes = [];
  let graphEdges = [];
  let activeFailedNode = 'LAB-UPS-01';
  let activeAffectedNodes = [];
  let animProgress = 0;

  // =========================================================================
  // NOTIFICATION TOAST SYSTEM (Replaces disruptive alerts)
  // =========================================================================
  function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.textContent = message;

    container.appendChild(toast);
    setTimeout(() => {
      toast.style.transition = 'opacity 0.3s ease';
      toast.style.opacity = '0';
      setTimeout(() => toast.remove(), 300);
    }, 3500);
  }

  // =========================================================================
  // DEMO FILL HELPERS (Easy to use)
  // =========================================================================
  if (btnFillDemoAsset) {
    btnFillDemoAsset.addEventListener('click', () => {
      document.getElementById('asset-code').value = 'LAB-OSC-011';
      document.getElementById('asset-name').value = 'Digital Storage Oscilloscope 100MHz';
      document.getElementById('asset-loc').value = 'Room 302, Signal Processing';
      document.getElementById('asset-calib').value = '25';
      document.getElementById('asset-downtime').value = '2.0';
      showToast('Sample asset data filled into registration form.', 'info');
    });
  }

  if (btnFillDemoReport) {
    btnFillDemoReport.addEventListener('click', () => {
      document.getElementById('report-asset').value = 'LAB-UPS-01';
      document.getElementById('report-severity').value = '5';
      document.getElementById('report-overdue').value = '10';
      document.getElementById('report-deviation').value = '24.50';
      document.getElementById('report-desc').value = 'Battery inverter phase fault and output voltage fluctuation';
      showToast('Sample emergency fault data filled into report form.', 'info');
    });
  }

  // =========================================================================
  // DASHBOARD STATE SYNCHRONIZATION
  // =========================================================================
  async function refreshDashboard() {
    try {
      // 1. Fetch Assets
      const assetsRes = await fetch('/api/assets');
      if (assetsRes.ok) {
        const assets = await assetsRes.json();
        valAssetCount.textContent = assets.length;
        renderInventoryTable(assets);
      }

      // 2. Fetch Triage Tasks from Max-Heap
      const triageRes = await fetch('/api/triage');
      if (triageRes.ok) {
        const tasks = await triageRes.json();
        valTriageCount.textContent = tasks.length;
        renderTriageTable(tasks);
      }

      // 3. Fetch Sources & SPOF Metrics
      const sourcesRes = await fetch('/api/reports/sources');
      if (sourcesRes.ok) {
        const sources = await sourcesRes.json();
        const spofs = sources.filter(s => s.criticality.includes('SPOF') || s.totalBlastRadius >= 3);
        valSpofCount.textContent = spofs.length;
        renderSpofList(sources);
      }

      // 4. Fetch Calibrations Count from AVL Tree
      const calibRes = await fetch('/api/reports/calibrations?start=0&end=30');
      if (calibRes.ok) {
        const calibs = await calibRes.json();
        valCalibCount.textContent = calibs.length;
      }
    } catch (err) {
      console.error('Error refreshing dashboard:', err);
    }
  }

  // =========================================================================
  // TRIAGE TABLE RENDERING
  // =========================================================================
  function renderTriageTable(tasks) {
    if (!tasks || tasks.length === 0) {
      triageBody.innerHTML = `<tr><td colspan="11" class="empty-state">No triage tasks in queue. Submit a fault report above.</td></tr>`;
      return;
    }

    triageBody.innerHTML = tasks.map((task, idx) => {
      const isRoot = (idx === 0);
      return `
        <tr class="${isRoot ? 'root-task' : ''}">
          <td>
            ${isRoot 
              ? '<span class="badge badge-root">ROOT (MAX URGENCY)</span>' 
              : '#' + (idx + 1)}
          </td>
          <td><code>${escapeHtml(task.taskId)}</code></td>
          <td><strong>${escapeHtml(task.assetCode)}</strong></td>
          <td>${escapeHtml(task.assetName || '-')}</td>
          <td>${task.severity} / 5</td>
          <td>${task.daysOverdue} d</td>
          <td>${Number(task.driftVelocity).toFixed(4)}/s</td>
          <td>
            ${task.blastRadiusCount > 0 
              ? `<span class="badge badge-high">${task.blastRadiusCount} machines</span>` 
              : `<span class="badge badge-terminal">0 (leaf)</span>`}
          </td>
          <td><code>${Number(task.blastMultiplier || 1.0).toFixed(2)}x</code></td>
          <td><span class="badge badge-risk">${Number(task.riskScore).toFixed(2)}</span></td>
          <td>${escapeHtml(task.description || '-')}</td>
        </tr>
      `;
    }).join('');
  }

  // =========================================================================
  // FORM SUBMISSIONS
  // =========================================================================
  formAddAsset.addEventListener('submit', async (e) => {
    e.preventDefault();
    const payload = {
      assetCode: document.getElementById('asset-code').value.trim(),
      name: document.getElementById('asset-name').value.trim(),
      location: document.getElementById('asset-loc').value.trim(),
      nextCalibrationDue: parseInt(document.getElementById('asset-calib').value, 10) || 30,
      totalDowntimeHours: parseFloat(document.getElementById('asset-downtime').value) || 0.0
    };

    try {
      const res = await fetch('/api/assets', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const data = await res.json();
      if (res.ok) {
        showToast(data.message || 'Asset registered successfully.', 'success');
        formAddAsset.reset();
        refreshDashboard();
        loadGraphData();
      } else {
        showToast(data.message || 'Failed to register asset.', 'danger');
      }
    } catch (err) {
      showToast('Network error registering asset.', 'danger');
    }
  });

  formSubmitReport.addEventListener('submit', async (e) => {
    e.preventDefault();
    const payload = {
      reportId: 'RPT-' + Math.floor(1000 + Math.random() * 9000),
      assetCode: document.getElementById('report-asset').value.trim(),
      severity: parseInt(document.getElementById('report-severity').value, 10),
      daysOverdue: parseInt(document.getElementById('report-overdue').value, 10) || 0,
      measuredDeviation: parseFloat(document.getElementById('report-deviation').value) || 0.0,
      timestamp: Math.floor(Date.now() / 1000),
      description: document.getElementById('report-desc').value.trim()
    };

    try {
      const res = await fetch('/api/reports', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const data = await res.json();
      if (res.ok) {
        showToast('Report enqueued: Ingestion -> Drift Calc -> Blast Radius -> Max-Heap updated.', 'success');
        formSubmitReport.reset();
        refreshDashboard();
      } else {
        showToast(data.message || 'Failed to process fault report.', 'danger');
      }
    } catch (err) {
      showToast('Network error submitting fault report.', 'danger');
    }
  });

  btnDispatch.addEventListener('click', async () => {
    try {
      const res = await fetch('/api/dispatch', { method: 'POST' });
      const data = await res.json();
      if (res.ok) {
        showToast(`Dispatched top task: ${data.dispatched.taskId} on ${data.dispatched.assetCode} (Risk: ${data.dispatched.riskScore})`, 'success');
        refreshDashboard();
      } else {
        showToast(data.message || 'Triage queue is empty.', 'info');
      }
    } catch (err) {
      showToast('Error dispatching triage task.', 'danger');
    }
  });

  btnUndo.addEventListener('click', async () => {
    try {
      const res = await fetch('/api/undo', { method: 'POST' });
      const data = await res.json();
      if (res.ok) {
        showToast(`Rollback successful: ${data.message}`, 'success');
        undoCountSpan.textContent = data.remainingUndo;
        refreshDashboard();
      } else {
        showToast(data.message || 'Undo stack is empty.', 'info');
      }
    } catch (err) {
      showToast('Error triggering rollback.', 'danger');
    }
  });

  // Catastrophic Cascade Simulation Button
  btnSimulateRoot.addEventListener('click', async () => {
    try {
      const res = await fetch('/api/simulate-cascade', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ assetCode: 'LAB-UPS-01' })
      });
      const data = await res.json();
      if (res.ok) {
        showToast(`Cascade Simulated: Outage on ${data.simulatedAsset} affected ${data.blastRadiusCount} machines (${data.blastMultiplier}x risk boost)`, 'danger');
        
        // Highlight in visual network
        activeFailedNode = data.simulatedAsset;
        activeAffectedNodes = data.affectedDependents;
        blastInput.value = data.simulatedAsset;
        
        // Switch to Network tab
        document.querySelector('[data-tab="tab-network"]').click();
        renderBlastDetails(data.simulatedAsset, data.affectedDependents);
        refreshDashboard();
      } else {
        showToast(data.message || 'Simulation failed.', 'danger');
      }
    } catch (err) {
      showToast('Network error triggering simulation.', 'danger');
    }
  });

  btnRefreshTriage.addEventListener('click', () => {
    refreshDashboard();
    showToast('Triage queue refreshed.', 'info');
  });

  // =========================================================================
  // INTERACTIVE GRAPH CANVAS VISUALIZATION (Graph BFS & Blast Radius)
  // =========================================================================
  async function loadGraphData() {
    try {
      const res = await fetch('/api/graph');
      if (!res.ok) return;
      const data = await res.json();

      graphNodes = data.nodes || [];
      graphEdges = data.edges || [];

      // Calculate node positions
      layoutGraphNodes();
      drawGraph();

      // Trigger initial blast inspection for current active input
      inspectBlastRadius(blastInput.value.trim() || 'LAB-UPS-01');
    } catch (err) {
      console.error('Error loading graph topology:', err);
    }
  }

  function layoutGraphNodes() {
    if (graphNodes.length === 0) return;

    const width = canvas.width;
    const height = canvas.height;
    const centerX = width / 2;
    const centerY = height / 2;

    const inDegrees = {};
    const outDegrees = {};
    graphNodes.forEach(n => { inDegrees[n.id] = 0; outDegrees[n.id] = 0; });
    graphEdges.forEach(e => {
      inDegrees[e.to] = (inDegrees[e.to] || 0) + 1;
      outDegrees[e.from] = (outDegrees[e.from] || 0) + 1;
    });

    const roots = graphNodes.filter(n => inDegrees[n.id] === 0);
    const intermediates = graphNodes.filter(n => inDegrees[n.id] > 0 && outDegrees[n.id] > 0);
    const terminals = graphNodes.filter(n => outDegrees[n.id] === 0);

    const assignCol = (arr, colX) => {
      const step = height / (arr.length + 1);
      arr.forEach((node, i) => {
        node.x = colX;
        node.y = step * (i + 1);
        node.radius = node.blastRadius >= 4 ? 26 : (node.blastRadius >= 1 ? 22 : 18);
      });
    };

    if (roots.length + intermediates.length + terminals.length === graphNodes.length) {
      assignCol(roots, width * 0.18);
      assignCol(intermediates, width * 0.50);
      assignCol(terminals, width * 0.82);
    } else {
      const radius = Math.min(width, height) * 0.38;
      const angleStep = (2 * Math.PI) / graphNodes.length;
      graphNodes.forEach((node, i) => {
        node.x = centerX + radius * Math.cos(i * angleStep);
        node.y = centerY + radius * Math.sin(i * angleStep);
        node.radius = 20;
      });
    }
  }

  function drawGraph() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // 1. Draw directed edges
    graphEdges.forEach(edge => {
      const fromNode = graphNodes.find(n => n.id === edge.from);
      const toNode = graphNodes.find(n => n.id === edge.to);
      if (!fromNode || !toNode) return;

      const isAffectedEdge = (activeFailedNode === fromNode.id && activeAffectedNodes.includes(toNode.id))
                          || (activeAffectedNodes.includes(fromNode.id) && activeAffectedNodes.includes(toNode.id));

      ctx.beginPath();
      ctx.moveTo(fromNode.x, fromNode.y);
      ctx.lineTo(toNode.x, toNode.y);
      ctx.strokeStyle = isAffectedEdge ? '#ef4444' : '#253553';
      ctx.lineWidth = isAffectedEdge ? 3 : 1.5;
      if (isAffectedEdge) {
        ctx.setLineDash([6, 4]);
      } else {
        ctx.setLineDash([]);
      }
      ctx.stroke();
      ctx.setLineDash([]);

      drawArrowHead(fromNode.x, fromNode.y, toNode.x, toNode.y, toNode.radius, isAffectedEdge ? '#ef4444' : '#475569');
    });

    // 2. Draw nodes
    graphNodes.forEach(node => {
      const isFailed = (node.id === activeFailedNode);
      const isAffected = activeAffectedNodes.includes(node.id);
      const isSource = (node.blastRadius >= 4);

      if (isFailed || isAffected) {
        ctx.beginPath();
        ctx.arc(node.x, node.y, node.radius + 8 + Math.sin(animProgress) * 4, 0, 2 * Math.PI);
        ctx.fillStyle = isFailed ? 'rgba(239, 68, 68, 0.22)' : 'rgba(245, 158, 11, 0.18)';
        ctx.fill();
      }

      ctx.beginPath();
      ctx.arc(node.x, node.y, node.radius, 0, 2 * Math.PI);

      if (isFailed) {
        ctx.fillStyle = '#ef4444';
      } else if (isAffected) {
        ctx.fillStyle = '#f59e0b';
      } else if (isSource) {
        ctx.fillStyle = '#9333ea';
      } else {
        ctx.fillStyle = '#0284c7';
      }
      ctx.fill();

      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = isFailed ? 3 : 1.5;
      ctx.stroke();

      ctx.fillStyle = '#ffffff';
      ctx.font = 'bold 11px sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(node.id, node.x, node.y);

      ctx.fillStyle = '#94a3b8';
      ctx.font = '9px sans-serif';
      ctx.fillText(truncate(node.name, 18), node.x, node.y + node.radius + 12);
    });
  }

  function drawArrowHead(fromX, fromY, toX, toY, targetRadius, color) {
    const angle = Math.atan2(toY - fromY, toX - fromX);
    const arrowX = toX - targetRadius * Math.cos(angle);
    const arrowY = toY - targetRadius * Math.sin(angle);
    const headLen = 8;

    ctx.beginPath();
    ctx.moveTo(arrowX, arrowY);
    ctx.lineTo(arrowX - headLen * Math.cos(angle - Math.PI / 6), arrowY - headLen * Math.sin(angle - Math.PI / 6));
    ctx.lineTo(arrowX - headLen * Math.cos(angle + Math.PI / 6), arrowY - headLen * Math.sin(angle + Math.PI / 6));
    ctx.closePath();
    ctx.fillStyle = color;
    ctx.fill();
  }

  canvas.addEventListener('click', (e) => {
    const rect = canvas.getBoundingClientRect();
    const clickX = (e.clientX - rect.left) * (canvas.width / rect.width);
    const clickY = (e.clientY - rect.top) * (canvas.height / rect.height);

    for (const node of graphNodes) {
      const dist = Math.hypot(clickX - node.x, clickY - node.y);
      if (dist <= node.radius + 6) {
        blastInput.value = node.id;
        inspectBlastRadius(node.id);
        break;
      }
    }
  });

  function animate() {
    animProgress += 0.05;
    if (activeFailedNode) {
      drawGraph();
    }
    requestAnimationFrame(animate);
  }
  requestAnimationFrame(animate);

  btnInspectBlast.addEventListener('click', () => {
    const code = blastInput.value.trim() || 'LAB-UPS-01';
    inspectBlastRadius(code);
  });

  btnRefreshGraph.addEventListener('click', () => {
    loadGraphData();
    showToast('Topology reloaded.', 'info');
  });

  async function inspectBlastRadius(assetCode) {
    activeFailedNode = assetCode;
    try {
      const res = await fetch(`/api/blast-radius-demo?asset=${encodeURIComponent(assetCode)}`);
      const data = await res.json();
      if (res.ok) {
        activeAffectedNodes = data.bfsAffected || [];
        renderBlastDetails(data.sourceAsset, data.bfsAffected);
        drawGraph();
      }
    } catch (err) {
      console.error('Error querying blast radius:', err);
    }
  }

  function renderBlastDetails(source, affected) {
    let text = `[GRAPH BFS FAILURE BLAST-RADIUS]\nSource Node: [${source}]\nCascaded Affected Equipment: ${affected.length} machines\n`;
    if (affected.length === 0) {
      text += `Impact: Localized terminal equipment (Zero downstream cascading).\n`;
    } else {
      text += `Downstream Affected Equipment Nodes:\n`;
      affected.forEach((n, i) => {
        text += `  ${i + 1}. [${n}]\n`;
      });
      text += `\nPrioritization Impact: Live Risk Score boosted by (1.0 + 0.20 * ${affected.length}) = ${(1.0 + 0.20 * affected.length).toFixed(2)}x`;
    }
    blastDetailsBox.textContent = text;
  }

  function renderSpofList(sources) {
    if (!sources || sources.length === 0) return;
    spofSummaryList.innerHTML = sources.slice(0, 4).map(s => `
      <div class="spof-row ${s.totalBlastRadius >= 4 ? 'critical' : ''}">
        <div>
          <strong>[${escapeHtml(s.assetCode)}]</strong> ${escapeHtml(s.name)}
          <div style="font-size: 0.74rem; color: #94a3b8;">Direct: ${s.outDegree} | Total Impact: ${s.totalBlastRadius} machines</div>
        </div>
        <span class="badge ${s.totalBlastRadius >= 4 ? 'badge-spof' : (s.totalBlastRadius >= 2 ? 'badge-high' : 'badge-terminal')}">
          ${escapeHtml(s.criticality)}
        </span>
      </div>
    `).join('');
  }

  // =========================================================================
  // AUDIT & REPORTS TAB DATA LOADERS
  // =========================================================================
  async function loadReportsData() {
    loadFaultyReport();
    loadSourcesReport();
  }

  async function loadFaultyReport() {
    const sortBy = selectFaultySort.value;
    try {
      const res = await fetch(`/api/reports/faulty?sort=${sortBy}`);
      const data = await res.json();
      if (!res.ok || data.length === 0) {
        faultyReportBody.innerHTML = `<tr><td colspan="9" class="empty-state">No equipment currently flagged with faults.</td></tr>`;
        return;
      }

      faultyReportBody.innerHTML = data.map((item, idx) => `
        <tr>
          <td>#${idx + 1}</td>
          <td><strong>${escapeHtml(item.assetCode)}</strong></td>
          <td>${escapeHtml(item.name)}</td>
          <td>${escapeHtml(item.location || '-')}</td>
          <td>${item.totalFaultCount}</td>
          <td><strong>${Number(item.totalDowntimeHours).toFixed(1)}h</strong></td>
          <td>${Number(item.latestDriftVelocity).toFixed(4)}/s</td>
          <td>
            ${item.blastRadiusCount > 0 
              ? `<span class="badge badge-high">${item.blastRadiusCount} affected</span>` 
              : `<span class="badge badge-terminal">0</span>`}
          </td>
          <td>${item.nextCalibrationDue}d</td>
        </tr>
      `).join('');
    } catch (err) {
      console.error('Error loading faulty report:', err);
    }
  }

  async function loadSourcesReport() {
    try {
      const res = await fetch('/api/reports/sources');
      const data = await res.json();
      if (!res.ok || data.length === 0) {
        sourcesReportBody.innerHTML = `<tr><td colspan="7" class="empty-state">No source equipment identified.</td></tr>`;
        return;
      }

      sourcesReportBody.innerHTML = data.map(item => `
        <tr>
          <td><strong>${escapeHtml(item.assetCode)}</strong></td>
          <td>${escapeHtml(item.name)}</td>
          <td>${escapeHtml(item.location || '-')}</td>
          <td>${item.outDegree}</td>
          <td>${item.inDegree}</td>
          <td><span class="badge ${item.totalBlastRadius >= 4 ? 'badge-spof' : 'badge-high'}">${item.totalBlastRadius} machines</span></td>
          <td>
            <span class="badge ${item.totalBlastRadius >= 4 ? 'badge-spof' : (item.totalBlastRadius >= 2 ? 'badge-high' : 'badge-terminal')}">
              ${escapeHtml(item.criticality)}
            </span>
          </td>
        </tr>
      `).join('');
    } catch (err) {
      console.error('Error loading sources report:', err);
    }
  }

  function renderInventoryTable(assets) {
    if (!assets || assets.length === 0) {
      inventoryReportBody.innerHTML = `<tr><td colspan="6" class="empty-state">No equipment registered in Hash Table.</td></tr>`;
      return;
    }

    inventoryReportBody.innerHTML = assets.map(a => `
      <tr>
        <td><strong>${escapeHtml(a.assetCode)}</strong></td>
        <td>${escapeHtml(a.name)}</td>
        <td>${escapeHtml(a.location || '-')}</td>
        <td>${a.nextCalibrationDue}d</td>
        <td>${a.totalFaultCount}</td>
        <td>${a.totalDowntimeHours}h</td>
      </tr>
    `).join('');
  }

  btnRefreshFaulty.addEventListener('click', () => {
    loadFaultyReport();
    showToast('Faulty equipment table re-sorted.', 'info');
  });

  // CSV Export Handler
  btnExportCsv.addEventListener('click', () => {
    const activeSubtab = document.querySelector('.subtab-btn.active').dataset.subtab;
    let type = 'faulty';
    if (activeSubtab === 'report-sources') type = 'sources';
    window.location.href = `/api/reports/export-csv?type=${type}`;
    showToast(`Downloading ${type} report CSV...`, 'info');
  });

  // =========================================================================
  // AVL CALIBRATION SCHEDULE TAB
  // =========================================================================
  async function loadCalibrationSchedule(start = 0, end = 9999999999) {
    try {
      const res = await fetch(`/api/reports/calibrations?start=${start}&end=${end}`);
      const data = await res.json();
      if (!res.ok || data.length === 0) {
        calibScheduleBody.innerHTML = `<tr><td colspan="5" class="empty-state">No calibration records in selected range.</td></tr>`;
        return;
      }

      calibScheduleBody.innerHTML = data.map(item => {
        let statusBadge = `<span class="badge badge-compliant">COMPLIANT</span>`;
        if (item.dueDate <= 0) {
          statusBadge = `<span class="badge badge-overdue">OVERDUE</span>`;
        } else if (item.dueDate <= 30) {
          statusBadge = `<span class="badge badge-due-soon">DUE SOON (${item.dueDate}d)</span>`;
        }

        return `
          <tr>
            <td><strong>${item.dueDate} days</strong></td>
            <td><code>${escapeHtml(item.assetCode)}</code></td>
            <td>${escapeHtml(item.assetName)}</td>
            <td>${escapeHtml(item.location || '-')}</td>
            <td>${statusBadge}</td>
          </tr>
        `;
      }).join('');
    } catch (err) {
      console.error('Error loading calibration schedule:', err);
    }
  }

  btnQueryCalib.addEventListener('click', () => {
    const start = parseInt(document.getElementById('calib-start-range').value, 10) || 0;
    const end = parseInt(document.getElementById('calib-end-range').value, 10) || 60;
    loadCalibrationSchedule(start, end);
    showToast(`Filtering calibrations between ${start} and ${end} days.`, 'info');
  });

  btnAllCalib.addEventListener('click', () => {
    loadCalibrationSchedule(0, 9999999999);
    showToast('Showing all calibration records.', 'info');
  });

  btnExportCalibCsv.addEventListener('click', () => {
    window.location.href = `/api/reports/export-csv?type=calibrations`;
    showToast('Downloading calibration schedule CSV...', 'info');
  });

  // Utilities
  function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
  }

  function truncate(str, max) {
    if (!str) return '';
    return str.length > max ? str.substring(0, max - 1) + '…' : str;
  }

  // Initial dashboard load
  refreshDashboard();
  loadGraphData();
});
