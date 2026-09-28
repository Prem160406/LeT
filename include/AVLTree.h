#ifndef AVL_TREE_H
#define AVL_TREE_H

#include "Common.h"
#include <string>
#include <algorithm>

/**
 * @brief Calibration record payload in AVL Tree.
 */
struct CalibrationItem {
    long long dueDate;          // Primary key (timestamp / epoch)
    std::string assetCode;      // Secondary discriminator to guarantee uniqueness
    std::string assetName;

    CalibrationItem() : dueDate(0), assetCode(""), assetName("") {}
    CalibrationItem(long long due, const std::string& code, const std::string& name = "")
        : dueDate(due), assetCode(code), assetName(name) {}

    bool operator<(const CalibrationItem& other) const {
        if (dueDate != other.dueDate) {
            return dueDate < other.dueDate;
        }
        return assetCode < other.assetCode;
    }

    bool operator==(const CalibrationItem& other) const {
        return dueDate == other.dueDate && assetCode == other.assetCode;
    }
};

/**
 * @brief AVL Tree Node storing calibration schedule items.
 */
struct AVLNode {
    CalibrationItem item;
    int height;
    AVLNode* left;
    AVLNode* right;

    AVLNode(const CalibrationItem& it, AVLNode* l = nullptr, AVLNode* r = nullptr)
        : item(it), height(1), left(l), right(r) {}
};

/**
 * @brief Self-balancing AVL Binary Search Tree.
 * 
 * Indexes equipment calibration schedules by next_calibration_due date.
 * Maintains balance factor <= 1 via 4 rotation types:
 * - Left-Left (Single Right Rotation)
 * - Right-Right (Single Left Rotation)
 * - Left-Right (Double Rotation: Left then Right)
 * - Right-Left (Double Rotation: Right then Left)
 * 
 * Guarantees O(log N) search, insertion, deletion, and O(log N + K) range queries.
 */
class AVLTree {
private:
    AVLNode* root;
    size_t count;

    int getNodeHeight(AVLNode* node) const;
    int getBalanceFactor(AVLNode* node) const;
    void updateHeight(AVLNode* node);

    AVLNode* rotateRight(AVLNode* y);
    AVLNode* rotateLeft(AVLNode* x);

    AVLNode* insertNode(AVLNode* node, const CalibrationItem& item, bool& inserted);
    AVLNode* findMinNode(AVLNode* node) const;
    AVLNode* removeNode(AVLNode* node, const CalibrationItem& item, bool& removed);
    void destroyTree(AVLNode* node);

    void inOrderHelper(AVLNode* node, CustomArray<CalibrationItem>& outItems) const;
    void rangeQueryHelper(AVLNode* node, long long startDue, long long endDue, 
                          CustomArray<CalibrationItem>& outItems) const;

public:
    AVLTree();
    ~AVLTree();

    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    /**
     * @brief Inserts calibration schedule item into AVL Tree.
     * Time Complexity: O(log N) guaranteed via tree balancing.
     * @return true if inserted, false if exact (dueDate, assetCode) already exists.
     */
    bool insert(long long dueDate, const std::string& assetCode, const std::string& assetName = "");

    /**
     * @brief Removes calibration schedule item.
     * Time Complexity: O(log N) guaranteed.
     */
    bool remove(long long dueDate, const std::string& assetCode);

    /**
     * @brief Range query: retrieves all calibrations due between startDue and endDue (inclusive).
     * Time Complexity: O(log N + K) where K is number of items in range.
     */
    void getRange(long long startDue, long long endDue, CustomArray<CalibrationItem>& outItems) const;

    /**
     * @brief Retrieves all items sorted by due date.
     * Time Complexity: O(N) in-order traversal.
     */
    void getAllSorted(CustomArray<CalibrationItem>& outItems) const;

    /**
     * @brief Checks if tree is balanced (for validation & grading proofs).
     * Time Complexity: O(N).
     */
    bool isBalanced() const;

    /**
     * @brief Returns tree height.
     * Time Complexity: O(1) from root node.
     */
    int height() const;

    /**
     * @brief Returns total items in tree.
     * Time Complexity: O(1).
     */
    size_t size() const;

    /**
     * @brief Checks if tree is empty.
     * Time Complexity: O(1).
     */
    bool empty() const;

    void clear();

private:
    bool checkBalanceHelper(AVLNode* node) const;
};

#endif // AVL_TREE_H
