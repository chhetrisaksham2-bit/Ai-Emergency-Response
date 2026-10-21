// Tests for the DATA STRUCTURES part. Owner: Utkarsh Tyagi.
// Run from the phase2 folder: make test   (or: ./build/ds_test)
#include "../common/check.h"
#include "dijkstra.h"
#include "graph.h"
#include "min_heap.h"

using namespace phase2;

struct IntLess {
    bool operator()(int a, int b) const { return a < b; }
};

int main() {
    std::cout << "== MinHeap ==\n";
    MinHeap<int, IntLess> heap(2);  // small start, so it has to grow
    for (int value : {7, 3, 9, 1, 5, 1, 8}) heap.push(value);
    CHECK(heap.size() == 7, "holds 7 items after growing past its first size of 2");
    CHECK(heap.top() == 1, "smallest item is on top");
    std::vector<int> out;
    while (!heap.empty()) out.push_back(heap.pop());
    CHECK(out == std::vector<int>({1, 1, 3, 5, 7, 8, 9}), "items come out smallest first");
    bool threw = false;
    try {
        heap.pop();
    } catch (const std::out_of_range&) {
        threw = true;
    }
    CHECK(threw, "pop() on an empty heap throws an error");

    std::cout << "\n== Graph ==\n";
    //   0 --2-- 1 --2-- 2        3 has no edges
    //    \_______5_____/
    Graph graph;
    for (int i = 0; i < 4; ++i) graph.addVertex();
    CHECK(graph.addUndirectedEdge(0, 1, 2), "add edge 0-1");
    CHECK(graph.addUndirectedEdge(1, 2, 2), "add edge 1-2");
    CHECK(graph.addUndirectedEdge(0, 2, 5), "add edge 0-2");
    CHECK(graph.vertexCount() == 4 && graph.edgeCount() == 3, "4 vertices, 3 edges");
    int neighboursOf1 = 0;
    for (const Graph::Edge* e = graph.edgesOf(1); e != nullptr; e = e->next) ++neighboursOf1;
    CHECK(neighboursOf1 == 2, "vertex 1 has 2 edges in its linked list (to 0 and to 2)");
    CHECK(!graph.addUndirectedEdge(0, 9, 1), "edge to a vertex that does not exist is refused");
    CHECK(!graph.addUndirectedEdge(1, 1, 1), "edge from a vertex to itself is refused");
    CHECK(!graph.addUndirectedEdge(0, 3, 0), "edge with weight 0 is refused");

    std::cout << "\n== Dijkstra ==\n";
    ShortestPaths from0 = dijkstra(graph, 0);
    CHECK(from0.distance[0] == 0, "distance from 0 to itself is 0");
    CHECK(from0.distance[2] == 4, "0 to 2 is 4 (through 1), not the direct edge of 5");
    CHECK(from0.pathTo(2) == std::vector<int>({0, 1, 2}), "path 0 to 2 is 0, 1, 2");
    CHECK(!from0.reachable(3) && from0.pathTo(3).empty(), "3 cannot be reached");
    CHECK(dijkstra(graph, 2).distance[0] == 4, "edges are two-way: 2 to 0 is also 4");
    CHECK(dijkstra(graph, 99).distance.size() == 4 && !dijkstra(graph, 99).reachable(0),
          "a source that does not exist reaches nothing");

    return test::finish("datastructures");
}
