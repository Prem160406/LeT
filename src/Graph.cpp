#include "Graph.h"

Graph::Graph(size_t initialCapacity)
    : capacity(initialCapacity < 8 ? 8 : initialCapacity), vertexCount(0) {
    vertices = new VertexNode[capacity];
}

Graph::~Graph() {
    clear();
    delete[] vertices;
    vertices = nullptr;
}

void Graph::ensureCapacity(size_t minCap) {
    if (minCap <= capacity) return;
    size_t newCap = capacity * 2;
    if (newCap < minCap) newCap = minCap;

    VertexNode* newVerts = new VertexNode[newCap];
    for (size_t i = 0; i < vertexCount; ++i) {
        newVerts[i] = vertices[i]; // Copies code and edge pointer
    }
    delete[] vertices;
    vertices = newVerts;
    capacity = newCap;
}

int Graph::findVertexIndex(const std::string& assetCode) const {
    for (size_t i = 0; i < vertexCount; ++i) {
        if (vertices[i].assetCode == assetCode) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/**
 * Time Complexity: O(V) to scan existing keys.
 */
int Graph::addVertex(const std::string& assetCode) {
    int idx = findVertexIndex(assetCode);
    if (idx != -1) {
        return idx;
    }
    ensureCapacity(vertexCount + 1);
    vertices[vertexCount].assetCode = assetCode;
    vertices[vertexCount].edges = nullptr;
    return static_cast<int>(vertexCount++);
}

/**
 * Time Complexity: O(1) prepend to vertex's linked edge list.
 */
bool Graph::addEdge(const std::string& fromAsset, const std::string& toAsset, double weight) {
    int u = addVertex(fromAsset);
    int v = addVertex(toAsset);

    // Check if edge already exists
    EdgeNode* curr = vertices[u].edges;
    while (curr != nullptr) {
        if (curr->targetNodeIndex == v) {
            curr->dependencyWeight = weight;
            return false;
        }
        curr = curr->next;
    }

    // Prepend new edge
    EdgeNode* newEdge = new EdgeNode(v, toAsset, weight, vertices[u].edges);
    vertices[u].edges = newEdge;
    return true;
}

/**
 * Time Complexity: O(V + E).
 * Explores vertices level-by-level using scratch FIFO array queue.
 */
void Graph::bfs(const std::string& startAsset, CustomArray<std::string>& outAffected) const {
    outAffected.clear();
    int startIdx = findVertexIndex(startAsset);
    if (startIdx == -1 || vertexCount == 0) {
        return;
    }

    bool* visited = new bool[vertexCount];
    for (size_t i = 0; i < vertexCount; ++i) {
        visited[i] = false;
    }

    // Scratch array-based FIFO queue
    int* queue = new int[vertexCount];
    int qHead = 0;
    int qTail = 0;

    // Enqueue start
    visited[startIdx] = true;
    queue[qTail++] = startIdx;

    while (qHead < qTail) {
        int u = queue[qHead++];

        if (u != startIdx) {
            outAffected.push_back(vertices[u].assetCode);
        }

        EdgeNode* edge = vertices[u].edges;
        while (edge != nullptr) {
            int v = edge->targetNodeIndex;
            if (v >= 0 && v < static_cast<int>(vertexCount) && !visited[v]) {
                visited[v] = true;
                queue[qTail++] = v;
            }
            edge = edge->next;
        }
    }

    delete[] queue;
    delete[] visited;
}

/**
 * Time Complexity: O(V + E).
 * Recursive helper for DFS traversal.
 */
void Graph::dfsRecursive(int currIdx, bool* visited, CustomArray<std::string>& outAffected) const {
    visited[currIdx] = true;

    EdgeNode* edge = vertices[currIdx].edges;
    while (edge != nullptr) {
        int v = edge->targetNodeIndex;
        if (v >= 0 && v < static_cast<int>(vertexCount) && !visited[v]) {
            outAffected.push_back(vertices[v].assetCode);
            dfsRecursive(v, visited, outAffected);
        }
        edge = edge->next;
    }
}

/**
 * Time Complexity: O(V + E).
 */
void Graph::dfs(const std::string& startAsset, CustomArray<std::string>& outAffected) const {
    outAffected.clear();
    int startIdx = findVertexIndex(startAsset);
    if (startIdx == -1 || vertexCount == 0) {
        return;
    }

    bool* visited = new bool[vertexCount];
    for (size_t i = 0; i < vertexCount; ++i) {
        visited[i] = false;
    }

    dfsRecursive(startIdx, visited, outAffected);

    delete[] visited;
}

int Graph::getBlastRadiusCount(const std::string& startAsset) const {
    CustomArray<std::string> affected;
    bfs(startAsset, affected);
    return static_cast<int>(affected.size());
}

size_t Graph::getVertexCount() const {
    return vertexCount;
}

void Graph::clear() {
    for (size_t i = 0; i < vertexCount; ++i) {
        EdgeNode* edge = vertices[i].edges;
        while (edge != nullptr) {
            EdgeNode* temp = edge->next;
            delete edge;
            edge = temp;
        }
        vertices[i].edges = nullptr;
    }
    vertexCount = 0;
}

void Graph::printGraph() const {
    std::cout << "Asset Dependency Graph (" << vertexCount << " nodes):" << std::endl;
    for (size_t i = 0; i < vertexCount; ++i) {
        std::cout << "  [" << vertices[i].assetCode << "] -> ";
        EdgeNode* edge = vertices[i].edges;
        if (!edge) {
            std::cout << "(no dependents)";
        }
        while (edge != nullptr) {
            std::cout << edge->targetAssetCode << " ";
            edge = edge->next;
        }
        std::cout << std::endl;
    }
}

void Graph::getAllVertexCodes(CustomArray<std::string>& outCodes) const {
    outCodes.clear();
    for (size_t i = 0; i < vertexCount; ++i) {
        outCodes.push_back(vertices[i].assetCode);
    }
}

void Graph::getAllEdges(CustomArray<GraphEdgeInfo>& outEdges) const {
    outEdges.clear();
    for (size_t i = 0; i < vertexCount; ++i) {
        EdgeNode* edge = vertices[i].edges;
        while (edge != nullptr) {
            outEdges.push_back(GraphEdgeInfo(vertices[i].assetCode, edge->targetAssetCode, edge->dependencyWeight));
            edge = edge->next;
        }
    }
}

void Graph::analyzeAllSources(CustomArray<SourceAnalysisInfo>& outSources) const {
    outSources.clear();

    for (size_t i = 0; i < vertexCount; ++i) {
        const std::string& code = vertices[i].assetCode;

        // 1. Calculate Out-Degree (Direct dependents)
        int outD = 0;
        EdgeNode* edge = vertices[i].edges;
        while (edge != nullptr) {
            ++outD;
            edge = edge->next;
        }

        // 2. Calculate In-Degree (Upstream dependencies)
        int inD = 0;
        for (size_t j = 0; j < vertexCount; ++j) {
            if (i == j) continue;
            EdgeNode* otherEdge = vertices[j].edges;
            while (otherEdge != nullptr) {
                if (otherEdge->targetNodeIndex == static_cast<int>(i)) {
                    ++inD;
                    break;
                }
                otherEdge = otherEdge->next;
            }
        }

        // 3. Compute Transitive Blast Radius via BFS
        int blast = getBlastRadiusCount(code);

        // 4. Assign Criticality Assessment
        std::string crit;
        if (blast >= 4) {
            crit = "CRITICAL (SPOF)";
        } else if (blast >= 2) {
            crit = "HIGH IMPACT";
        } else if (blast == 1) {
            crit = "MODERATE";
        } else {
            crit = "TERMINAL LEAF";
        }

        outSources.push_back(SourceAnalysisInfo(code, outD, inD, blast, crit));
    }

    // Sort sources by totalBlastRadius descending (highest systemic risk first)
    for (size_t i = 1; i < outSources.size(); ++i) {
        SourceAnalysisInfo key = outSources[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && outSources[j].totalBlastRadius < key.totalBlastRadius) {
            outSources[j + 1] = outSources[j];
            --j;
        }
        outSources[j + 1] = key;
    }
}
