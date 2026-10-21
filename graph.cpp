#include "graph.h"

namespace phase2 {

Graph::~Graph() {
    for (Edge* edge : heads) {
        while (edge != nullptr) {
            Edge* next = edge->next;
            delete edge;
            edge = next;
        }
    }
}

int Graph::addVertex() {
    heads.push_back(nullptr);  // a new vertex starts with no edges
    return vertexCount() - 1;
}

// New edges go at the FRONT of the list, so adding is O(1).
void Graph::addDirected(int from, int to, double weight) {
    heads[from] = new Edge{to, weight, heads[from]};
}

bool Graph::addUndirectedEdge(int a, int b, double weight) {
    if (a < 0 || b < 0 || a >= vertexCount() || b >= vertexCount() || a == b || weight <= 0) return false;
    addDirected(a, b, weight);
    addDirected(b, a, weight);
    ++edges;
    return true;
}

const Graph::Edge* Graph::edgesOf(int vertex) const {
    if (vertex < 0 || vertex >= vertexCount()) return nullptr;
    return heads[vertex];
}

}  // namespace phase2
