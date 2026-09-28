#include "Engine.h"
#include <iostream>
#include <cmath>

Engine::Engine(int intakeCapacity)
    : assetRegistry(61), intakeBuffer(intakeCapacity), triageQueue(64),
      calibrationIndex(), undoManager(), dependencyGraph(32),
      currentSystemTime(1000), processedReportsCount(0) {}

Engine::~Engine() {}

// =============================================================================
// PIPELINE STEP 1: Intake Buffering
// =============================================================================
bool Engine::submitReportToBuffer(const FaultReport& report) {
    // Time Complexity: O(1)
    return intakeBuffer.enqueue(report);
}

// =============================================================================
// PIPELINE STEPS 2, 3, 4: Ingestion -> Lookup -> DLL Append -> Drift -> Heap
// =============================================================================
bool Engine::processNextBufferReport() {
    FaultReport report;
    // Step 1: Dequeue from bounded circular buffer (O(1))
    if (!intakeBuffer.dequeue(report)) {
        return false;
    }

    // Step 2: O(1) Hash Table lookup by unique asset code
    Asset* targetAsset = assetRegistry.find(report.assetCode);
    if (targetAsset == nullptr) {
        std::cerr << "[Engine Error] Asset not found in registry: " << report.assetCode << std::endl;
        return false;
    }

    // Step 3: Compute Linear Drift Rate from recent history nodes:
    // Drift Rate = Delta(Deviation) / Delta(Time)
    double driftVelocity = 0.0;
    const HistoryEntry* prevEntry = targetAsset->history->getLatestEntry();
    if (prevEntry != nullptr) {
        double deltaDev = report.measuredDeviation - prevEntry->measuredDeviation;
        long long deltaTime = report.timestamp - prevEntry->timestamp;
        if (deltaTime != 0) {
            driftVelocity = deltaDev / static_cast<double>(deltaTime);
        }
    }

    // Append to asset's Doubly Linked List (O(1))
    HistoryEntry newEntry(ENTRY_FAULT, report.reportId, report.timestamp, 
                          report.measuredDeviation, report.severity, 
                          report.description, driftVelocity);
    targetAsset->history->append(newEntry);
    targetAsset->totalFaultCount++;

    // Step 4: Calculate Composite Risk Score and insert into Max-Heap
    // Formula: Risk = 3*Severity + 2*DaysOverdue + 5*|DriftVelocity|
    // ASSUMPTION: absolute value applied to Drift Velocity term so that negative drift 
    // does not reduce urgency — deviates from literal synopsis formula by design.
    double riskScore = MaxHeap::computeRiskScore(report.severity, report.daysOverdue, driftVelocity);

    TriageTask task(report.reportId, targetAsset->assetCode, targetAsset->name,
                   report.severity, report.daysOverdue, driftVelocity, riskScore,
                   report.timestamp, report.description);
    
    // Insert into Max-Heap priority queue (O(log N))
    triageQueue.insert(task);
    ++processedReportsCount;

    // Push snapshot to Undo Stack for O(1) rollback capability
    UndoRecord rec(UNDO_FAULT_REPORT, targetAsset->assetCode, task.taskId,
                   report.timestamp, report.measuredDeviation, report.severity,
                   report.description);
    undoManager.push(rec);

    return true;
}

int Engine::processAllBufferedReports() {
    int processed = 0;
    while (!intakeBuffer.isEmpty()) {
        if (processNextBufferReport()) {
            ++processed;
        } else {
            break;
        }
    }
    return processed;
}

// =============================================================================
// ASSET REGISTRATION & LOOKUP
// =============================================================================
bool Engine::registerAsset(const Asset& asset) {
    // Insert into O(1) Hash Table
    bool inserted = assetRegistry.insert(asset);
    if (!inserted) {
        return false; // Already existed
    }

    // Index next calibration due date into AVL Tree (O(log N))
    if (asset.nextCalibrationDue > 0) {
        calibrationIndex.insert(asset.nextCalibrationDue, asset.assetCode, asset.name);
    }

    // Add node to Dependency Graph
    dependencyGraph.addVertex(asset.assetCode);

    // Push to Undo Stack
    UndoRecord rec(UNDO_ASSET_REGISTRATION, asset.assetCode);
    undoManager.push(rec);

    return true;
}

Asset* Engine::getAsset(const std::string& assetCode) {
    return assetRegistry.find(assetCode);
}

void Engine::getAllAssets(CustomArray<Asset*>& outAssets) {
    assetRegistry.getAllAssets(outAssets);
}

// =============================================================================
// TRIAGE ACTIONS
// =============================================================================
bool Engine::peekHighestUrgencyTask(TriageTask& outTask) const {
    return triageQueue.peekMax(outTask);
}

bool Engine::dispatchHighestUrgencyTask(TriageTask& outTask) {
    if (!triageQueue.extractMax(outTask)) {
        return false;
    }

    // Push to Undo Stack for rollback
    UndoRecord rec(UNDO_TRIAGE_DISPATCH, outTask.assetCode, outTask.taskId,
                   outTask.timestamp, 0.0, outTask.severity, outTask.description);
    undoManager.push(rec);

    return true;
}

void Engine::getTriageTasksSnapshot(CustomArray<TriageTask>& outTasks) const {
    triageQueue.getTasksSnapshot(outTasks);
}

// =============================================================================
// UNDO / ROLLBACK
// =============================================================================
bool Engine::rollbackLastAction() {
    UndoRecord rec;
    if (!undoManager.pop(rec)) {
        return false;
    }

    switch (rec.type) {
        case UNDO_ASSET_REGISTRATION: {
            Asset* a = assetRegistry.find(rec.assetCode);
            if (a != nullptr) {
                if (a->nextCalibrationDue > 0) {
                    calibrationIndex.remove(a->nextCalibrationDue, a->assetCode);
                }
                assetRegistry.remove(rec.assetCode);
            }
            return true;
        }
        case UNDO_FAULT_REPORT: {
            Asset* a = assetRegistry.find(rec.assetCode);
            if (a != nullptr) {
                a->history->removeTail();
                if (a->totalFaultCount > 0) {
                    --a->totalFaultCount;
                }
            }
            triageQueue.removeByTaskId(rec.taskId);
            return true;
        }
        case UNDO_TRIAGE_DISPATCH: {
            // Restore dispatched task back into triage heap
            Asset* a = assetRegistry.find(rec.assetCode);
            std::string name = a ? a->name : "Unknown";
            double drift = 0.0;
            if (a && a->history->getLatestEntry()) {
                drift = a->history->getLatestEntry()->computedDrift;
            }
            double risk = MaxHeap::computeRiskScore(rec.severity, 0, drift);
            TriageTask restoredTask(rec.taskId, rec.assetCode, name, rec.severity,
                                   0, drift, risk, rec.timestamp, rec.description);
            triageQueue.insert(restoredTask);
            return true;
        }
    }
    return false;
}

size_t Engine::getUndoStackDepth() const {
    return undoManager.size();
}

// =============================================================================
// DELIBERATELY STUBBED ITEMS (HANDOFF SAFETY REQUIREMENTS)
// =============================================================================

/**
 * ============================================================================
 * [HANDOFF STUB 1] Pipeline Step 5: Blast-Radius Multiplier in Fault Prioritization
 * ============================================================================
 * (a) INPUTS / OUTPUTS:
 *     - Inputs:  const std::string& assetCode (Primary key of failed asset)
 *     - Outputs: double blastMultiplier (Weight multiplier >= 1.0)
 *
 * (b) FUNCTIONS & STRUCTURES CALLED:
 *     - dependencyGraph.bfs(assetCode, reachableAssets) [src/Graph.cpp]
 *     - reachableAssets.size() [include/Common.h]
 *
 * (c) FINALIZED DESIGN DECISION:
 *     - When a critical fault occurs (severity >= 4), the live triage risk score
 *       must be augmented by:
 *           EffectiveRisk = BaseRisk * (1.0 + 0.20 * dependentCount)
 *     - In this 70% milestone, the Graph and BFS/DFS are fully implemented and
 *       tested standalone, but NOT yet injected into processNextBufferReport()
 *       to keep pipeline steps 1-4 isolated and cleanly verified.
 *
 * (d) EXACT NEXT ACTION FOR RESUMING DEVELOPER:
 *     1. Uncomment the BFS call below: dependencyGraph.bfs(assetCode, affected);
 *     2. In processNextBufferReport(), multiply riskScore by calculateBlastRadiusFactor(targetAsset->assetCode).
 *     3. Recompile and verify via `main.cpp` demo suite.
 * ============================================================================
 */
double Engine::calculateBlastRadiusFactor(const std::string& assetCode) {
    // STUB IMPLEMENTATION (Returns neutral 1.0 multiplier until Step 5 wiring is enabled)
    // To complete:
    // CustomArray<std::string> affected;
    // dependencyGraph.bfs(assetCode, affected);
    // return 1.0 + (0.20 * static_cast<double>(affected.size()));
    (void)assetCode;
    return 1.0;
}

/**
 * ============================================================================
 * [HANDOFF STUB 2] Compliance Reporting Feature built on top of Merge Sort
 * ============================================================================
 * (a) INPUTS / OUTPUTS:
 *     - Inputs:  SortCriterion criterion (e.g. SORT_BY_DOWNTIME_DESC, SORT_BY_FAULT_COUNT_DESC)
 *     - Outputs: CustomArray<Asset*>& outReport (Sorted list of asset pointers)
 *
 * (b) FUNCTIONS & STRUCTURES CALLED:
 *     - assetRegistry.getAllAssets(allAssets) [src/HashTable.cpp]
 *     - MergeSort::sort(outReport, criterion) [src/MergeSort.cpp]
 *
 * (c) FINALIZED DESIGN DECISION:
 *     - MergeSort is fully implemented and tested standalone in Phase 2.
 *     - In this 70% milestone, the business report formatting and UI presentation
 *       are deferred.
 *
 * (d) EXACT NEXT ACTION FOR RESUMING DEVELOPER:
 *     1. Call assetRegistry.getAllAssets(outReport);
 *     2. Call MergeSort::sort(outReport, criterion);
 *     3. Expose endpoint `GET /api/compliance-report?sort=downtime` in HttpServer.cpp.
 * ============================================================================
 */
void Engine::generateComplianceReport(SortCriterion criterion, CustomArray<Asset*>& outReport) {
    // STUB IMPLEMENTATION
    // To complete:
    assetRegistry.getAllAssets(outReport);
    MergeSort::sort(outReport, criterion);
}

/**
 * ============================================================================
 * [HANDOFF STUB 3] Frontend View for AVL-Driven Calibration Scheduling
 * ============================================================================
 * (a) INPUTS / OUTPUTS:
 *     - Inputs:  long long fromDate, long long toDate (Epoch timestamps)
 *     - Outputs: CustomArray<CalibrationItem>& outItems (Calibrations in date range)
 *
 * (b) FUNCTIONS & STRUCTURES CALLED:
 *     - calibrationIndex.getRange(fromDate, toDate, outItems) [src/AVLTree.cpp]
 *
 * (c) FINALIZED DESIGN DECISION:
 *     - AVL Tree range query runs in O(log N + K).
 *     - Calibration records are indexed during asset registration.
 *
 * (d) EXACT NEXT ACTION FOR RESUMING DEVELOPER:
 *     1. Invoke calibrationIndex.getRange(fromDate, toDate, outItems);
 *     2. Add frontend date-picker component in index.html.
 * ============================================================================
 */
void Engine::getUpcomingCalibrations(long long fromDate, long long toDate,
                                     CustomArray<CalibrationItem>& outItems) {
    // STUB IMPLEMENTATION
    // To complete:
    calibrationIndex.getRange(fromDate, toDate, outItems);
}
