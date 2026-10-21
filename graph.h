// Graph - weighted, undirected graph stored as an ADJACENCY LIST.
// Owner: Utkarsh Tyagi (Data Structures)
//
// Every vertex keeps a singly linked list of the edges that leave it:
//
//   head[0] -> Edge{to 1, 0.8 km} -> Edge{to 5, 1.5 km} -> nullptr
//   head[1] -> Edge{to 0, 0.8 km} -> Edge{to 2, 1.2 km} -> nullptr
//
// Vertices are numbered 0, 1, 2, ... in the order they are added.
// In RAHAT a vertex is a landmark and an edge is a road (weight = km).
// Space: O(V + E). Adding an edge: O(1).
#pragma once

#include <vector>

namespace phase2 {

class Graph {
public:
    struct Edge {
        int to;
        double weight;
        Edge* next;  // next edge leaving the same vertex
    };

    Graph() = default;
    ~Graph();
    Graph(const Graph&) = delete;             // the graph owns its edge nodes
    Graph& operator=(const Graph&) = delete;

    int addVertex();  // returns the new vertex number
    // Two-way edge: a -> b and b -> a. Returns false for bad vertices, a == b, or weight <= 0.
    bool addUndirectedEdge(int a, int b, double weight);

    int vertexCount() const { return static_cast<int>(heads.size()); }
    int edgeCount() const { return edges; }       // each two-way road counted once
    const Edge* edgesOf(int vertex) const;        // first edge; follow ->next for the rest

private:
    std::vector<Edge*> heads;  // heads[v] = first edge of vertex v
    int edges = 0;
    void addDirected(int from, int to, double weight);
};

}  // namespace phase2
