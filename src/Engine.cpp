#include "Engine.h"
#include <iostream>
#include <cmath>
#include <ctime>

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

    // Step 4: Calculate Composite Base Risk Score
    // Formula: BaseRisk = 3*Severity + 2*DaysOverdue + 5*|DriftVelocity|
    // ASSUMPTION: absolute value applied to Drift Velocity term so that negative drift 
    // does not reduce urgency — deviates from literal synopsis formula by design.
    double baseRiskScore = MaxHeap::computeRiskScore(report.severity, report.daysOverdue, driftVelocity);

    // Step 5: Factoring Blast Radius from Dependency Graph (Transitive impact discovery)
    // Formula: EffectiveRisk = BaseRisk * (1.0 + 0.20 * dependentCount)
    int blastRadiusCount = dependencyGraph.getBlastRadiusCount(targetAsset->assetCode);
    double blastMultiplier = calculateBlastRadiusFactor(targetAsset->assetCode);
    double effectiveRiskScore = baseRiskScore * blastMultiplier;

    TriageTask task(report.reportId, targetAsset->assetCode, targetAsset->name,
                   report.severity, report.daysOverdue, driftVelocity, effectiveRiskScore,
                   report.timestamp, report.description, blastRadiusCount, blastMultiplier);
    
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
// =============================================================================
// PIPELINE STEP 5 & ADVANCED REPORT GENERATION IMPLEMENTATIONS
// =============================================================================

/**
 * Time Complexity: O(V + E).
 * Performs Breadth-First Search on Dependency Graph to find all directly & indirectly
 * affected downstream equipment, then computes the prioritization multiplier:
 * Formula: EffectiveRisk = BaseRisk * (1.0 + 0.20 * dependentCount)
 */
double Engine::calculateBlastRadiusFactor(const std::string& assetCode) {
    CustomArray<std::string> affected;
    dependencyGraph.bfs(assetCode, affected);
    return 1.0 + (0.20 * static_cast<double>(affected.size()));
}

/**
 * Time Complexity: O(N log N).
 * Collects all registered assets from Hash Table and sorts them via custom MergeSort.
 */
void Engine::generateComplianceReport(SortCriterion criterion, CustomArray<Asset*>& outReport) {
    assetRegistry.getAllAssets(outReport);
    MergeSort::sort(outReport, criterion);
}

/**
 * Time Complexity: O(N log N).
 * Gathers faulty equipment (non-zero fault count or accumulated downtime),
 * then sorts by chosen criteria (downtime, fault count, etc.) using MergeSort.
 */
void Engine::getFaultyEquipmentReport(SortCriterion criterion, CustomArray<Asset*>& outFaulty) {
    CustomArray<Asset*> allAssets;
    assetRegistry.getAllAssets(allAssets);
    outFaulty.clear();
    for (size_t i = 0; i < allAssets.size(); ++i) {
        if (allAssets[i]->totalFaultCount > 0 || allAssets[i]->totalDowntimeHours > 0.0) {
            outFaulty.push_back(allAssets[i]);
        }
    }
    MergeSort::sort(outFaulty, criterion);
}

/**
 * Time Complexity: O(V * (V + E)).
 * Leverages Graph adjacency lists and BFS to compute in-degree, out-degree,
 * transitive blast radius, and Single Point of Failure (SPOF) classification for all nodes.
 */
void Engine::getSourceEquipmentReport(CustomArray<SourceAnalysisInfo>& outSources) {
    dependencyGraph.analyzeAllSources(outSources);
}

/**
 * Time Complexity: O(log N + K).
 * Range query using self-balancing AVL Tree on calibration due dates.
 */
void Engine::getUpcomingCalibrations(long long fromDate, long long toDate,
                                     CustomArray<CalibrationItem>& outItems) {
    calibrationIndex.getRange(fromDate, toDate, outItems);
}

/**
 * @brief Simulates a critical catastrophic failure on a root equipment node.
 * Demonstrates end-to-end pipeline: Intake Enqueue -> Hash Lookup -> DLL Append ->
 * Drift Rate Calc -> Graph BFS Blast Radius Multiplication -> Max-Heap Insertion.
 */
bool Engine::simulateFailureCascade(const std::string& assetCode, TriageTask& outTask,
                                    CustomArray<std::string>& outAffected) {
    Asset* target = assetRegistry.find(assetCode);
    if (target == nullptr) {
        return false;
    }

    // Discover affected downstream assets via BFS
    dependencyGraph.bfs(assetCode, outAffected);

    // Formulate emergency cascade report
    long long now = static_cast<long long>(time(nullptr));
    std::string reportId = "CASCADE-SIM-" + std::to_string(rand() % 9000 + 1000);
    FaultReport cascadeReport(reportId, assetCode, 5, 14, 28.5, now,
                             "SIMULATED CATASTROPHIC ROOT FAILURE: Subsystem Offline");

    // Push into Circular Queue
    if (!submitReportToBuffer(cascadeReport)) {
        return false;
    }

    // Process through pipeline steps 1-5
    if (!processNextBufferReport()) {
        return false;
    }

    // Peek the newly inserted high-urgency task from Max-Heap
    return peekHighestUrgencyTask(outTask);
}
