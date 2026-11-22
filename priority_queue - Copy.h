// PriorityQueue: a binary heap stored in a dynamic array (new[] / delete[]).
//
// Compare(a, b) must return true when `a` should come out BEFORE `b`.
// That makes the same class work as a "most urgent first" queue for
// emergencies and a "smallest distance first" queue for Dijkstra.
//
// Layout of the array (0-based):
//   parent(i) = (i - 1) / 2,  left(i) = 2i + 1,  right(i) = 2i + 2
#pragma once

#include <stdexcept>
#include <utility>

namespace rahat {

template <typename T, typename Compare>
class PriorityQueue {
public:
    explicit PriorityQueue(Compare compare = Compare(), int initialCapacity = 16)
        : data(nullptr), count(0), capacity(initialCapacity < 1 ? 1 : initialCapacity),
          before(compare) {
        data = new T[capacity];
    }

    // Rule of Three: the heap owns its array, so copies must be deep.
    PriorityQueue(const PriorityQueue& other)
        : data(new T[other.capacity]), count(other.count), capacity(other.capacity),
          before(other.before) {
        for (int i = 0; i < count; ++i) data[i] = other.data[i];
    }

    PriorityQueue& operator=(const PriorityQueue& other) {
        if (this == &other) return *this;
        T* fresh = new T[other.capacity];
        for (int i = 0; i < other.count; ++i) fresh[i] = other.data[i];
        delete[] data;
        data = fresh;
        count = other.count;
        capacity = other.capacity;
        before = other.before;
        return *this;
    }

    ~PriorityQueue() { delete[] data; }

    void push(const T& value) {
        if (count == capacity) grow();
        data[count] = value;
        siftUp(count);
        ++count;
    }

    // Removes and returns the element that should come out first.
    T pop() {
        if (count == 0) throw std::out_of_range("PriorityQueue::pop on empty queue");
        T result = data[0];
        --count;
        if (count > 0) {
            data[0] = data[count];
            siftDown(0);
        }
        return result;
    }

    const T& top() const {
        if (count == 0) throw std::out_of_range("PriorityQueue::top on empty queue");
        return data[0];
    }

    bool empty() const { return count == 0; }
    int size() const { return count; }
    void clear() { count = 0; }

private:
    T* data;
    int count;
    int capacity;
    Compare before;

    void grow() {
        int newCapacity = capacity * 2;
        T* bigger = new T[newCapacity];
        for (int i = 0; i < count; ++i) bigger[i] = data[i];
        delete[] data;
        data = bigger;
        capacity = newCapacity;
    }

    // Move the element at i up while it should come out before its parent.
    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (!before(data[i], data[parent])) break;
            std::swap(data[i], data[parent]);
            i = parent;
        }
    }

    // Move the element at i down while one of its children should come out first.
    void siftDown(int i) {
        while (true) {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int first = i;
            if (left < count && before(data[left], data[first])) first = left;
            if (right < count && before(data[right], data[first])) first = right;
            if (first == i) break;
            std::swap(data[i], data[first]);
            i = first;
        }
    }
};

}  // namespace rahat
