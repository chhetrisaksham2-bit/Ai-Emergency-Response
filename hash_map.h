// HashMap: key -> value lookup using separate chaining.
//
// buckets[i] points to a singly linked chain of nodes whose key hashes to i.
// When the map gets more than 75% full, the bucket array doubles and every
// node is re-linked into its new bucket (rehash).
//
// Supported key types: int and std::string (see hashKey overloads below).
// Iteration order is NOT the insertion order.
#pragma once

#include <string>

namespace rahat {

inline unsigned long hashKey(int key) {
    // Mix the bits so nearby integers land in different buckets.
    unsigned long x = static_cast<unsigned long>(static_cast<unsigned int>(key));
    x = ((x >> 16) ^ x) * 0x45d9f3bUL;
    x = ((x >> 16) ^ x) * 0x45d9f3bUL;
    x = (x >> 16) ^ x;
    return x;
}

inline unsigned long hashKey(const std::string& key) {
    // djb2 string hash: hash * 33 + character
    unsigned long hash = 5381;
    for (unsigned char c : key) hash = hash * 33 + c;
    return hash;
}

template <typename K, typename V>
class HashMap {
public:
    explicit HashMap(int initialBuckets = 16)
        : buckets(nullptr), bucketCount(initialBuckets < 1 ? 1 : initialBuckets), count(0) {
        buckets = new Node*[bucketCount];
        for (int i = 0; i < bucketCount; ++i) buckets[i] = nullptr;
    }

    HashMap(const HashMap& other) : buckets(nullptr), bucketCount(other.bucketCount), count(0) {
        buckets = new Node*[bucketCount];
        for (int i = 0; i < bucketCount; ++i) buckets[i] = nullptr;
        copyNodesFrom(other);
    }

    HashMap& operator=(const HashMap& other) {
        if (this == &other) return *this;
        clear();
        copyNodesFrom(other);
        return *this;
    }

    ~HashMap() {
        clear();
        delete[] buckets;
    }

    // Inserts the key, or replaces the value if the key already exists.
    void put(const K& key, const V& value) {
        Node* existing = findNode(key);
        if (existing != nullptr) {
            existing->value = value;
            return;
        }
        if ((count + 1) * 4 > bucketCount * 3) rehash(bucketCount * 2);
        int index = indexFor(key, bucketCount);
        buckets[index] = new Node{key, value, buckets[index]};
        ++count;
    }

    // Returns a pointer to the stored value, or nullptr when the key is missing.
    V* find(const K& key) {
        Node* node = findNode(key);
        return node == nullptr ? nullptr : &node->value;
    }

    const V* find(const K& key) const {
        Node* node = findNode(key);
        return node == nullptr ? nullptr : &node->value;
    }

    bool contains(const K& key) const { return findNode(key) != nullptr; }

    bool remove(const K& key) {
        int index = indexFor(key, bucketCount);
        Node* previous = nullptr;
        Node* current = buckets[index];
        while (current != nullptr) {
            if (current->key == key) {
                if (previous == nullptr) buckets[index] = current->next;
                else previous->next = current->next;
                delete current;
                --count;
                return true;
            }
            previous = current;
            current = current->next;
        }
        return false;
    }

    int size() const { return count; }
    bool empty() const { return count == 0; }

    void clear() {
        for (int i = 0; i < bucketCount; ++i) {
            Node* current = buckets[i];
            while (current != nullptr) {
                Node* next = current->next;
                delete current;
                current = next;
            }
            buckets[i] = nullptr;
        }
        count = 0;
    }

    // Calls fn(key, value) for every entry.
    template <typename Fn>
    void forEach(Fn fn) const {
        for (int i = 0; i < bucketCount; ++i) {
            for (Node* node = buckets[i]; node != nullptr; node = node->next) {
                fn(node->key, node->value);
            }
        }
    }

private:
    struct Node {
        K key;
        V value;
        Node* next;
    };

    Node** buckets;
    int bucketCount;
    int count;

    static int indexFor(const K& key, int buckets) {
        return static_cast<int>(hashKey(key) % static_cast<unsigned long>(buckets));
    }

    Node* findNode(const K& key) const {
        for (Node* node = buckets[indexFor(key, bucketCount)]; node != nullptr; node = node->next) {
            if (node->key == key) return node;
        }
        return nullptr;
    }

    void rehash(int newBucketCount) {
        Node** fresh = new Node*[newBucketCount];
        for (int i = 0; i < newBucketCount; ++i) fresh[i] = nullptr;
        for (int i = 0; i < bucketCount; ++i) {
            Node* current = buckets[i];
            while (current != nullptr) {
                Node* next = current->next;
                int index = indexFor(current->key, newBucketCount);
                current->next = fresh[index];
                fresh[index] = current;
                current = next;
            }
        }
        delete[] buckets;
        buckets = fresh;
        bucketCount = newBucketCount;
    }

    void copyNodesFrom(const HashMap& other) {
        other.forEach([this](const K& key, const V& value) { put(key, value); });
    }
};

}  // namespace rahat
