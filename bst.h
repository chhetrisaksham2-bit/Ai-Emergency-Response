// BST: binary search tree of (key -> value) pairs, built from linked nodes.
//
// Left subtree keys < node key < right subtree keys. Keys must support `<`.
// The tree is not self-balancing; for small tables like traffic slots that
// is fine.
//
// RAHAT uses it for traffic time slots: key = slot start minute, and
// floor(t) finds the slot that started most recently before time t.
#pragma once

namespace rahat {

template <typename K, typename V>
class BST {
public:
    BST() : root(nullptr), count(0) {}

    BST(const BST& other) : root(cloneTree(other.root)), count(other.count) {}

    BST& operator=(const BST& other) {
        if (this == &other) return *this;
        Node* fresh = cloneTree(other.root);
        destroyTree(root);
        root = fresh;
        count = other.count;
        return *this;
    }

    ~BST() { destroyTree(root); }

    // Inserts the key, or replaces the value if the key already exists.
    void insert(const K& key, const V& value) {
        Node** link = &root;
        while (*link != nullptr) {
            if (key < (*link)->key) link = &(*link)->left;
            else if ((*link)->key < key) link = &(*link)->right;
            else {
                (*link)->value = value;
                return;
            }
        }
        *link = new Node{key, value, nullptr, nullptr};
        ++count;
    }

    V* find(const K& key) {
        Node* node = findNode(key);
        return node == nullptr ? nullptr : &node->value;
    }

    const V* find(const K& key) const {
        Node* node = findNode(key);
        return node == nullptr ? nullptr : &node->value;
    }

    // Value with the largest key <= key, or nullptr if every key is bigger.
    // If foundKey is given, the matching key is written there.
    const V* floor(const K& key, K* foundKey = nullptr) const {
        Node* best = nullptr;
        Node* current = root;
        while (current != nullptr) {
            if (key < current->key) {
                current = current->left;
            } else {
                best = current;  // current->key <= key, try to find a closer one
                current = current->right;
            }
        }
        if (best == nullptr) return nullptr;
        if (foundKey != nullptr) *foundKey = best->key;
        return &best->value;
    }

    bool remove(const K& key) {
        Node** link = &root;
        while (*link != nullptr && ((*link)->key < key || key < (*link)->key)) {
            link = key < (*link)->key ? &(*link)->left : &(*link)->right;
        }
        Node* target = *link;
        if (target == nullptr) return false;

        if (target->left == nullptr) {
            *link = target->right;
        } else if (target->right == nullptr) {
            *link = target->left;
        } else {
            // Two children: replace with the smallest node of the right subtree.
            Node** successorLink = &target->right;
            while ((*successorLink)->left != nullptr) successorLink = &(*successorLink)->left;
            Node* successor = *successorLink;
            *successorLink = successor->right;
            successor->left = target->left;
            successor->right = target->right;
            *link = successor;
        }
        delete target;
        --count;
        return true;
    }

    int size() const { return count; }
    bool empty() const { return count == 0; }

    void clear() {
        destroyTree(root);
        root = nullptr;
        count = 0;
    }

    // Calls fn(key, value) in ascending key order.
    template <typename Fn>
    void inOrder(Fn fn) const { inOrderFrom(root, fn); }

    // Calls fn(key, value) for keys in [low, high], ascending.
    template <typename Fn>
    void forRange(const K& low, const K& high, Fn fn) const { rangeFrom(root, low, high, fn); }

private:
    struct Node {
        K key;
        V value;
        Node* left;
        Node* right;
    };

    Node* root;
    int count;

    Node* findNode(const K& key) const {
        Node* current = root;
        while (current != nullptr) {
            if (key < current->key) current = current->left;
            else if (current->key < key) current = current->right;
            else return current;
        }
        return nullptr;
    }

    static Node* cloneTree(Node* node) {
        if (node == nullptr) return nullptr;
        return new Node{node->key, node->value, cloneTree(node->left), cloneTree(node->right)};
    }

    static void destroyTree(Node* node) {
        if (node == nullptr) return;
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }

    template <typename Fn>
    static void inOrderFrom(Node* node, Fn& fn) {
        if (node == nullptr) return;
        inOrderFrom(node->left, fn);
        fn(node->key, node->value);
        inOrderFrom(node->right, fn);
    }

    template <typename Fn>
    static void rangeFrom(Node* node, const K& low, const K& high, Fn& fn) {
        if (node == nullptr) return;
        if (low < node->key) rangeFrom(node->left, low, high, fn);
        if (!(node->key < low) && !(high < node->key)) fn(node->key, node->value);
        if (node->key < high) rangeFrom(node->right, low, high, fn);
    }
};

}  // namespace rahat
