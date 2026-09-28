#include "AVLTree.h"
#include <cmath>
#include <algorithm>

AVLTree::AVLTree() : root(nullptr), count(0) {}

AVLTree::~AVLTree() {
    clear();
}

int AVLTree::getNodeHeight(AVLNode* node) const {
    return node == nullptr ? 0 : node->height;
}

int AVLTree::getBalanceFactor(AVLNode* node) const {
    if (node == nullptr) return 0;
    return getNodeHeight(node->left) - getNodeHeight(node->right);
}

void AVLTree::updateHeight(AVLNode* node) {
    if (node != nullptr) {
        int hl = getNodeHeight(node->left);
        int hr = getNodeHeight(node->right);
        node->height = 1 + (hl > hr ? hl : hr);
    }
}

/**
 * Right Rotation (LL Case).
 * Time Complexity: O(1) constant pointer manipulation.
 */
AVLNode* AVLTree::rotateRight(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;

    x->right = y;
    y->left = T2;

    updateHeight(y);
    updateHeight(x);

    return x; // New subtree root
}

/**
 * Left Rotation (RR Case).
 * Time Complexity: O(1) constant pointer manipulation.
 */
AVLNode* AVLTree::rotateLeft(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;

    y->left = x;
    x->right = T2;

    updateHeight(x);
    updateHeight(y);

    return y; // New subtree root
}

/**
 * Time Complexity: O(log N).
 * Traverses down BST path, inserts leaf, then unwinds stack applying rotations
 * if balance factor deviates beyond [-1, 1].
 */
AVLNode* AVLTree::insertNode(AVLNode* node, const CalibrationItem& item, bool& inserted) {
    if (node == nullptr) {
        inserted = true;
        return new AVLNode(item);
    }

    if (item < node->item) {
        node->left = insertNode(node->left, item, inserted);
    } else if (node->item < item) {
        node->right = insertNode(node->right, item, inserted);
    } else {
        inserted = false; // Duplicate (dueDate, assetCode) rejected
        return node;
    }

    updateHeight(node);
    int balance = getBalanceFactor(node);

    // Left Left (LL)
    if (balance > 1 && item < node->left->item) {
        return rotateRight(node);
    }

    // Right Right (RR)
    if (balance < -1 && node->right->item < item) {
        return rotateLeft(node);
    }

    // Left Right (LR)
    if (balance > 1 && node->left->item < item) {
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }

    // Right Left (RL)
    if (balance < -1 && item < node->right->item) {
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }

    return node;
}

AVLNode* AVLTree::findMinNode(AVLNode* node) const {
    AVLNode* curr = node;
    while (curr != nullptr && curr->left != nullptr) {
        curr = curr->left;
    }
    return curr;
}

/**
 * Time Complexity: O(log N).
 * Locates node, removes it, and rebalances ancestral nodes up to root.
 */
AVLNode* AVLTree::removeNode(AVLNode* node, const CalibrationItem& item, bool& removed) {
    if (node == nullptr) {
        removed = false;
        return nullptr;
    }

    if (item < node->item) {
        node->left = removeNode(node->left, item, removed);
    } else if (node->item < item) {
        node->right = removeNode(node->right, item, removed);
    } else {
        removed = true;
        // Node found
        if (node->left == nullptr || node->right == nullptr) {
            AVLNode* temp = node->left ? node->left : node->right;
            if (temp == nullptr) {
                // No child
                temp = node;
                node = nullptr;
            } else {
                // One child
                *node = *temp;
            }
            delete temp;
        } else {
            // Two children: get in-order successor (min in right subtree)
            AVLNode* temp = findMinNode(node->right);
            node->item = temp->item;
            node->right = removeNode(node->right, temp->item, removed);
        }
    }

    if (node == nullptr) {
        return nullptr;
    }

    updateHeight(node);
    int balance = getBalanceFactor(node);

    // LL
    if (balance > 1 && getBalanceFactor(node->left) >= 0) {
        return rotateRight(node);
    }
    // LR
    if (balance > 1 && getBalanceFactor(node->left) < 0) {
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }
    // RR
    if (balance < -1 && getBalanceFactor(node->right) <= 0) {
        return rotateLeft(node);
    }
    // RL
    if (balance < -1 && getBalanceFactor(node->right) > 0) {
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }

    return node;
}

bool AVLTree::insert(long long dueDate, const std::string& assetCode, const std::string& assetName) {
    bool inserted = false;
    CalibrationItem item(dueDate, assetCode, assetName);
    root = insertNode(root, item, inserted);
    if (inserted) {
        ++count;
    }
    return inserted;
}

bool AVLTree::remove(long long dueDate, const std::string& assetCode) {
    bool removed = false;
    CalibrationItem item(dueDate, assetCode);
    root = removeNode(root, item, removed);
    if (removed) {
        --count;
    }
    return removed;
}

/**
 * Time Complexity: O(log N + K) where K is number of items in the range.
 * Prunes subtrees that fall completely outside [startDue, endDue].
 */
void AVLTree::rangeQueryHelper(AVLNode* node, long long startDue, long long endDue,
                               CustomArray<CalibrationItem>& outItems) const {
    if (node == nullptr) return;

    if (startDue < node->item.dueDate) {
        rangeQueryHelper(node->left, startDue, endDue, outItems);
    }

    if (node->item.dueDate >= startDue && node->item.dueDate <= endDue) {
        outItems.push_back(node->item);
    }

    if (endDue > node->item.dueDate) {
        rangeQueryHelper(node->right, startDue, endDue, outItems);
    }
}

void AVLTree::getRange(long long startDue, long long endDue, 
                       CustomArray<CalibrationItem>& outItems) const {
    outItems.clear();
    rangeQueryHelper(root, startDue, endDue, outItems);
}

void AVLTree::inOrderHelper(AVLNode* node, CustomArray<CalibrationItem>& outItems) const {
    if (node == nullptr) return;
    inOrderHelper(node->left, outItems);
    outItems.push_back(node->item);
    inOrderHelper(node->right, outItems);
}

void AVLTree::getAllSorted(CustomArray<CalibrationItem>& outItems) const {
    outItems.clear();
    inOrderHelper(root, outItems);
}

bool AVLTree::checkBalanceHelper(AVLNode* node) const {
    if (node == nullptr) return true;
    int bf = getBalanceFactor(node);
    if (bf < -1 || bf > 1) return false;
    return checkBalanceHelper(node->left) && checkBalanceHelper(node->right);
}

bool AVLTree::isBalanced() const {
    return checkBalanceHelper(root);
}

int AVLTree::height() const {
    return getNodeHeight(root);
}

size_t AVLTree::size() const {
    return count;
}

bool AVLTree::empty() const {
    return count == 0;
}

void AVLTree::destroyTree(AVLNode* node) {
    if (node != nullptr) {
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }
}

void AVLTree::clear() {
    destroyTree(root);
    root = nullptr;
    count = 0;
}
