#ifndef GRAPH_H
#define GRAPH_H

#include "Common.h"
#include <string>
#include <iostream>

/**
 * @brief Singly linked edge node for Adjacency List representation.
 */
struct EdgeNode {
    int targetNodeIndex;
    std::string targetAssetCode;
    double dependencyWeight;
    EdgeNode* next;

    EdgeNode(int targetIdx, const std::string& targetCode, double weight = 1.0, EdgeNode* n = nullptr)
        : targetNodeIndex(targetIdx), targetAssetCode(targetCode), dependencyWeight(weight), next(n) {}
};

/**
 * @brief Graph vertex entry.
 */
struct VertexNode {
    std::string assetCode;
    EdgeNode* edges;

    VertexNode() : assetCode(""), edges(nullptr) {}
    explicit VertexNode(const std::string& code) : assetCode(code), edges(nullptr) {}
};

/**
 * @brief Custom Directed Graph using Adjacency List for asset dependency modeling.
 * Used for blast-radius analysis: when a critical asset fails, BFS/DFS finds all
 * downstream equipment that depend on it directly or indirectly.
 * 
 * Time Complexity:
 * - Add Vertex: O(V) to check uniqueness.
 * - Add Edge: O(E_v) to append to adjacency list.
 * - BFS: O(V + E) exploring breadth of downstream impact.
 * - DFS: O(V + E) exploring full transitive dependency paths.
 * 
 * Space Complexity: O(V + E) adjacency lists.
 */
class Graph {
private:
    VertexNode* vertices;
    size_t capacity;
    size_t vertexCount;

    int findVertexIndex(const std::string& assetCode) const;
    void ensureCapacity(size_t minCap);
    void dfsRecursive(int currIdx, bool* visited, CustomArray<std::string>& outAffected) const;

public:
    explicit Graph(size_t initialCapacity = 32);
    ~Graph();

    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    /**
     * @brief Adds a vertex (asset) to the dependency graph.
     * Time Complexity: O(V).
     * @return index of vertex.
     */
    int addVertex(const std::string& assetCode);

    /**
     * @brief Adds directed dependency edge (from -> to).
     * Meaning: 'to' depends on 'from'. If 'from' fails, 'to' is impacted.
     * Time Complexity: O(1) prepend to vertex's edge list.
     */
    bool addEdge(const std::string& fromAsset, const std::string& toAsset, double weight = 1.0);

    /**
     * @brief Computes blast radius using Breadth-First Search (BFS).
     * Discovers all directly and indirectly affected equipment level by level.
     * 
     * Time Complexity: O(V + E) visiting reachable vertices and exploring their edges.
     * Space Complexity: O(V) for visited set and queue buffer.
     * 
     * @param startAsset Equipment node that suffered the fault.
     * @param outAffected Output list of affected equipment asset codes.
     */
    void bfs(const std::string& startAsset, CustomArray<std::string>& outAffected) const;

    /**
     * @brief Computes blast radius using Depth-First Search (DFS).
     * Discovers deep transitive dependency paths.
     * 
     * Time Complexity: O(V + E).
     * Space Complexity: O(V) recursion call stack.
     */
    void dfs(const std::string& startAsset, CustomArray<std::string>& outAffected) const;

    /**
     * @brief Calculates numerical blast radius count (number of impacted assets).
     * Time Complexity: O(V + E).
     */
    int getBlastRadiusCount(const std::string& startAsset) const;

    /**
     * @brief Returns total vertices.
     * Time Complexity: O(1).
     */
    size_t getVertexCount() const;

    /**
     * @brief Clears all vertices and edge lists.
     * Time Complexity: O(V + E).
     */
    void clear();

    /**
     * @brief Prints graph adjacency list structure.
     * Time Complexity: O(V + E).
     */
    void printGraph() const;
};

#endif // GRAPH_H
