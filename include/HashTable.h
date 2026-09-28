#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "Asset.h"
#include "Common.h"
#include <string>

/**
 * @brief Singly-linked bucket node for Separate Chaining collision handling.
 */
struct HashNode {
    std::string key;        // Unique assetCode (e.g. "LAB-PC-017")
    Asset value;            // Full Asset entity
    HashNode* next;

    HashNode(const std::string& k, const Asset& v, HashNode* n = nullptr)
        : key(k), value(v), next(n) {}
};

/**
 * @brief Hash Table built 100% from scratch with Separate Chaining collision handling.
 * 
 * Collision Handling Strategy: Separate Chaining via custom Singly Linked List.
 * Rationale:
 * 1. Resilient to high load factors; performance degrades gracefully to O(K) where K is
 *    bucket depth rather than suffering severe clustering cascades seen in Linear Probing.
 * 2. Deletions and updates do not require 'tombstones' (DELETED markers), preserving clean O(1) searches.
 * 3. Memory overhead is strictly proportional to inserted records + bucket pointer array.
 * 
 * Time Complexities:
 * - Insert: Average O(1), Worst O(N) if all keys hash to the same bucket.
 * - Search: Average O(1), Worst O(N).
 * - Remove: Average O(1), Worst O(N).
 */
class HashTable {
private:
    HashNode** buckets;
    size_t bucketCount;
    size_t totalElements;
    const double maxLoadFactor = 0.75;

    /**
     * @brief Computes polynomial rolling hash (djb2 variant).
     * Formula: hash = (hash * 31 + char) % bucketCount
     * Time Complexity: O(L) where L is string length of the key.
     */
    size_t hashFunction(const std::string& key) const;

    /**
     * @brief Resizes bucket array and rehashes all elements when load factor > 0.75.
     * Time Complexity: O(N + M) where N is element count and M is new bucket count.
     */
    void rehash(size_t newCapacity);

public:
    explicit HashTable(size_t initialBuckets = 61);
    ~HashTable();

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    /**
     * @brief Inserts or updates an asset keyed by assetCode.
     * Time Complexity: Average O(1), Worst O(N).
     * @return true if new asset was inserted, false if existing asset was updated.
     */
    bool insert(const Asset& asset);

    /**
     * @brief Finds pointer to asset by assetCode.
     * Time Complexity: Average O(1), Worst O(N).
     * @return Pointer to Asset if found, nullptr otherwise.
     */
    Asset* find(const std::string& assetCode);

    /**
     * @brief Const version of find.
     * Time Complexity: Average O(1), Worst O(N).
     */
    const Asset* find(const std::string& assetCode) const;

    /**
     * @brief Removes asset by assetCode.
     * Time Complexity: Average O(1), Worst O(N).
     * @return true if removed, false if not found.
     */
    bool remove(const std::string& assetCode);

    /**
     * @brief Checks if asset exists.
     * Time Complexity: Average O(1).
     */
    bool contains(const std::string& assetCode) const;

    /**
     * @brief Returns total number of assets stored.
     * Time Complexity: O(1).
     */
    size_t size() const;

    /**
     * @brief Returns current load factor.
     * Time Complexity: O(1).
     */
    double loadFactor() const;

    /**
     * @brief Collects all assets currently stored into a CustomArray.
     * Time Complexity: O(N + B) where B is bucket count.
     */
    void getAllAssets(CustomArray<Asset*>& outAssets);
};

#endif // HASH_TABLE_H
