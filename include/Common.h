#ifndef COMMON_H
#define COMMON_H

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>

/**
 * @brief Custom dynamic array implemented entirely from scratch.
 * Replaces std::vector to adhere strictly to the zero-STL container rule.
 * Provides amortized O(1) appending, O(1) random access, and automatic capacity doubling.
 */
template <typename T>
class CustomArray {
private:
    T* data;
    size_t capacity_;
    size_t size_;

    /**
     * @brief Resizes the underlying buffer to a new capacity.
     * Time Complexity: O(N) where N is current size, copying elements to new buffer.
     */
    void reallocate(size_t newCapacity) {
        T* newData = new T[newCapacity];
        for (size_t i = 0; i < size_; ++i) {
            newData[i] = std::move(data[i]);
        }
        delete[] data;
        data = newData;
        capacity_ = newCapacity;
    }

public:
    /**
     * @brief Constructor initializing default capacity.
     * Time Complexity: O(1).
     */
    CustomArray(size_t initialCapacity = 8)
        : data(nullptr), capacity_(initialCapacity < 4 ? 4 : initialCapacity), size_(0) {
        data = new T[capacity_];
    }

    /**
     * @brief Copy Constructor.
     * Time Complexity: O(N).
     */
    CustomArray(const CustomArray& other)
        : data(nullptr), capacity_(other.capacity_), size_(other.size_) {
        data = new T[capacity_];
        for (size_t i = 0; i < size_; ++i) {
            data[i] = other.data[i];
        }
    }

    /**
     * @brief Move Constructor.
     * Time Complexity: O(1).
     */
    CustomArray(CustomArray&& other) noexcept
        : data(other.data), capacity_(other.capacity_), size_(other.size_) {
        other.data = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    /**
     * @brief Copy Assignment Operator.
     * Time Complexity: O(N).
     */
    CustomArray& operator=(const CustomArray& other) {
        if (this != &other) {
            delete[] data;
            capacity_ = other.capacity_;
            size_ = other.size_;
            data = new T[capacity_];
            for (size_t i = 0; i < size_; ++i) {
                data[i] = other.data[i];
            }
        }
        return *this;
    }

    /**
     * @brief Move Assignment Operator.
     * Time Complexity: O(1).
     */
    CustomArray& operator=(CustomArray&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = other.data;
            capacity_ = other.capacity_;
            size_ = other.size_;
            other.data = nullptr;
            other.capacity_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    /**
     * @brief Destructor.
     * Time Complexity: O(N) calling element destructors and deallocating buffer.
     */
    ~CustomArray() {
        delete[] data;
        data = nullptr;
        capacity_ = 0;
        size_ = 0;
    }

    /**
     * @brief Appends element to the end.
     * Time Complexity: Amortized O(1), worst-case O(N) when buffer doubles.
     */
    void push_back(const T& element) {
        if (size_ >= capacity_) {
            reallocate(capacity_ * 2);
        }
        data[size_++] = element;
    }

    /**
     * @brief Appends element by move.
     * Time Complexity: Amortized O(1), worst-case O(N) when buffer doubles.
     */
    void push_back(T&& element) {
        if (size_ >= capacity_) {
            reallocate(capacity_ * 2);
        }
        data[size_++] = std::move(element);
    }

    /**
     * @brief Removes the last element.
     * Time Complexity: O(1).
     */
    void pop_back() {
        if (size_ > 0) {
            --size_;
        }
    }

    /**
     * @brief Random access operator (mutable).
     * Time Complexity: O(1) direct pointer offset.
     */
    T& operator[](size_t index) {
        return data[index];
    }

    /**
     * @brief Random access operator (const).
     * Time Complexity: O(1) direct pointer offset.
     */
    const T& operator[](size_t index) const {
        return data[index];
    }

    /**
     * @brief Returns current element count.
     * Time Complexity: O(1).
     */
    size_t size() const {
        return size_;
    }

    /**
     * @brief Returns current internal capacity.
     * Time Complexity: O(1).
     */
    size_t capacity() const {
        return capacity_;
    }

    /**
     * @brief Checks if array is empty.
     * Time Complexity: O(1).
     */
    bool empty() const {
        return size_ == 0;
    }

    /**
     * @brief Clears all elements without deallocating buffer.
     * Time Complexity: O(1).
     */
    void clear() {
        size_ = 0;
    }

    /**
     * @brief Raw pointer access.
     */
    T* raw_data() {
        return data;
    }

    const T* raw_data() const {
        return data;
    }
};

#endif // COMMON_H
