#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include "Asset.h"
#include <stdexcept>
#include <string>

/**
 * @brief Fixed-capacity, memory-bounded O(1) FIFO buffer for incoming service desk requests.
 * Acts as the entry-point staging area before hash table resolution and processing.
 * 
 * Space Complexity: O(Capacity) pre-allocated memory.
 * All operations (enqueue, dequeue, peek) run strictly in O(1) time.
 */
class CircularQueue {
private:
    FaultReport* buffer;
    int capacity;
    int head;       // Front element index
    int tail;       // Next insertion index
    int count;      // Current number of queued items

public:
    /**
     * @brief Constructs circular queue with fixed capacity.
     * Time Complexity: O(Capacity) for memory allocation.
     * Space Complexity: O(Capacity).
     */
    explicit CircularQueue(int cap = 64);

    /**
     * @brief Destructor deallocating buffer.
     * Time Complexity: O(1).
     */
    ~CircularQueue();

    // Disable copy to avoid accidental duplicate ownership
    CircularQueue(const CircularQueue&) = delete;
    CircularQueue& operator=(const CircularQueue&) = delete;

    /**
     * @brief Enqueues a report into the circular buffer.
     * Time Complexity: O(1) direct array write with modulo arithmetic.
     * @return true if enqueued, false if buffer is full (bounded memory protection).
     */
    bool enqueue(const FaultReport& report);

    /**
     * @brief Dequeues the oldest report from the circular buffer.
     * Time Complexity: O(1) index increment with modulo arithmetic.
     * @param outReport Output parameter receiving the dequeued report.
     * @return true if successfully dequeued, false if queue is empty.
     */
    bool dequeue(FaultReport& outReport);

    /**
     * @brief Peeks at the front report without removing it.
     * Time Complexity: O(1).
     */
    bool peek(FaultReport& outReport) const;

    /**
     * @brief Returns true if queue is full.
     * Time Complexity: O(1).
     */
    bool isFull() const;

    /**
     * @brief Returns true if queue is empty.
     * Time Complexity: O(1).
     */
    bool isEmpty() const;

    /**
     * @brief Returns current number of elements.
     * Time Complexity: O(1).
     */
    int size() const;

    /**
     * @brief Returns maximum capacity.
     * Time Complexity: O(1).
     */
    int getCapacity() const;

    /**
     * @brief Clears the queue in O(1) by resetting pointers.
     * Time Complexity: O(1).
     */
    void clear();
};

#endif // CIRCULAR_QUEUE_H
