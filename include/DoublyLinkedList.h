#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

#include "Asset.h"
#include <cstddef>
#include <string>

/**
 * @brief Node for custom Doubly Linked List.
 * Stores interleaved fault and calibration history entries.
 */
struct DLLNode {
    HistoryEntry data;
    DLLNode* prev;
    DLLNode* next;

    DLLNode(const HistoryEntry& entry, DLLNode* p = nullptr, DLLNode* n = nullptr)
        : data(entry), prev(p), next(n) {}
};

/**
 * @brief Custom Doubly Linked List for asset maintenance history & trend analysis.
 * 
 * Supports O(1) tail appends (latest events) and bidirectional traversal
 * for linear drift rate calculation and trend analysis.
 */
class DoublyLinkedList {
private:
    DLLNode* head;
    DLLNode* tail;
    size_t count;

public:
    DoublyLinkedList();
    ~DoublyLinkedList();

    DoublyLinkedList(const DoublyLinkedList& other);
    DoublyLinkedList& operator=(const DoublyLinkedList& other);
    DoublyLinkedList(DoublyLinkedList&& other) noexcept;
    DoublyLinkedList& operator=(DoublyLinkedList&& other) noexcept;

    /**
     * @brief Appends a new history entry at the tail (latest chronological event).
     * Time Complexity: O(1) via direct tail pointer update.
     */
    void append(const HistoryEntry& entry);

    /**
     * @brief Prepends a history entry at the head.
     * Time Complexity: O(1) via direct head pointer update.
     */
    void prepend(const HistoryEntry& entry);

    /**
     * @brief Removes the latest entry from the tail (used during undo rollback).
     * Time Complexity: O(1).
     * @return true if an entry was removed, false if list was empty.
     */
    bool removeTail();

    /**
     * @brief Computes linear drift rate between the latest two history nodes:
     * Drift Rate = Delta(Deviation) / Delta(Time)
     * 
     * If fewer than 2 history entries exist, returns 0.0.
     * Time Complexity: O(1) by accessing tail and tail->prev.
     */
    double computeLatestDriftRate() const;

    /**
     * @brief Computes average drift velocity across the last K entries.
     * Time Complexity: O(K) backward traversal from tail.
     */
    double computeWindowDriftRate(size_t windowSize = 3) const;

    /**
     * @brief Returns pointer to the latest history entry (at tail).
     * Time Complexity: O(1).
     */
    const HistoryEntry* getLatestEntry() const;

    /**
     * @brief Returns current total history count.
     * Time Complexity: O(1).
     */
    size_t size() const;

    /**
     * @brief Returns true if no history records exist.
     * Time Complexity: O(1).
     */
    bool empty() const;

    /**
     * @brief Deallocates all nodes and resets count.
     * Time Complexity: O(N).
     */
    void clear();

    /**
     * @brief Prints history from oldest (head) to newest (tail).
     * Time Complexity: O(N).
     */
    void printForward() const;

    /**
     * @brief Prints history from newest (tail) to oldest (head).
     * Time Complexity: O(N).
     */
    void printBackward() const;
};

#endif // DOUBLY_LINKED_LIST_H
