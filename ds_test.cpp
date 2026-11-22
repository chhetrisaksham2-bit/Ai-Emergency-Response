// Tests for the data structures library. Build and run with: make test-ds
#include <string>
#include <vector>

#include "../testkit/check.h"
#include "bst.h"
#include "doubly_linked_list.h"
#include "graph.h"
#include "hash_map.h"
#include "priority_queue.h"

using namespace rahat;

struct SmallestFirst {
    bool operator()(int a, int b) const { return a < b; }
};

struct Call {
    std::string name;
    int urgency;
    int arrival;
};

// Higher urgency first; same urgency -> earlier arrival first.
struct MostUrgentFirst {
    bool operator()(const Call& a, const Call& b) const {
        if (a.urgency != b.urgency) return a.urgency > b.urgency;
        return a.arrival < b.arrival;
    }
};

static void testPriorityQueue() {
    testkit::section("PriorityQueue");

    PriorityQueue<int, SmallestFirst> heap(SmallestFirst(), 2);  // tiny capacity forces grow()
    int values[] = {42, 7, 19, 3, 88, 7, 1, 56};
    for (int v : values) heap.push(v);
    CHECK_EQ(heap.size(), 8);
    CHECK_EQ(heap.top(), 1);

    PriorityQueue<int, SmallestFirst> copy = heap;  // deep copy
    std::vector<int> order;
    while (!heap.empty()) order.push_back(heap.pop());
    std::vector<int> expected = {1, 3, 7, 7, 19, 42, 56, 88};
    CHECK(order == expected);
    CHECK_EQ(copy.size(), 8);  // popping the original did not touch the copy
    CHECK_EQ(copy.pop(), 1);

    PriorityQueue<Call, MostUrgentFirst> calls;
    calls.push({"dog bite", 2, 1});
    calls.push({"cardiac arrest", 10, 3});
    calls.push({"bleeding", 5, 2});
    calls.push({"stroke", 10, 4});
    CHECK_EQ(calls.pop().name, std::string("cardiac arrest"));  // same urgency as stroke, but earlier
    CHECK_EQ(calls.pop().name, std::string("stroke"));
    CHECK_EQ(calls.pop().name, std::string("bleeding"));
    CHECK_EQ(calls.pop().name, std::string("dog bite"));

    bool threw = false;
    try {
        calls.pop();
    } catch (const std::out_of_range&) {
        threw = true;
    }
    CHECK(threw);
}

static void testHashMap() {
    testkit::section("HashMap");

    HashMap<std::string, int> beds(2);  // tiny bucket array forces rehash
    beds.put("H1", 10);
    beds.put("H2", 4);
    beds.put("H3", 0);
    for (int i = 0; i < 100; ++i) beds.put("extra" + std::to_string(i), i);
    CHECK_EQ(beds.size(), 103);
    CHECK(beds.find("H2") != nullptr && *beds.find("H2") == 4);
    CHECK(beds.find("missing") == nullptr);
    CHECK(beds.find("extra77") != nullptr && *beds.find("extra77") == 77);

    beds.put("H2", 3);  // replace, not duplicate
    CHECK_EQ(*beds.find("H2"), 3);
    CHECK_EQ(beds.size(), 103);

    HashMap<std::string, int> copy = beds;
    CHECK(beds.remove("H1"));
    CHECK(!beds.remove("H1"));
    CHECK(!beds.contains("H1"));
    CHECK(copy.contains("H1"));  // copy is independent

    int total = 0;
    int entries = 0;
    copy.forEach([&](const std::string&, const int& value) {
        total += value;
        ++entries;
    });
    CHECK_EQ(entries, 103);
    CHECK_EQ(total, 10 + 3 + 0 + 4950);

    HashMap<int, std::string> byNumber;
    byNumber.put(101, "A1");
    byNumber.put(-5, "negative key");
    CHECK_EQ(*byNumber.find(101), std::string("A1"));
    CHECK_EQ(*byNumber.find(-5), std::string("negative key"));
    byNumber.clear();
    CHECK(byNumber.empty());
}

static void testBST() {
    testkit::section("BST");

    BST<int, std::string> slots;
    slots.insert(480, "morning rush");  // 08:00
    slots.insert(0, "night");
    slots.insert(1020, "evening rush");  // 17:00
    slots.insert(720, "afternoon");
    slots.insert(1260, "late evening");
    CHECK_EQ(slots.size(), 5);

    int key = -1;
    const std::string* found = slots.floor(500, &key);  // 08:20
    CHECK(found != nullptr && *found == "morning rush");
    CHECK_EQ(key, 480);
    CHECK(slots.floor(1020) != nullptr && *slots.floor(1020) == "evening rush");  // exact match
    CHECK(slots.floor(-1) == nullptr);                                            // nothing before

    std::vector<int> keys;
    slots.inOrder([&](const int& k, const std::string&) { keys.push_back(k); });
    std::vector<int> sorted = {0, 480, 720, 1020, 1260};
    CHECK(keys == sorted);

    std::vector<int> ranged;
    slots.forRange(400, 1100, [&](const int& k, const std::string&) { ranged.push_back(k); });
    std::vector<int> expectedRange = {480, 720, 1020};
    CHECK(ranged == expectedRange);

    BST<int, std::string> copy = slots;
    CHECK(slots.remove(480));   // node with two children
    CHECK(slots.remove(1260));  // leaf
    CHECK(!slots.remove(999));
    CHECK_EQ(slots.size(), 3);
    CHECK(slots.find(480) == nullptr);
    CHECK(copy.find(480) != nullptr);  // copy unaffected

    keys.clear();
    slots.inOrder([&](const int& k, const std::string&) { keys.push_back(k); });
    std::vector<int> afterRemoval = {0, 720, 1020};
    CHECK(keys == afterRemoval);

    slots.insert(720, "replaced");
    CHECK_EQ(*slots.find(720), std::string("replaced"));
    CHECK_EQ(slots.size(), 3);
}

static void testDoublyLinkedList() {
    testkit::section("DoublyLinkedList");

    DoublyLinkedList<std::string> route;
    route.pushBack("Clock Tower");
    route.pushBack("ISBT");
    route.pushFront("City Hospital");
    CHECK_EQ(route.size(), 3);
    CHECK_EQ(route.front(), std::string("City Hospital"));
    CHECK_EQ(route.back(), std::string("ISBT"));

    // Reroute: insert a checkpoint in the middle.
    auto* clockTower = route.head()->next;
    route.insertAfter(clockTower, "Prince Chowk");
    std::vector<std::string> forward;
    route.forEach([&](const std::string& s) { forward.push_back(s); });
    std::vector<std::string> expected = {"City Hospital", "Clock Tower", "Prince Chowk", "ISBT"};
    CHECK(forward == expected);

    // Walk backwards using prev pointers.
    std::vector<std::string> backward;
    for (auto* node = route.tail(); node != nullptr; node = node->prev) backward.push_back(node->value);
    std::vector<std::string> expectedBack = {"ISBT", "Prince Chowk", "Clock Tower", "City Hospital"};
    CHECK(backward == expectedBack);

    DoublyLinkedList<std::string> copy = route;
    route.erase(clockTower);  // road closed: drop a checkpoint
    route.popFront();
    CHECK_EQ(route.size(), 2);
    CHECK_EQ(route.front(), std::string("Prince Chowk"));
    CHECK(route.head()->prev == nullptr);
    route.popBack();
    route.popBack();
    CHECK(route.empty());
    CHECK(route.head() == nullptr && route.tail() == nullptr);
    CHECK_EQ(copy.size(), 4);
}

static void testGraph() {
    testkit::section("Graph");

    Graph<double> roads;
    for (int i = 0; i < 20; ++i) roads.addVertex();  // more than initial capacity
    CHECK_EQ(roads.vertexCount(), 20);

    roads.addUndirectedEdge(0, 1, 2.5);
    roads.addUndirectedEdge(1, 2, 1.0);
    roads.addEdge(2, 19, 4.0);  // one-way
    CHECK_EQ(roads.edgeCount(), 5);

    int neighboursOf1 = 0;
    double totalFrom1 = 0;
    for (auto* edge = roads.edgesOf(1); edge != nullptr; edge = edge->next) {
        ++neighboursOf1;
        totalFrom1 += edge->data;
    }
    CHECK_EQ(neighboursOf1, 2);
    CHECK_NEAR(totalFrom1, 3.5, 1e-9);
    CHECK(roads.edgesOf(19) == nullptr);  // one-way edge does not come back

    Graph<double> copy = roads;
    roads.clear();
    CHECK_EQ(roads.vertexCount(), 0);
    CHECK_EQ(copy.vertexCount(), 20);
    CHECK_EQ(copy.edgeCount(), 5);

    bool threw = false;
    try {
        copy.addEdge(0, 25, 1.0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    CHECK(threw);
}

int main() {
    testPriorityQueue();
    testHashMap();
    testBST();
    testDoublyLinkedList();
    testGraph();
    return testkit::summary("ds");
}
