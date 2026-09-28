document.addEventListener('DOMContentLoaded', () => {
  // Elements
  const formAddAsset = document.getElementById('form-add-asset');
  const formSubmitReport = document.getElementById('form-submit-report');
  const btnRefresh = document.getElementById('btn-refresh');
  const btnUndo = document.getElementById('btn-undo');
  const btnDispatch = document.getElementById('btn-dispatch');
  const triageBody = document.getElementById('triage-body');
  
  const valAssetCount = document.getElementById('val-asset-count');
  const valTriageCount = document.getElementById('val-triage-count');
  const valUndoDepth = document.getElementById('val-undo-depth');
  const undoCountSpan = document.getElementById('undo-count');
  
  const btnBlastRadius = document.getElementById('btn-blast-radius');
  const blastInput = document.getElementById('graph-asset');
  const blastResults = document.getElementById('blast-results');

  const btnRunSort = document.getElementById('btn-run-sort');
  const sortCriterion = document.getElementById('sort-criterion');
  const sortResults = document.getElementById('sort-results');

  // Fetch and update dashboard state
  async function refreshDashboard() {
    try {
      // 1. Fetch Assets
      const assetsRes = await fetch('/api/assets');
      if (assetsRes.ok) {
        const assets = await assetsRes.json();
        valAssetCount.textContent = assets.length;
      }

      // 2. Fetch Triage Tasks
      const triageRes = await fetch('/api/triage');
      if (triageRes.ok) {
        const tasks = await triageRes.json();
        valTriageCount.textContent = tasks.length;
        renderTriageTable(tasks);
      }
    } catch (err) {
      console.error('Error refreshing dashboard:', err);
    }
  }

  // Render Max-Heap Triage Table
  function renderTriageTable(tasks) {
    if (!tasks || tasks.length === 0) {
      triageBody.innerHTML = `<tr><td colspan="9" class="empty-state">No triage tasks in queue. Submit a fault report above.</td></tr>`;
      return;
    }

    triageBody.innerHTML = tasks.map((task, idx) => {
      const isRoot = (idx === 0);
      return `
        <tr class="${isRoot ? 'root-task' : ''}">
          <td>
            ${isRoot 
              ? '<span class="badge badge-root">ROOT (MAX RISK)</span>' 
              : '#' + (idx + 1)}
          </td>
          <td><code>${escapeHtml(task.taskId)}</code></td>
          <td><strong>${escapeHtml(task.assetCode)}</strong></td>
          <td>${escapeHtml(task.assetName || '-')}</td>
          <td>${task.severity} / 5</td>
          <td>${task.daysOverdue} d</td>
          <td>${Number(task.driftVelocity).toFixed(4)}/s</td>
          <td><span class="badge badge-risk">${Number(task.riskScore).toFixed(2)}</span></td>
          <td>${escapeHtml(task.description || '-')}</td>
        </tr>
      `;
    }).join('');
  }

  // 1. Submit Add Asset Form
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
        alert('Success: ' + data.message);
        formAddAsset.reset();
        refreshDashboard();
      } else {
        alert('Error: ' + data.message);
      }
    } catch (err) {
      alert('Network error registering asset.');
    }
  });

  // 2. Submit Fault Report Form
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
        alert('Triage Successful: Enqueued into Circular Queue -> DLL History Updated -> Prioritized into Max-Heap!');
        formSubmitReport.reset();
        refreshDashboard();
      } else {
        alert('Triage Failed: ' + data.message);
      }
    } catch (err) {
      alert('Network error submitting report.');
    }
  });

  // 3. Dispatch Highest Urgency Task
  btnDispatch.addEventListener('click', async () => {
    try {
      const res = await fetch('/api/dispatch', { method: 'POST' });
      const data = await res.json();
      if (res.ok) {
        alert(`Dispatched Highest Urgency Job:\nTask: ${data.dispatched.taskId}\nAsset: ${data.dispatched.assetCode}\nRisk Score: ${data.dispatched.riskScore}`);
        refreshDashboard();
      } else {
        alert(data.message);
      }
    } catch (err) {
      alert('Error dispatching task.');
    }
  });

  // 4. Undo Last Action (LIFO Stack Rollback)
  btnUndo.addEventListener('click', async () => {
    try {
      const res = await fetch('/api/undo', { method: 'POST' });
      const data = await res.json();
      if (res.ok) {
        alert('Undo Rollback Succeeded: ' + data.message);
        valUndoDepth.textContent = data.remainingUndo;
        undoCountSpan.textContent = data.remainingUndo;
        refreshDashboard();
      } else {
        alert('Undo Notice: ' + data.message);
      }
    } catch (err) {
      alert('Error triggering undo.');
    }
  });

  // 5. Standalone Blast-Radius BFS Demo
  btnBlastRadius.addEventListener('click', async () => {
    const code = blastInput.value.trim() || 'LAB-UPS-01';
    blastResults.textContent = 'Traversing dependency graph via BFS...';
    try {
      const res = await fetch(`/api/blast-radius-demo?asset=${encodeURIComponent(code)}`);
      const data = await res.json();
      if (res.ok) {
        let text = `[Graph BFS Traversal Result]\nSource Asset: ${data.sourceAsset}\nDownstream Impact Count: ${data.bfsCount} assets affected\n\nDirect & Indirect Dependents (Blast Radius):\n`;
        if (data.bfsAffected.length === 0) {
          text += '  (None - isolated terminal asset)\n';
        } else {
          data.bfsAffected.forEach((node, i) => {
            text += `  ${i + 1}. [${node}]\n`;
          });
        }
        blastResults.textContent = text;
      } else {
        blastResults.textContent = 'Error computing blast radius.';
      }
    } catch (err) {
      blastResults.textContent = 'Network error during BFS query.';
    }
  });

  // 6. Standalone Merge Sort Demo
  btnRunSort.addEventListener('click', async () => {
    const crit = sortCriterion.value;
    sortResults.textContent = 'Executing O(N log N) recursive Merge Sort...';
    try {
      const res = await fetch(`/api/compliance-sort-demo?criterion=${encodeURIComponent(crit)}`);
      const data = await res.json();
      if (res.ok) {
        let text = `[Merge Sort Result — O(N log N) Divide & Conquer]\nCriterion: ${crit}\nTotal Sorted Records: ${data.length}\n\n`;
        data.forEach((item, i) => {
          text += `${i + 1}. [${item.assetCode}] ${item.name} | Downtime: ${item.totalDowntimeHours}h | Faults: ${item.totalFaultCount} | CalibDue: ${item.nextCalibrationDue}d\n`;
        });
        sortResults.textContent = text;
      } else {
        sortResults.textContent = 'Error executing Merge Sort.';
      }
    } catch (err) {
      sortResults.textContent = 'Network error during sort query.';
    }
  });

  btnRefresh.addEventListener('click', refreshDashboard);

  function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
  }

  // Initial load
  refreshDashboard();
});
