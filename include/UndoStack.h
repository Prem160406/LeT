#ifndef UNDO_STACK_H
#define UNDO_STACK_H

#include "Asset.h"
#include <string>

/**
 * @brief Identifies the type of operation to reverse during undo.
 */
enum UndoActionType {
    UNDO_ASSET_REGISTRATION,
    UNDO_FAULT_REPORT,
    UNDO_TRIAGE_DISPATCH
};

/**
 * @brief Snapshot of a state-changing transaction stored on the Undo Stack.
 */
struct UndoRecord {
    UndoActionType type;
    std::string assetCode;
    std::string taskId;
    long long timestamp;
    double deviation;
    int severity;
    std::string description;

    UndoRecord();
    UndoRecord(UndoActionType t, const std::string& aCode, const std::string& tId = "",
               long long ts = 0, double dev = 0.0, int sev = 1, const std::string& desc = "");
};

/**
 * @brief Node for custom LIFO Stack.
 */
struct StackNode {
    UndoRecord record;
    StackNode* next;

    StackNode(const UndoRecord& rec, StackNode* n = nullptr)
        : record(rec), next(n) {}
};

/**
 * @brief Custom LIFO Stack for O(1) transaction rollback/undo.
 * Implemented with linked nodes, guaranteeing true O(1) push and O(1) pop
 * without memory relocation pauses.
 */
class UndoStack {
private:
    StackNode* topNode;
    size_t count;

public:
    UndoStack();
    ~UndoStack();

    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;

    /**
     * @brief Pushes a rollback action record onto top of stack.
     * Time Complexity: O(1) node allocation and pointer assignment.
     */
    void push(const UndoRecord& record);

    /**
     * @brief Pops the most recent rollback record from top of stack.
     * Time Complexity: O(1) pointer adjustment and node deallocation.
     * @return true if popped, false if stack was empty.
     */
    bool pop(UndoRecord& outRecord);

    /**
     * @brief Peeks at the top record without popping.
     * Time Complexity: O(1).
     */
    bool peek(UndoRecord& outRecord) const;

    /**
     * @brief Checks if stack is empty.
     * Time Complexity: O(1).
     */
    bool empty() const;

    /**
     * @brief Returns number of recorded rollback operations.
     * Time Complexity: O(1).
     */
    size_t size() const;

    /**
     * @brief Clears the stack.
     * Time Complexity: O(N).
     */
    void clear();
};

#endif // UNDO_STACK_H
