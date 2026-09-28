#include "MergeSort.h"

bool MergeSort::compare(const Asset* a, const Asset* b, SortCriterion criterion) {
    if (a == nullptr || b == nullptr) return false;

    switch (criterion) {
        case SORT_BY_DOWNTIME_DESC:
            return a->totalDowntimeHours > b->totalDowntimeHours;
        case SORT_BY_FAULT_COUNT_DESC:
            return a->totalFaultCount > b->totalFaultCount;
        case SORT_BY_CALIBRATION_DUE_ASC:
            return a->nextCalibrationDue < b->nextCalibrationDue;
        default:
            return a->assetCode < b->assetCode;
    }
}

/**
 * Time Complexity: O(M) where M = right - left + 1.
 * Merges two sorted subarrays into temp and copies back into arr.
 */
void MergeSort::merge(Asset** arr, size_t left, size_t mid, size_t right,
                      Asset** temp, SortCriterion criterion) {
    size_t i = left;
    size_t j = mid + 1;
    size_t k = left;

    while (i <= mid && j <= right) {
        if (compare(arr[i], arr[j], criterion)) {
            temp[k++] = arr[i++];
        } else {
            temp[k++] = arr[j++];
        }
    }

    while (i <= mid) {
        temp[k++] = arr[i++];
    }

    while (j <= right) {
        temp[k++] = arr[j++];
    }

    for (size_t idx = left; idx <= right; ++idx) {
        arr[idx] = temp[idx];
    }
}

/**
 * Time Complexity: O(N log N) recursive tree with log N levels, each level taking O(N) merge work.
 */
void MergeSort::mergeSortRange(Asset** arr, size_t left, size_t right,
                               Asset** temp, SortCriterion criterion) {
    if (left >= right) {
        return;
    }

    size_t mid = left + (right - left) / 2;
    mergeSortRange(arr, left, mid, temp, criterion);
    mergeSortRange(arr, mid + 1, right, temp, criterion);
    merge(arr, left, mid, right, temp, criterion);
}

/**
 * Time Complexity: O(N log N).
 * Allocates single O(N) temporary buffer to avoid repeated heap allocation overhead.
 */
void MergeSort::sort(Asset** arr, size_t count, SortCriterion criterion) {
    if (arr == nullptr || count <= 1) {
        return;
    }

    Asset** temp = new Asset*[count];
    mergeSortRange(arr, 0, count - 1, temp, criterion);
    delete[] temp;
}

void MergeSort::sort(CustomArray<Asset*>& assetList, SortCriterion criterion) {
    if (assetList.size() <= 1) {
        return;
    }
    sort(assetList.raw_data(), assetList.size(), criterion);
}
