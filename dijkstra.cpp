#include "dijkstra.h"

#include <algorithm>
#include <limits>

#include "min_heap.h"

namespace phase2 {

namespace {

const double kInfinity = std::numeric_limits<double>::infinity();

struct HeapItem {
    double distance;
    int vertex;
};

struct SmallerDistance {
    bool operator()(const HeapItem& a, const HeapItem& b) const { return a.distance < b.distance; }
};

}  // namespace

ShortestPaths dijkstra(const Graph& graph, int source) {
    int n = graph.vertexCount();
    ShortestPaths result;
    result.source = source;
    result.distance.assign(n, kInfinity);
    result.previous.assign(n, -1);
    if (source < 0 || source >= n) return result;

    std::vector<bool> done(n, false);
    MinHeap<HeapItem, SmallerDistance> heap;

    result.distance[source] = 0;
    heap.push({0, source});

    while (!heap.empty()) {
        HeapItem item = heap.pop();  // the closest vertex not finished yet
        int u = item.vertex;
        if (done[u]) continue;       // an older, longer entry for u: skip it ("lazy deletion")
        done[u] = true;

        // Relax every edge leaving u.
        for (const Graph::Edge* edge = graph.edgesOf(u); edge != nullptr; edge = edge->next) {
            double throughU = result.distance[u] + edge->weight;
            if (throughU < result.distance[edge->to]) {
                result.distance[edge->to] = throughU;
                result.previous[edge->to] = u;
                heap.push({throughU, edge->to});
            }
        }
    }
    return result;
}

bool ShortestPaths::reachable(int vertex) const {
    return vertex >= 0 && vertex < static_cast<int>(distance.size()) && distance[vertex] != kInfinity;
}

std::vector<int> ShortestPaths::pathTo(int vertex) const {
    std::vector<int> path;
    if (!reachable(vertex)) return path;
    for (int v = vertex; v != -1; v = previous[v]) path.push_back(v);  // walk back to the source
    std::reverse(path.begin(), path.end());
    return path;
}

}  // namespace phase2
