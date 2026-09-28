#include "CircularQueue.h"

// CircularQueue implementation

CircularQueue::CircularQueue(int cap)
    : capacity(cap > 0 ? cap : 64), head(0), tail(0), count(0) {
    buffer = new FaultReport[capacity];
}

CircularQueue::~CircularQueue() {
    delete[] buffer;
    buffer = nullptr;
}

/**
 * Time Complexity: O(1).
 * Directly writes at index 'tail' and advances tail = (tail + 1) % capacity.
 */
bool CircularQueue::enqueue(const FaultReport& report) {
    if (isFull()) {
        return false; // Bounded buffer protection: reject when full
    }
    buffer[tail] = report;
    tail = (tail + 1) % capacity;
    ++count;
    return true;
}

/**
 * Time Complexity: O(1).
 * Directly reads at index 'head' and advances head = (head + 1) % capacity.
 */
bool CircularQueue::dequeue(FaultReport& outReport) {
    if (isEmpty()) {
        return false;
    }
    outReport = buffer[head];
    head = (head + 1) % capacity;
    --count;
    return true;
}

/**
 * Time Complexity: O(1).
 * Reads element at index 'head' without state mutation.
 */
bool CircularQueue::peek(FaultReport& outReport) const {
    if (isEmpty()) {
        return false;
    }
    outReport = buffer[head];
    return true;
}

bool CircularQueue::isFull() const {
    return count == capacity;
}

bool CircularQueue::isEmpty() const {
    return count == 0;
}

int CircularQueue::size() const {
    return count;
}

int CircularQueue::getCapacity() const {
    return capacity;
}

void CircularQueue::clear() {
    head = 0;
    tail = 0;
    count = 0;
}
