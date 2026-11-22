// DoublyLinkedList: nodes linked with prev and next pointers.
//
// RAHAT stores an ambulance's route as a list of checkpoints. Walking
// forward (next) moves the ambulance along its route; stepping back (prev)
// shows the checkpoint it just passed. Inserting or erasing a node in the
// middle only relinks two neighbours, which is how a route gets updated.
#pragma once

#include <stdexcept>

namespace rahat {

template <typename T>
class DoublyLinkedList {
public:
    struct Node {
        T value;
        Node* prev;
        Node* next;
    };

    DoublyLinkedList() : first(nullptr), last(nullptr), count(0) {}

    DoublyLinkedList(const DoublyLinkedList& other) : first(nullptr), last(nullptr), count(0) {
        for (Node* node = other.first; node != nullptr; node = node->next) pushBack(node->value);
    }

    DoublyLinkedList& operator=(const DoublyLinkedList& other) {
        if (this == &other) return *this;
        clear();
        for (Node* node = other.first; node != nullptr; node = node->next) pushBack(node->value);
        return *this;
    }

    ~DoublyLinkedList() { clear(); }

    void pushBack(const T& value) {
        Node* node = new Node{value, last, nullptr};
        if (last == nullptr) first = node;
        else last->next = node;
        last = node;
        ++count;
    }

    void pushFront(const T& value) {
        Node* node = new Node{value, nullptr, first};
        if (first == nullptr) last = node;
        else first->prev = node;
        first = node;
        ++count;
    }

    // Inserts value right after `position` and returns the new node.
    Node* insertAfter(Node* position, const T& value) {
        if (position == nullptr) throw std::invalid_argument("insertAfter: null position");
        Node* node = new Node{value, position, position->next};
        if (position->next == nullptr) last = node;
        else position->next->prev = node;
        position->next = node;
        ++count;
        return node;
    }

    // Unlinks and deletes `node`. Returns the node that followed it.
    Node* erase(Node* node) {
        if (node == nullptr) throw std::invalid_argument("erase: null node");
        Node* following = node->next;
        if (node->prev == nullptr) first = node->next;
        else node->prev->next = node->next;
        if (node->next == nullptr) last = node->prev;
        else node->next->prev = node->prev;
        delete node;
        --count;
        return following;
    }

    void popFront() {
        if (first == nullptr) throw std::out_of_range("popFront on empty list");
        erase(first);
    }

    void popBack() {
        if (last == nullptr) throw std::out_of_range("popBack on empty list");
        erase(last);
    }

    T& front() {
        if (first == nullptr) throw std::out_of_range("front on empty list");
        return first->value;
    }

    T& back() {
        if (last == nullptr) throw std::out_of_range("back on empty list");
        return last->value;
    }

    Node* head() const { return first; }
    Node* tail() const { return last; }
    int size() const { return count; }
    bool empty() const { return count == 0; }

    void clear() {
        Node* current = first;
        while (current != nullptr) {
            Node* next = current->next;
            delete current;
            current = next;
        }
        first = last = nullptr;
        count = 0;
    }

    // Calls fn(value) from head to tail.
    template <typename Fn>
    void forEach(Fn fn) const {
        for (Node* node = first; node != nullptr; node = node->next) fn(node->value);
    }

private:
    Node* first;
    Node* last;
    int count;
};

}  // namespace rahat
