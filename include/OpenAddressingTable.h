#ifndef OPEN_ADDRESSING_TABLE_H
#define OPEN_ADDRESSING_TABLE_H

#include <string>
#include <cstddef>

/**
 * @brief Slot state for Linear Probing collision resolution.
 */
enum OpenSlotState {
    SLOT_EMPTY,
    SLOT_OCCUPIED,
    SLOT_DELETED
};

/**
 * @brief Isolated educational experiment: Open Addressing with Linear Probing.
 * 
 * Used strictly for empirical comparison against Separate Chaining in main test suite.
 * Does NOT replace or modify the production HashTable used by Engine and AssetRegistry.
 * 
 * Collision Resolution: Linear Probing (hash + i) % capacity
 * Deletion: Tombstone marker (SLOT_DELETED) to preserve search probe sequences.
 */
template <typename ValueType = std::string>
class OpenAddressingTable {
public:
    struct Slot {
        std::string key;
        ValueType value;
        OpenSlotState state;

        Slot() : key(""), value(ValueType()), state(SLOT_EMPTY) {}
    };

    struct Stats {
        size_t capacity;
        size_t elementCount;
        size_t deletedCount;
        size_t totalInsertProbes;
        size_t maxProbeSequence;
        double averageInsertProbes;
        double loadFactor;

        Stats()
            : capacity(0), elementCount(0), deletedCount(0),
              totalInsertProbes(0), maxProbeSequence(0), averageInsertProbes(0.0), loadFactor(0.0) {}
    };

private:
    Slot* table;
    size_t capacity;
    size_t elementCount;
    size_t deletedCount;
    size_t totalInsertProbes;
    size_t maxProbeSequence;

    /**
     * @brief Identical polynomial rolling hash (djb2 variant) used by Separate Chaining.
     */
    size_t hashFunction(const std::string& key) const {
        size_t hash = 5381;
        for (char c : key) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
        }
        return hash % capacity;
    }

public:
    explicit OpenAddressingTable(size_t initialCapacity = 61)
        : table(nullptr), capacity(initialCapacity < 11 ? 11 : initialCapacity),
          elementCount(0), deletedCount(0), totalInsertProbes(0), maxProbeSequence(0) {
        table = new Slot[capacity];
    }

    ~OpenAddressingTable() {
        delete[] table;
        table = nullptr;
    }

    OpenAddressingTable(const OpenAddressingTable&) = delete;
    OpenAddressingTable& operator=(const OpenAddressingTable&) = delete;

    /**
     * @brief Inserts or updates an element using Linear Probing.
     * @param outProbes Optional output receiving number of probe steps taken.
     * @return true if newly inserted, false if existing key updated.
     */
    bool insert(const std::string& key, const ValueType& value, size_t* outProbes = nullptr) {
        if (elementCount + deletedCount >= capacity) {
            if (outProbes) *outProbes = capacity;
            return false; // Bounded table full
        }

        size_t baseIdx = hashFunction(key);
        size_t firstDeleted = capacity;
        size_t probes = 0;

        for (size_t i = 0; i < capacity; ++i) {
            ++probes;
            size_t idx = (baseIdx + i) % capacity;

            if (table[idx].state == SLOT_OCCUPIED) {
                if (table[idx].key == key) {
                    table[idx].value = value;
                    if (outProbes) *outProbes = probes;
                    return false; // Updated existing key
                }
            } else if (table[idx].state == SLOT_DELETED) {
                if (firstDeleted == capacity) {
                    firstDeleted = idx;
                }
            } else { // SLOT_EMPTY
                size_t targetIdx = (firstDeleted != capacity) ? firstDeleted : idx;
                if (firstDeleted != capacity) {
                    --deletedCount;
                }
                table[targetIdx].key = key;
                table[targetIdx].value = value;
                table[targetIdx].state = SLOT_OCCUPIED;
                ++elementCount;
                totalInsertProbes += probes;
                if (probes > maxProbeSequence) {
                    maxProbeSequence = probes;
                }
                if (outProbes) *outProbes = probes;
                return true;
            }
        }

        if (firstDeleted != capacity) {
            --deletedCount;
            table[firstDeleted].key = key;
            table[firstDeleted].value = value;
            table[firstDeleted].state = SLOT_OCCUPIED;
            ++elementCount;
            totalInsertProbes += probes;
            if (probes > maxProbeSequence) {
                maxProbeSequence = probes;
            }
            if (outProbes) *outProbes = probes;
            return true;
        }

        if (outProbes) *outProbes = probes;
        return false;
    }

    /**
     * @brief Searches for key using Linear Probing.
     * Skips SLOT_DELETED tombstones; terminates on SLOT_EMPTY or match.
     */
    bool find(const std::string& key, ValueType& outValue, size_t* outProbes = nullptr) const {
        size_t baseIdx = hashFunction(key);
        size_t probes = 0;

        for (size_t i = 0; i < capacity; ++i) {
            ++probes;
            size_t idx = (baseIdx + i) % capacity;

            if (table[idx].state == SLOT_EMPTY) {
                if (outProbes) *outProbes = probes;
                return false;
            }
            if (table[idx].state == SLOT_OCCUPIED && table[idx].key == key) {
                outValue = table[idx].value;
                if (outProbes) *outProbes = probes;
                return true;
            }
        }
        if (outProbes) *outProbes = probes;
        return false;
    }

    /**
     * @brief Removes key by marking slot as SLOT_DELETED (tombstone).
     */
    bool remove(const std::string& key) {
        size_t baseIdx = hashFunction(key);
        for (size_t i = 0; i < capacity; ++i) {
            size_t idx = (baseIdx + i) % capacity;
            if (table[idx].state == SLOT_EMPTY) {
                return false;
            }
            if (table[idx].state == SLOT_OCCUPIED && table[idx].key == key) {
                table[idx].state = SLOT_DELETED;
                table[idx].key = "";
                table[idx].value = ValueType();
                --elementCount;
                ++deletedCount;
                return true;
            }
        }
        return false;
    }

    size_t size() const { return elementCount; }
    size_t getCapacity() const { return capacity; }
    double loadFactor() const { return static_cast<double>(elementCount) / static_cast<double>(capacity); }

    Stats getStats() const {
        Stats s;
        s.capacity = capacity;
        s.elementCount = elementCount;
        s.deletedCount = deletedCount;
        s.totalInsertProbes = totalInsertProbes;
        s.maxProbeSequence = maxProbeSequence;
        s.averageInsertProbes = elementCount > 0 ? (static_cast<double>(totalInsertProbes) / elementCount) : 0.0;
        s.loadFactor = loadFactor();
        return s;
    }
};

#endif // OPEN_ADDRESSING_TABLE_H
