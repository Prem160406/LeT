#include "DoublyLinkedList.h"
#include <iostream>
#include <cmath>

DoublyLinkedList::DoublyLinkedList()
    : head(nullptr), tail(nullptr), count(0) {}

DoublyLinkedList::~DoublyLinkedList() {
    clear();
}

DoublyLinkedList::DoublyLinkedList(const DoublyLinkedList& other)
    : head(nullptr), tail(nullptr), count(0) {
    DLLNode* curr = other.head;
    while (curr != nullptr) {
        append(curr->data);
        curr = curr->next;
    }
}

DoublyLinkedList& DoublyLinkedList::operator=(const DoublyLinkedList& other) {
    if (this != &other) {
        clear();
        DLLNode* curr = other.head;
        while (curr != nullptr) {
            append(curr->data);
            curr = curr->next;
        }
    }
    return *this;
}

DoublyLinkedList::DoublyLinkedList(DoublyLinkedList&& other) noexcept
    : head(other.head), tail(other.tail), count(other.count) {
    other.head = nullptr;
    other.tail = nullptr;
    other.count = 0;
}

DoublyLinkedList& DoublyLinkedList::operator=(DoublyLinkedList&& other) noexcept {
    if (this != &other) {
        clear();
        head = other.head;
        tail = other.tail;
        count = other.count;
        other.head = nullptr;
        other.tail = nullptr;
        other.count = 0;
    }
    return *this;
}

/**
 * Time Complexity: O(1).
 * Appends to tail directly via pointer update.
 */
void DoublyLinkedList::append(const HistoryEntry& entry) {
    DLLNode* newNode = new DLLNode(entry, tail, nullptr);
    if (tail == nullptr) {
        head = newNode;
        tail = newNode;
    } else {
        tail->next = newNode;
        tail = newNode;
    }
    ++count;
}

/**
 * Time Complexity: O(1).
 * Prepends to head directly via pointer update.
 */
void DoublyLinkedList::prepend(const HistoryEntry& entry) {
    DLLNode* newNode = new DLLNode(entry, nullptr, head);
    if (head == nullptr) {
        head = newNode;
        tail = newNode;
    } else {
        head->prev = newNode;
        head = newNode;
    }
    ++count;
}

/**
 * Time Complexity: O(1).
 * Directly detaches tail node. Critical for rollback/undo operations.
 */
bool DoublyLinkedList::removeTail() {
    if (tail == nullptr) {
        return false;
    }
    DLLNode* oldTail = tail;
    if (head == tail) {
        head = nullptr;
        tail = nullptr;
    } else {
        tail = tail->prev;
        tail->next = nullptr;
    }
    delete oldTail;
    --count;
    return true;
}

/**
 * Time Complexity: O(1).
 * Reads tail and tail->prev in constant time.
 * Drift Rate = Delta(Deviation) / Delta(Time)
 */
double DoublyLinkedList::computeLatestDriftRate() const {
    if (tail == nullptr || tail->prev == nullptr) {
        return 0.0; // Needs at least 2 history data points
    }

    double deltaDev = tail->data.measuredDeviation - tail->prev->data.measuredDeviation;
    long long deltaTime = tail->data.timestamp - tail->prev->data.timestamp;

    if (deltaTime == 0) {
        return 0.0; // Avoid division by zero
    }

    return deltaDev / static_cast<double>(deltaTime);
}

/**
 * Time Complexity: O(K) where K is windowSize.
 * Traverses backward from tail across up to K historical nodes.
 */
double DoublyLinkedList::computeWindowDriftRate(size_t windowSize) const {
    if (tail == nullptr || tail->prev == nullptr || windowSize < 2) {
        return 0.0;
    }

    DLLNode* curr = tail;
    size_t steps = 0;
    while (curr->prev != nullptr && steps < windowSize - 1) {
        curr = curr->prev;
        ++steps;
    }

    double deltaDev = tail->data.measuredDeviation - curr->data.measuredDeviation;
    long long deltaTime = tail->data.timestamp - curr->data.timestamp;

    if (deltaTime == 0) {
        return 0.0;
    }

    return deltaDev / static_cast<double>(deltaTime);
}

const HistoryEntry* DoublyLinkedList::getLatestEntry() const {
    if (tail == nullptr) {
        return nullptr;
    }
    return &(tail->data);
}

size_t DoublyLinkedList::size() const {
    return count;
}

bool DoublyLinkedList::empty() const {
    return count == 0;
}

void DoublyLinkedList::clear() {
    DLLNode* curr = head;
    while (curr != nullptr) {
        DLLNode* nextNode = curr->next;
        delete curr;
        curr = nextNode;
    }
    head = nullptr;
    tail = nullptr;
    count = 0;
}

void DoublyLinkedList::printForward() const {
    DLLNode* curr = head;
    std::cout << "History [Oldest -> Newest]:" << std::endl;
    while (curr != nullptr) {
        std::cout << "  - [" << (curr->data.type == ENTRY_FAULT ? "FAULT" : "CALIB") << "] "
                  << "TS=" << curr->data.timestamp << ", Dev=" << curr->data.measuredDeviation
                  << ", Drift=" << curr->data.computedDrift << ", Desc: " << curr->data.description
                  << std::endl;
        curr = curr->next;
    }
}

void DoublyLinkedList::printBackward() const {
    DLLNode* curr = tail;
    std::cout << "History [Newest -> Oldest]:" << std::endl;
    while (curr != nullptr) {
        std::cout << "  - [" << (curr->data.type == ENTRY_FAULT ? "FAULT" : "CALIB") << "] "
                  << "TS=" << curr->data.timestamp << ", Dev=" << curr->data.measuredDeviation
                  << ", Drift=" << curr->data.computedDrift << ", Desc: " << curr->data.description
                  << std::endl;
        curr = curr->prev;
    }
}
