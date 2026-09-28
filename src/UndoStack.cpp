#include "UndoStack.h"

UndoRecord::UndoRecord()
    : type(UNDO_ASSET_REGISTRATION), assetCode(""), taskId(""),
      timestamp(0), deviation(0.0), severity(1), description("") {}

UndoRecord::UndoRecord(UndoActionType t, const std::string& aCode, const std::string& tId,
                       long long ts, double dev, int sev, const std::string& desc)
    : type(t), assetCode(aCode), taskId(tId), timestamp(ts),
      deviation(dev), severity(sev), description(desc) {}

UndoStack::UndoStack() : topNode(nullptr), count(0) {}

UndoStack::~UndoStack() {
    clear();
}

/**
 * Time Complexity: O(1).
 * Allocates new StackNode and points it to old top.
 */
void UndoStack::push(const UndoRecord& record) {
    StackNode* newNode = new StackNode(record, topNode);
    topNode = newNode;
    ++count;
}

/**
 * Time Complexity: O(1).
 * Unlinks current topNode and deallocates it.
 */
bool UndoStack::pop(UndoRecord& outRecord) {
    if (empty()) {
        return false;
    }
    StackNode* oldTop = topNode;
    outRecord = oldTop->record;
    topNode = topNode->next;
    delete oldTop;
    --count;
    return true;
}

/**
 * Time Complexity: O(1).
 * Inspects topNode payload directly.
 */
bool UndoStack::peek(UndoRecord& outRecord) const {
    if (empty()) {
        return false;
    }
    outRecord = topNode->record;
    return true;
}

bool UndoStack::empty() const {
    return topNode == nullptr;
}

size_t UndoStack::size() const {
    return count;
}

void UndoStack::clear() {
    while (topNode != nullptr) {
        StackNode* temp = topNode;
        topNode = topNode->next;
        delete temp;
    }
    count = 0;
}
