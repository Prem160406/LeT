#include "MaxHeap.h"
#include <utility>

MaxHeap::MaxHeap(size_t initialCapacity)
    : capacity(initialCapacity < 8 ? 8 : initialCapacity), count(0) {
    data = new TriageTask[capacity];
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
