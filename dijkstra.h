// Dijkstra's shortest-path algorithm on the Graph, using the MinHeap.
// Owner: Utkarsh Tyagi (Data Structures)
//
// One run from a source vertex gives the shortest distance to EVERY vertex.
// Time: O((V + E) log E)
#pragma once

#include <vector>

#include "graph.h"

namespace phase2 {

struct ShortestPaths {
    int source = -1;
    std::vector<double> distance;  // distance[v] = shortest distance from source (infinity if unreachable)
    std::vector<int> previous;     // previous[v] = vertex just before v on the shortest path (-1 = none)

    bool reachable(int vertex) const;
    // Vertices from the source to `vertex`, in order. Empty if unreachable.
    std::vector<int> pathTo(int vertex) const;
};

ShortestPaths dijkstra(const Graph& graph, int source);

}  // namespace phase2
