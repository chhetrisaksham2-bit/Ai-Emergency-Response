// MinHeap - a binary heap stored in an array. The SMALLEST item comes out first.
// Owner: Utkarsh Tyagi (Data Structures)
//
// Used by Dijkstra to always pick the place with the smallest distance so far.
//
// Array layout (0-based):  parent(i) = (i - 1) / 2,  left(i) = 2i + 1,  right(i) = 2i + 2
//   push: put at the end, move UP while smaller than the parent      O(log n)
//   pop:  take the root, move the last item to the root, move DOWN   O(log n)
#pragma once

#include <stdexcept>
#include <utility>

namespace phase2 {

// Less(a, b) must return true when `a` should come out before `b`.
template <typename T, typename Less>
class MinHeap {
public:
    explicit MinHeap(int initialCapacity = 16)
        : data(new T[initialCapacity]), count(0), capacity(initialCapacity) {}
    ~MinHeap() { delete[] data; }

    // The heap owns its array, so copying is not allowed.
    MinHeap(const MinHeap&) = delete;
    MinHeap& operator=(const MinHeap&) = delete;

    void push(const T& item) {
        if (count == capacity) grow();
        data[count] = item;
        siftUp(count);
        ++count;
    }

    T pop() {
        if (count == 0) throw std::out_of_range("pop() on an empty heap");
        T smallest = data[0];
        --count;
        if (count > 0) {
            data[0] = data[count];
            siftDown(0);
        }
        return smallest;
    }

    const T& top() const {
        if (count == 0) throw std::out_of_range("top() on an empty heap");
        return data[0];
    }

    bool empty() const { return count == 0; }
    int size() const { return count; }

private:
    T* data;
    int count;
    int capacity;
    Less less;

    // Double the array when it is full.
    void grow() {
        int bigger = capacity * 2;
        T* fresh = new T[bigger];
        for (int i = 0; i < count; ++i) fresh[i] = data[i];
        delete[] data;
        data = fresh;
        capacity = bigger;
    }

    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (!less(data[i], data[parent])) break;
            std::swap(data[i], data[parent]);
            i = parent;
        }
    }

    void siftDown(int i) {
        while (true) {
            int left = 2 * i + 1, right = 2 * i + 2, smallest = i;
            if (left < count && less(data[left], data[smallest])) smallest = left;
            if (right < count && less(data[right], data[smallest])) smallest = right;
            if (smallest == i) break;
            std::swap(data[i], data[smallest]);
            i = smallest;
        }
    }
};

}  // namespace phase2
