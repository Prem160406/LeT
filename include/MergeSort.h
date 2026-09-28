#ifndef MERGE_SORT_H
#define MERGE_SORT_H

#include "Asset.h"
#include "Common.h"
#include <cstddef>

/**
 * @brief Criteria for compliance and audit reporting.
 */
enum SortCriterion {
    SORT_BY_DOWNTIME_DESC,
    SORT_BY_FAULT_COUNT_DESC,
    SORT_BY_CALIBRATION_DUE_ASC
};

/**
 * @brief Standalone Merge Sort implementation running strictly in O(N log N) time.
 * Used for generating compliance reports by sorting assets by downtime, fault count, etc.
 * 
 * Implemented 100% from scratch with zero STL sort dependencies.
 */
class MergeSort {
private:
    static bool compare(const Asset* a, const Asset* b, SortCriterion criterion);
    static void merge(Asset** arr, size_t left, size_t mid, size_t right, 
                      Asset** temp, SortCriterion criterion);
    static void mergeSortRange(Asset** arr, size_t left, size_t right, 
                               Asset** temp, SortCriterion criterion);

public:
    /**
     * @brief Sorts an array of Asset pointers using recursive divide-and-conquer Merge Sort.
     * 
     * Time Complexity: O(N log N) in all cases (worst, average, best).
     * Space Complexity: O(N) auxiliary buffer for merging subarrays.
     * 
     * @param arr Array of pointers to Asset objects.
     * @param count Number of elements in the array.
     * @param criterion Sorting criterion (e.g. SORT_BY_DOWNTIME_DESC).
     */
    static void sort(Asset** arr, size_t count, SortCriterion criterion);

    /**
     * @brief Overload sorting a CustomArray of Asset pointers.
     * Time Complexity: O(N log N).
     */
    static void sort(CustomArray<Asset*>& assetList, SortCriterion criterion);
};

#endif // MERGE_SORT_H
