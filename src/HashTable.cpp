#include "HashTable.h"

HashTable::HashTable(size_t initialBuckets)
    : bucketCount(initialBuckets < 11 ? 11 : initialBuckets), totalElements(0) {
    buckets = new HashNode*[bucketCount];
    for (size_t i = 0; i < bucketCount; ++i) {
        buckets[i] = nullptr;
    }
}

HashTable::~HashTable() {
    for (size_t i = 0; i < bucketCount; ++i) {
        HashNode* curr = buckets[i];
        while (curr != nullptr) {
            HashNode* temp = curr->next;
            delete curr;
            curr = temp;
        }
    }
    delete[] buckets;
    buckets = nullptr;
}

/**
 * Time Complexity: O(L) where L is string length.
 * Polynomial rolling hash (djb2 variant).
 */
size_t HashTable::hashFunction(const std::string& key) const {
    size_t hash = 5381;
    for (char c : key) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c); // hash * 33 + c
    }
    return hash % bucketCount;
}

/**
 * Time Complexity: O(N + M) where N is element count, M is new bucket count.
 * Allocates new bucket array and re-chains all existing nodes.
 */
void HashTable::rehash(size_t newCapacity) {
    HashNode** newBuckets = new HashNode*[newCapacity];
    for (size_t i = 0; i < newCapacity; ++i) {
        newBuckets[i] = nullptr;
    }

    // Re-insert all nodes into new buckets
    for (size_t i = 0; i < bucketCount; ++i) {
        HashNode* curr = buckets[i];
        while (curr != nullptr) {
            HashNode* nextNode = curr->next;
            // Compute new index with new bucket capacity
            size_t newHash = 5381;
            for (char c : curr->key) {
                newHash = ((newHash << 5) + newHash) + static_cast<unsigned char>(c);
            }
            size_t newIdx = newHash % newCapacity;

            curr->next = newBuckets[newIdx];
            newBuckets[newIdx] = curr;

            curr = nextNode;
        }
    }

    delete[] buckets;
    buckets = newBuckets;
    bucketCount = newCapacity;
}

/**
 * Time Complexity: Average O(1), Worst O(N) when all keys collide into one bucket.
 * Traverses bucket chain; if key exists, updates in place; otherwise prepends new HashNode.
 */
bool HashTable::insert(const Asset& asset) {
    if (loadFactor() >= maxLoadFactor) {
        rehash(bucketCount * 2 + 1);
    }

    size_t idx = hashFunction(asset.assetCode);
    HashNode* curr = buckets[idx];

    // Check if key already exists (update existing)
    while (curr != nullptr) {
        if (curr->key == asset.assetCode) {
            curr->value = asset;
            return false; // Updated existing asset
        }
        curr = curr->next;
    }

    // Insert new node at head of bucket chain
    HashNode* newNode = new HashNode(asset.assetCode, asset, buckets[idx]);
    buckets[idx] = newNode;
    ++totalElements;
    return true; // Newly inserted
}

/**
 * Time Complexity: Average O(1), Worst O(N).
 * Resolves bucket index in O(L) time and scans the small collision chain.
 */
Asset* HashTable::find(const std::string& assetCode) {
    size_t idx = hashFunction(assetCode);
    HashNode* curr = buckets[idx];
    while (curr != nullptr) {
        if (curr->key == assetCode) {
            return &(curr->value);
        }
        curr = curr->next;
    }
    return nullptr;
}

const Asset* HashTable::find(const std::string& assetCode) const {
    size_t idx = hashFunction(assetCode);
    HashNode* curr = buckets[idx];
    while (curr != nullptr) {
        if (curr->key == assetCode) {
            return &(curr->value);
        }
        curr = curr->next;
    }
    return nullptr;
}

/**
 * Time Complexity: Average O(1), Worst O(N).
 * Unlinks target node from the bucket list and frees memory.
 */
bool HashTable::remove(const std::string& assetCode) {
    size_t idx = hashFunction(assetCode);
    HashNode* curr = buckets[idx];
    HashNode* prev = nullptr;

    while (curr != nullptr) {
        if (curr->key == assetCode) {
            if (prev == nullptr) {
                buckets[idx] = curr->next;
            } else {
                prev->next = curr->next;
            }
            delete curr;
            --totalElements;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

bool HashTable::contains(const std::string& assetCode) const {
    return find(assetCode) != nullptr;
}

size_t HashTable::size() const {
    return totalElements;
}

double HashTable::loadFactor() const {
    return static_cast<double>(totalElements) / static_cast<double>(bucketCount);
}

void HashTable::getAllAssets(CustomArray<Asset*>& outAssets) {
    outAssets.clear();
    for (size_t i = 0; i < bucketCount; ++i) {
        HashNode* curr = buckets[i];
        while (curr != nullptr) {
            outAssets.push_back(&(curr->value));
            curr = curr->next;
        }
    }
}
