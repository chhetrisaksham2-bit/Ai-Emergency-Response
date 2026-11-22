// Graph: adjacency list where each vertex keeps a linked list of its edges.
//
// Vertices are numbered 0, 1, 2, ... in the order they are added.
// EdgeData is whatever the user wants to store on an edge; RAHAT stores a
// Road (distance, speed, traffic information).
//
//   adjacency[v] -> Edge{to, data} -> Edge{to, data} -> nullptr
#pragma once

#include <stdexcept>

namespace rahat {

template <typename EdgeData>
class Graph {
public:
    struct Edge {
        int to;
        EdgeData data;
        Edge* next;
    };

    Graph() : adjacency(nullptr), vertices(0), capacity(8) {
        adjacency = new Edge*[capacity];
        for (int i = 0; i < capacity; ++i) adjacency[i] = nullptr;
    }

    Graph(const Graph& other) : adjacency(nullptr), vertices(0), capacity(8) {
        adjacency = new Edge*[capacity];
        for (int i = 0; i < capacity; ++i) adjacency[i] = nullptr;
        copyFrom(other);
    }

    Graph& operator=(const Graph& other) {
        if (this == &other) return *this;
        clear();
        copyFrom(other);
        return *this;
    }

    ~Graph() {
        clear();
        delete[] adjacency;
    }

    // Adds a vertex with no edges and returns its number.
    int addVertex() {
        if (vertices == capacity) grow();
        adjacency[vertices] = nullptr;
        return vertices++;
    }

    // One-way edge from -> to.
    void addEdge(int from, int to, const EdgeData& data) {
        checkVertex(from);
        checkVertex(to);
        adjacency[from] = new Edge{to, data, adjacency[from]};
    }

    // Two-way edge: a -> b and b -> a with the same data.
    void addUndirectedEdge(int a, int b, const EdgeData& data) {
        addEdge(a, b, data);
        addEdge(b, a, data);
    }

    // First edge leaving v; follow edge->next for the rest.
    Edge* edgesOf(int v) const {
        checkVertex(v);
        return adjacency[v];
    }

    int vertexCount() const { return vertices; }

    int edgeCount() const {
        int total = 0;
        for (int v = 0; v < vertices; ++v) {
            for (Edge* edge = adjacency[v]; edge != nullptr; edge = edge->next) ++total;
        }
        return total;
    }

    void clear() {
        for (int v = 0; v < vertices; ++v) {
            Edge* edge = adjacency[v];
            while (edge != nullptr) {
                Edge* next = edge->next;
                delete edge;
                edge = next;
            }
            adjacency[v] = nullptr;
        }
        vertices = 0;
    }

private:
    Edge** adjacency;
    int vertices;
    int capacity;

    void checkVertex(int v) const {
        if (v < 0 || v >= vertices) throw std::out_of_range("Graph: vertex does not exist");
    }

    void grow() {
        int newCapacity = capacity * 2;
        Edge** bigger = new Edge*[newCapacity];
        for (int i = 0; i < newCapacity; ++i) bigger[i] = i < vertices ? adjacency[i] : nullptr;
        delete[] adjacency;
        adjacency = bigger;
        capacity = newCapacity;
    }

    void copyFrom(const Graph& other) {
        for (int v = 0; v < other.vertices; ++v) addVertex();
        for (int v = 0; v < other.vertices; ++v) {
            // Collect this vertex's edges, then add them in reverse so the
            // copy keeps the same edge order as the original.
            int n = 0;
            for (Edge* e = other.adjacency[v]; e != nullptr; e = e->next) ++n;
            Edge** edges = new Edge*[n > 0 ? n : 1];
            int i = 0;
            for (Edge* e = other.adjacency[v]; e != nullptr; e = e->next) edges[i++] = e;
            for (int k = n - 1; k >= 0; --k) addEdge(v, edges[k]->to, edges[k]->data);
            delete[] edges;
        }
    }
};

}  // namespace rahat
