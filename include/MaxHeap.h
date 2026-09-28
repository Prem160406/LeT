#ifndef MAX_HEAP_H
#define MAX_HEAP_H

#include "Asset.h"
#include "Common.h"
#include <cstddef>
#include <cmath>

/**
 * @brief Custom Max-Heap Priority Queue for emergency fault triage.
 * 
 * Orders tasks by composite Risk Score:
 * Risk = 3*Severity + 2*DaysOverdue + 5*|DriftVelocity|
 * 
 * ASSUMPTION: absolute value applied to Drift Velocity term so that negative drift 
 * does not reduce urgency — deviates from literal synopsis formula by design.
 * 
 * Provides:
 * - O(1) root access to highest-urgency task (peekMax)
 * - O(log N) insertion (siftUp)
 * - O(log N) extraction (siftDown)
 */
class MaxHeap {
private:
    TriageTask* data;
    size_t capacity;
    size_t count;

    void resize(size_t newCap);
    void siftUp(size_t index);
    void siftDown(size_t index);

    static size_t parent(size_t i) { return (i - 1) / 2; }
    static size_t leftChild(size_t i) { return 2 * i + 1; }
    static size_t rightChild(size_t i) { return 2 * i + 2; }

public:
    explicit MaxHeap(size_t initialCapacity = 32);
    ~MaxHeap();

    MaxHeap(const MaxHeap& other);
    MaxHeap& operator=(const MaxHeap& other);
    MaxHeap(MaxHeap&& other) noexcept;
    MaxHeap& operator=(MaxHeap&& other) noexcept;

    /**
     * @brief Inserts a new triage task into the heap.
     * Time Complexity: O(log N) worst case during siftUp; O(1) average.
     */
    void insert(const TriageTask& task);

    /**
     * @brief Peeks at the maximum risk task without removal.
     * Time Complexity: O(1) direct root inspection.
     * @return true if root exists, false if heap is empty.
     */
    bool peekMax(TriageTask& outTask) const;

    /**
     * @brief Extracts and removes the maximum risk task.
     * Time Complexity: O(log N) via siftDown from root.
     * @return true if extracted, false if heap was empty.
     */
    bool extractMax(TriageTask& outTask);

    /**
     * @brief Removes a task by its taskId (used for undo rollback).
     * Time Complexity: O(N) linear search + O(log N) sift down/up.
     * @return true if task was found and removed.
     */
    bool removeByTaskId(const std::string& taskId);

    /**
     * @brief Returns current number of active triage tasks.
     * Time Complexity: O(1).
     */
    size_t size() const;

    /**
     * @brief Checks if heap is empty.
     * Time Complexity: O(1).
     */
    bool empty() const;

    /**
     * @brief Clears all tasks.
     * Time Complexity: O(1).
     */
    void clear();

    /**
     * @brief Returns a snapshot of tasks in level-order for UI display.
     * Time Complexity: O(N).
     */
    void getTasksSnapshot(CustomArray<TriageTask>& outTasks) const;

    /**
     * @brief Helper function to compute Risk Score according to design formula.
     * Risk = 3*Severity + 2*DaysOverdue + 5*|DriftVelocity|
     * 
     * ASSUMPTION: absolute value applied to Drift Velocity term so that negative drift 
     * does not reduce urgency — deviates from literal synopsis formula by design.
     */
    static double computeRiskScore(int severity, int daysOverdue, double driftVelocity) {
        // ASSUMPTION: absolute value applied to Drift Velocity term so that negative drift 
        // does not reduce urgency — deviates from literal synopsis formula by design.
        return (3.0 * static_cast<double>(severity)) 
             + (2.0 * static_cast<double>(daysOverdue)) 
             + (5.0 * std::fabs(driftVelocity));
    }
};

#endif // MAX_HEAP_H
