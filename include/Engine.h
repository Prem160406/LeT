#ifndef ENGINE_H
#define ENGINE_H

#include "Asset.h"
#include "CircularQueue.h"
#include "HashTable.h"
#include "DoublyLinkedList.h"
#include "MaxHeap.h"
#include "AVLTree.h"
#include "UndoStack.h"
#include "MergeSort.h"
#include "Graph.h"
#include "Common.h"
#include <string>

/**
 * @brief Core Pipeline Engine coordinating all 8 data structures.
 * 
 * Implemented Scope (70%):
 * - Steps 1-4 fully wired:
 *   1. Incoming reports buffer in CircularQueue.
 *   2. Target asset resolved in HashTable (O(1)) and appended to its DoublyLinkedList.
 *   3. Drift rate computed from recent history nodes: Delta(Dev) / Delta(Time).
 *   4. Risk Score computed and task prioritized in Max-Heap.
 * - UndoStack rollback for all state changes.
 * - Graph BFS/DFS available standalone.
 * - MergeSort available standalone.
 * 
 * Stubbed Scope (30% Handoff Safety):
 * - Step 5 (Blast radius factored into live risk score) clearly stubbed with specifications.
 * - Compliance report generation using MergeSort clearly stubbed.
 * - Calibration schedule window querying clearly stubbed.
 */
class Engine {
private:
    HashTable assetRegistry;        // 1. O(1) asset lookup
    CircularQueue intakeBuffer;     // 5. O(1) FIFO intake buffer
    MaxHeap triageQueue;           // 3. Max-Heap priority queue
    AVLTree calibrationIndex;       // 4. AVL Tree indexed by next_calibration_due
    UndoStack undoManager;          // 6. LIFO undo stack
    Graph dependencyGraph;          // 8. Asset dependency graph (blast-radius)

    long long currentSystemTime;
    size_t processedReportsCount;

public:
    explicit Engine(int intakeCapacity = 64);
    ~Engine();

    // =========================================================================
    // PIPELINE STEP 1: Intake Buffering
    // =========================================================================
    /**
     * @brief Stages an incoming service desk report into CircularQueue.
     * Time Complexity: O(1).
     * @return true if enqueued, false if queue full.
     */
    bool submitReportToBuffer(const FaultReport& report);

    // =========================================================================
    // PIPELINE STEPS 2, 3, 4: Ingestion, Drift, Risk, & Max-Heap Triage
    // =========================================================================
    /**
     * @brief Pulls next report from CircularQueue, performs O(1) HashTable lookup,
     * appends to Asset DLL, computes Drift Velocity, calculates Risk Score,
     * and inserts into Max-Heap.
     * 
     * Time Complexity:
     * - CircularQueue dequeue: O(1)
     * - HashTable find: Average O(1)
     * - DLL append: O(1)
     * - Drift computation: O(1)
     * - Risk calculation: O(1)
     * - MaxHeap insert: O(log N)
     * Overall Time Complexity: O(log N).
     * 
     * @return true if a report was successfully processed, false if buffer empty or asset not found.
     */
    bool processNextBufferReport();

    /**
     * @brief Processes all pending reports currently buffered in the CircularQueue.
     * Time Complexity: O(K log N) where K is number of buffered reports.
     */
    int processAllBufferedReports();

    // =========================================================================
    // ASSET REGISTRATION & LOOKUP
    // =========================================================================
    /**
     * @brief Registers a new laboratory equipment asset into the engine.
     * Inserts into HashTable (O(1)) and AVL Tree (O(log N)).
     * Pushes rollback record to UndoStack.
     * Time Complexity: O(log N) dominated by AVL insertion.
     */
    bool registerAsset(const Asset& asset);

    /**
     * @brief Finds asset pointer in HashTable.
     * Time Complexity: Average O(1).
     */
    Asset* getAsset(const std::string& assetCode);

    /**
     * @brief Retrieves all registered assets.
     */
    void getAllAssets(CustomArray<Asset*>& outAssets);

    // =========================================================================
    // TRIAGE ACTIONS
    // =========================================================================
    /**
     * @brief Inspects the highest urgency fault without dispatching it.
     * Time Complexity: O(1) root inspection.
     */
    bool peekHighestUrgencyTask(TriageTask& outTask) const;

    /**
     * @brief Dispatches highest-urgency maintenance task from Max-Heap.
     * Records action on UndoStack for rollback capability.
     * Time Complexity: O(log N).
     */
    bool dispatchHighestUrgencyTask(TriageTask& outTask);

    /**
     * @brief Retrieves snapshot of all pending triage tasks for dashboard display.
     * Time Complexity: O(N).
     */
    void getTriageTasksSnapshot(CustomArray<TriageTask>& outTasks) const;

    // =========================================================================
    // UNDO / ROLLBACK
    // =========================================================================
    /**
     * @brief Rolls back the most recent state-changing transaction.
     * Time Complexity: O(log N) for heap restoration or O(1) for DLL rollback.
     */
    bool rollbackLastAction();

    size_t getUndoStackDepth() const;

    // =========================================================================
    // DIRECT GRAPH & SORT ACCESS (Standalone 70% Scope)
    // =========================================================================
    Graph& getGraph() { return dependencyGraph; }
    const Graph& getGraph() const { return dependencyGraph; }

    AVLTree& getAVLTree() { return calibrationIndex; }
    const AVLTree& getAVLTree() const { return calibrationIndex; }

    CircularQueue& getIntakeQueue() { return intakeBuffer; }
    const CircularQueue& getIntakeQueue() const { return intakeBuffer; }

    MaxHeap& getMaxHeap() { return triageQueue; }
    const MaxHeap& getMaxHeap() const { return triageQueue; }

    HashTable& getHashTable() { return assetRegistry; }
    const HashTable& getHashTable() const { return assetRegistry; }

    // =========================================================================
    // PIPELINE STEP 5: Blast-Radius Prioritization Integration
    // =========================================================================
    /**
     * @brief Computes blast-radius multiplier factor from Graph BFS.
     * Formula: 1.0 + (0.20 * dependentCount)
     * Time Complexity: O(V + E) BFS traversal.
     */
    double calculateBlastRadiusFactor(const std::string& assetCode);

    // =========================================================================
    // COMPREHENSIVE REPORT GENERATION ENGINE
    // =========================================================================
    /**
     * @brief Generates sorted compliance audit report using MergeSort.
     * Time Complexity: O(N log N).
     */
    void generateComplianceReport(SortCriterion criterion, CustomArray<Asset*>& outReport);

    /**
     * @brief Generates faulty equipment report sorted by downtime or fault count.
     * Filters assets with non-zero faults or downtime, then sorts via MergeSort.
     * Time Complexity: O(N log N).
     */
    void getFaultyEquipmentReport(SortCriterion criterion, CustomArray<Asset*>& outFaulty);

    /**
     * @brief Generates root source & single point of failure (SPOF) report.
     * Analyzes out-degree, in-degree, and transitive blast radius from Graph.
     * Time Complexity: O(V * (V + E)).
     */
    void getSourceEquipmentReport(CustomArray<SourceAnalysisInfo>& outSources);

    /**
     * @brief Range query for upcoming calibrations via self-balancing AVL Tree.
     * Time Complexity: O(log N + K) where K is number of items in range.
     */
    void getUpcomingCalibrations(long long fromDate, long long toDate, 
                                 CustomArray<CalibrationItem>& outItems);

    /**
     * @brief Simulates a critical equipment failure cascade across the engine.
     * Dispatches report through intake queue, updates history, evaluates blast radius,
     * boosts risk score, and places top task in Max-Heap.
     */
    bool simulateFailureCascade(const std::string& assetCode, TriageTask& outTask,
                                CustomArray<std::string>& outAffected);
};

#endif // ENGINE_H
