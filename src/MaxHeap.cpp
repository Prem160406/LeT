#include "MaxHeap.h"
#include <utility>

MaxHeap::MaxHeap(size_t initialCapacity)
    : capacity(initialCapacity < 8 ? 8 : initialCapacity), count(0) {
    data = new TriageTask[capacity];
}

MaxHeap::MaxHeap(const TriageTask* tasks, size_t n)
    : capacity(n < 8 ? 8 : n), count(0) {
    data = new TriageTask[capacity];
    buildHeap(tasks, n);
}

MaxHeap::~MaxHeap() {
    delete[] data;
    data = nullptr;
}

MaxHeap::MaxHeap(const MaxHeap& other)
    : capacity(other.capacity), count(other.count) {
    data = new TriageTask[capacity];
    for (size_t i = 0; i < count; ++i) {
        data[i] = other.data[i];
    }
}

MaxHeap& MaxHeap::operator=(const MaxHeap& other) {
    if (this != &other) {
        delete[] data;
        capacity = other.capacity;
        count = other.count;
        data = new TriageTask[capacity];
        for (size_t i = 0; i < count; ++i) {
            data[i] = other.data[i];
        }
    }
    return *this;
}

MaxHeap::MaxHeap(MaxHeap&& other) noexcept
    : data(other.data), capacity(other.capacity), count(other.count) {
    other.data = nullptr;
    other.capacity = 0;
    other.count = 0;
}

MaxHeap& MaxHeap::operator=(MaxHeap&& other) noexcept {
    if (this != &other) {
        delete[] data;
        data = other.data;
        capacity = other.capacity;
        count = other.count;
        other.data = nullptr;
        other.capacity = 0;
        other.count = 0;
    }
    return *this;
}

void MaxHeap::resize(size_t newCap) {
    TriageTask* newData = new TriageTask[newCap];
    for (size_t i = 0; i < count; ++i) {
        newData[i] = std::move(data[i]);
    }
    delete[] data;
    data = newData;
    capacity = newCap;
}

/**
 * Time Complexity: O(log N).
 * Traverses up tree height exchanging node with its parent if its risk score is greater.
 */
void MaxHeap::siftUp(size_t index) {
    while (index > 0) {
        size_t p = parent(index);
        if (data[index].riskScore > data[p].riskScore) {
            std::swap(data[index], data[p]);
            index = p;
        } else {
            break;
        }
    }
}

/**
 * Time Complexity: O(log N).
 * Traverses down tree height exchanging node with the larger of its two children.
 */
void MaxHeap::siftDown(size_t index) {
    while (index < count) {
        size_t largest = index;
        size_t left = leftChild(index);
        size_t right = rightChild(index);

        if (left < count && data[left].riskScore > data[largest].riskScore) {
            largest = left;
        }
        if (right < count && data[right].riskScore > data[largest].riskScore) {
            largest = right;
        }

        if (largest != index) {
            std::swap(data[index], data[largest]);
            index = largest;
        } else {
            break;
        }
    }
}

/**
 * Time Complexity: O(N) using Floyd's bottom-up heap construction algorithm.
 * Copies tasks into contiguous memory and sifts down from the last internal node
 * (count / 2 - 1) to the root (0).
 */
void MaxHeap::buildHeap(const TriageTask* tasks, size_t n) {
    if (tasks == nullptr || n == 0) {
        count = 0;
        return;
    }

    if (n > capacity) {
        delete[] data;
        capacity = n < 8 ? 8 : n;
        data = new TriageTask[capacity];
    }

    for (size_t i = 0; i < n; ++i) {
        data[i] = tasks[i];
    }
    count = n;

    // Floyd's algorithm: Start at the parent of the last element and siftDown.
    // Leaves at indices [count/2 .. count-1] already satisfy the heap property trivially.
    if (count >= 2) {
        for (int i = static_cast<int>((count / 2) - 1); i >= 0; --i) {
            siftDown(static_cast<size_t>(i));
        }
    }
}

/**
 * Time Complexity: O(N) from CustomArray.
 */
void MaxHeap::buildHeap(const CustomArray<TriageTask>& tasks) {
    if (tasks.empty()) {
        count = 0;
        return;
    }
    size_t n = tasks.size();
    if (n > capacity) {
        delete[] data;
        capacity = n < 8 ? 8 : n;
        data = new TriageTask[capacity];
    }
    for (size_t i = 0; i < n; ++i) {
        data[i] = tasks[i];
    }
    count = n;

    if (count >= 2) {
        for (int i = static_cast<int>((count / 2) - 1); i >= 0; --i) {
            siftDown(static_cast<size_t>(i));
        }
    }
}

/**
 * Time Complexity: O(log N).
 * Places task at end of array and bubbles it up.
 */
void MaxHeap::insert(const TriageTask& task) {
    if (count >= capacity) {
        resize(capacity * 2);
    }
    data[count] = task;
    siftUp(count);
    ++count;
}

/**
 * Time Complexity: O(1).
 * Directly reads root element at index 0.
 */
bool MaxHeap::peekMax(TriageTask& outTask) const {
    if (empty()) {
        return false;
    }
    outTask = data[0];
    return true;
}

/**
 * Time Complexity: O(log N).
 * Replaces root with the last leaf element and restores heap invariant via siftDown.
 */
bool MaxHeap::extractMax(TriageTask& outTask) {
    if (empty()) {
        return false;
    }
    outTask = data[0];
    data[0] = data[count - 1];
    --count;
    if (count > 0) {
        siftDown(0);
    }
    return true;
}

/**
 * Time Complexity: O(log N).
 * Updates risk score at specified index and sifts up towards root.
 */
bool MaxHeap::increaseKey(size_t index, double newRiskScore) {
    if (index >= count) {
        return false;
    }
    if (newRiskScore < data[index].riskScore) {
        return false; // Cannot decrease key via increaseKey
    }
    data[index].riskScore = newRiskScore;
    siftUp(index);
    return true;
}

/**
 * Time Complexity: O(log N).
 * Updates risk score at specified index and sifts down towards leaves.
 */
bool MaxHeap::decreaseKey(size_t index, double newRiskScore) {
    if (index >= count) {
        return false;
    }
    if (newRiskScore > data[index].riskScore) {
        return false; // Cannot increase key via decreaseKey
    }
    data[index].riskScore = newRiskScore;
    siftDown(index);
    return true;
}

/**
 * Time Complexity: O(N) linear search.
 */
bool MaxHeap::findTaskIndex(const std::string& taskId, size_t& outIndex) const {
    for (size_t i = 0; i < count; ++i) {
        if (data[i].taskId == taskId) {
            outIndex = i;
            return true;
        }
    }
    return false;
}

/**
 * Time Complexity: O(N) search + O(log N) siftUp.
 */
bool MaxHeap::increaseKeyByTaskId(const std::string& taskId, double newRiskScore) {
    size_t idx = 0;
    if (!findTaskIndex(taskId, idx)) {
        return false;
    }
    return increaseKey(idx, newRiskScore);
}

/**
 * Time Complexity: O(N) search + O(log N) siftDown.
 */
bool MaxHeap::decreaseKeyByTaskId(const std::string& taskId, double newRiskScore) {
    size_t idx = 0;
    if (!findTaskIndex(taskId, idx)) {
        return false;
    }
    return decreaseKey(idx, newRiskScore);
}

/**
 * Time Complexity: O(N) to find + O(log N) to restore.
 */
bool MaxHeap::removeByTaskId(const std::string& taskId) {
    for (size_t i = 0; i < count; ++i) {
        if (data[i].taskId == taskId) {
            data[i] = data[count - 1];
            --count;
            if (i < count) {
                siftDown(i);
                siftUp(i);
            }
            return true;
        }
    }
    return false;
}

size_t MaxHeap::size() const {
    return count;
}

bool MaxHeap::empty() const {
    return count == 0;
}

void MaxHeap::clear() {
    count = 0;
}

void MaxHeap::getTasksSnapshot(CustomArray<TriageTask>& outTasks) const {
    outTasks.clear();
    for (size_t i = 0; i < count; ++i) {
        outTasks.push_back(data[i]);
    }
}
