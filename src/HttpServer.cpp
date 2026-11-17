/**
 * ============================================================================
 * EXCEPTION NOTICE: HTTP TRANSPORT & JSON SERIALIZATION LAYER ONLY
 * ============================================================================
 * As explicitly authorized under user amendment #2:
 * "Do NOT hand-roll the HTTP server or JSON parsing from raw sockets. Use a single-header,
 *  dependency-free HTTP library for the transport layer only. All 8 core data structures
 *  remain 100% custom/from-scratch as originally specified — this exception applies ONLY
 *  to HTTP request handling and JSON serialization, not to any DSA logic."
 * 
 * Furthermore, per amendment #3:
 * "Handle requests single-threaded (one request processed at a time). Do NOT implement
 *  multi-threading for this scope — it introduces data-race risk on the custom data structures
 *  (Hash Table, Max-Heap, AVL Tree, etc.) with no offsetting benefit for a project this size."
 * ============================================================================
 */

#include "HttpServer.h"
#include "httplib_single.h"
#include "Common.h"
#include <sstream>
#include <iostream>
#include <ctime>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>

// Minimal JSON parsing helpers for standard key-value extraction
static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    size_t colon = json.find(':', pos);
    if (colon == std::string::npos) return "";
    size_t firstQuote = json.find('"', colon);
    if (firstQuote == std::string::npos) return "";
    size_t secondQuote = json.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) return "";
    return json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

static double extractJsonDouble(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return 0.0;
    size_t colon = json.find(':', pos);
    if (colon == std::string::npos) return 0.0;
    size_t start = json.find_first_not_of(" \t\r\n", colon + 1);
    if (start == std::string::npos) return 0.0;
    size_t end = json.find_first_of(",}\r\n", start);
    std::string valStr = json.substr(start, end - start);
    try {
        return std::stod(valStr);
    } catch (...) {
        return 0.0;
    }
}

static long long extractJsonLong(const std::string& json, const std::string& key) {
    return static_cast<long long>(extractJsonDouble(json, key));
}

static int extractJsonInt(const std::string& json, const std::string& key) {
    return static_cast<int>(extractJsonDouble(json, key));
}

static std::string escapeJson(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        if (c == '"') ss << "\\\"";
        else if (c == '\\') ss << "\\\\";
        else if (c == '\b') ss << "\\b";
        else if (c == '\f') ss << "\\f";
        else if (c == '\n') ss << "\\n";
        else if (c == '\r') ss << "\\r";
        else if (c == '\t') ss << "\\t";
        else ss << c;
    }
    return ss.str();
}

HttpServer::HttpServer(Engine& eng, int p)
    : engine(eng), port(p) {}

HttpServer::~HttpServer() {}

void HttpServer::start() {
    httplib_single::SingleThreadedServer server;
    server.set_mount_point("/", "./frontend");

    // =========================================================================
    // GET /api/assets — Lists all registered assets from Hash Table
    // =========================================================================
    server.Get("/api/assets", [this](const httplib_single::Request&, httplib_single::Response& res) {
        CustomArray<Asset*> assets;
        engine.getAllAssets(assets);

        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < assets.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{"
               << "\"assetCode\":\"" << assets[i]->assetCode << "\","
               << "\"name\":\"" << assets[i]->name << "\","
               << "\"location\":\"" << assets[i]->location << "\","
               << "\"supplier\":\"" << assets[i]->supplier << "\","
               << "\"nextCalibrationDue\":" << assets[i]->nextCalibrationDue << ","
               << "\"totalFaultCount\":" << assets[i]->totalFaultCount << ","
               << "\"totalDowntimeHours\":" << assets[i]->totalDowntimeHours
               << "}";
        }
        ss << "]";
        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // POST /api/assets — Registers an asset into Hash Table & AVL Tree
    // =========================================================================
    server.Post("/api/assets", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string code = extractJsonString(req.body, "assetCode");
        std::string name = extractJsonString(req.body, "name");
        std::string loc = extractJsonString(req.body, "location");
        std::string supp = extractJsonString(req.body, "supplier");
        std::string dependsOn = extractJsonString(req.body, "dependsOn");
        if (dependsOn.empty()) {
            dependsOn = extractJsonString(req.body, "upstreamDependency");
        }
        long long calibDue = extractJsonLong(req.body, "nextCalibrationDue");
        double downtime = extractJsonDouble(req.body, "totalDowntimeHours");

        if (code.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"assetCode is required\"}", "application/json");
            return;
        }

        if (supp.empty()) supp = "Standard Lab Corp";

        Asset newAsset(code, name, loc, calibDue, downtime, supp);
        bool success = engine.registerAsset(newAsset);

        if (success) {
            engine.getGraph().addVertex(code);
            if (!dependsOn.empty()) {
                engine.getGraph().addEdge(dependsOn, code);
            }
            res.set_content("{\"status\":\"success\",\"message\":\"Asset registered successfully\"}", "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Asset code already exists\"}", "application/json");
        }
    });

    // =========================================================================
    // POST /api/graph/edge — Adds a dependency edge between existing assets
    // =========================================================================
    server.Post("/api/graph/edge", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string from = extractJsonString(req.body, "from");
        std::string to = extractJsonString(req.body, "to");
        if (from.empty() || to.empty()) {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Both 'from' and 'to' asset codes are required\"}", "application/json");
            return;
        }
        if (from == to) {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Self-loop dependencies are not permitted\"}", "application/json");
            return;
        }
        bool added = engine.getGraph().addEdge(from, to);
        if (added) {
            res.set_content("{\"status\":\"success\",\"message\":\"Dependency connection added successfully\"}", "application/json");
        } else {
            res.set_content("{\"status\":\"info\",\"message\":\"Dependency connection updated/already exists\"}", "application/json");
        }
    });

    // =========================================================================
    // POST /api/reports — Buffers report into CircularQueue and processes pipeline
    // =========================================================================
    server.Post("/api/reports", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string rId = extractJsonString(req.body, "reportId");
        std::string aCode = extractJsonString(req.body, "assetCode");
        int severity = extractJsonInt(req.body, "severity");
        int overdue = extractJsonInt(req.body, "daysOverdue");
        double dev = extractJsonDouble(req.body, "measuredDeviation");
        long long ts = extractJsonLong(req.body, "timestamp");
        std::string desc = extractJsonString(req.body, "description");

        if (rId.empty()) {
            rId = "RPT-" + std::to_string(rand() % 9000 + 1000);
        }
        if (ts <= 0) {
            ts = static_cast<long long>(time(nullptr));
        }

        FaultReport report(rId, aCode, severity, overdue, dev, ts, desc);

        // Step 1: Buffer in Circular Queue
        bool buffered = engine.submitReportToBuffer(report);
        if (!buffered) {
            res.status = 503;
            res.set_content("{\"status\":\"error\",\"message\":\"Circular Queue full! Buffer saturated.\"}", "application/json");
            return;
        }

        // Steps 2-5: Process buffered report through HashTable, DLL, Drift calc, Blast-Radius, and MaxHeap
        bool processed = engine.processNextBufferReport();
        if (processed) {
            res.set_content("{\"status\":\"success\",\"message\":\"Report enqueued and prioritized into Max-Heap\"}", "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Failed to process report — asset code not found in Hash Table.\"}", "application/json");
        }
    });

    // =========================================================================
    // GET /api/triage — Retrieves all active triage tasks from Max-Heap
    // =========================================================================
    server.Get("/api/triage", [this](const httplib_single::Request&, httplib_single::Response& res) {
        CustomArray<TriageTask> tasks;
        engine.getTriageTasksSnapshot(tasks);

        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < tasks.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{"
               << "\"taskId\":\"" << tasks[i].taskId << "\","
               << "\"assetCode\":\"" << tasks[i].assetCode << "\","
               << "\"assetName\":\"" << tasks[i].assetName << "\","
               << "\"severity\":" << tasks[i].severity << ","
               << "\"daysOverdue\":" << tasks[i].daysOverdue << ","
               << "\"driftVelocity\":" << tasks[i].driftVelocity << ","
               << "\"riskScore\":" << tasks[i].riskScore << ","
               << "\"blastRadiusCount\":" << tasks[i].blastRadiusCount << ","
               << "\"blastMultiplier\":" << tasks[i].blastMultiplier << ","
               << "\"timestamp\":" << tasks[i].timestamp << ","
               << "\"description\":\"" << tasks[i].description << "\""
               << "}";
        }
        ss << "]";
        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // POST /api/dispatch — Pops highest-urgency task from Max-Heap (O(log N))
    // =========================================================================
    server.Post("/api/dispatch", [this](const httplib_single::Request&, httplib_single::Response& res) {
        TriageTask topTask;
        bool ok = engine.dispatchHighestUrgencyTask(topTask);
        if (ok) {
            std::ostringstream ss;
            ss << "{\"status\":\"success\",\"dispatched\":{"
               << "\"taskId\":\"" << topTask.taskId << "\","
               << "\"assetCode\":\"" << topTask.assetCode << "\","
               << "\"riskScore\":" << topTask.riskScore
               << "}}";
            res.set_content(ss.str(), "application/json");
        } else {
            res.status = 404;
            res.set_content("{\"status\":\"error\",\"message\":\"Triage queue is empty.\"}", "application/json");
        }
    });

    // =========================================================================
    // POST /api/undo — Pops from UndoStack and reverses last state modification
    // =========================================================================
    server.Post("/api/undo", [this](const httplib_single::Request&, httplib_single::Response& res) {
        bool ok = engine.rollbackLastAction();
        if (ok) {
            res.set_content("{\"status\":\"success\",\"message\":\"Last operation successfully rolled back.\",\"remainingUndo\":" 
                            + std::to_string(engine.getUndoStackDepth()) + "}", "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Undo stack is empty.\",\"remainingUndo\":0}", "application/json");
        }
    });

    // =========================================================================
    // POST /api/chat — Intelligent DSA Copilot Query Engine
    // =========================================================================
    server.Post("/api/chat", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string rawQuery = extractJsonString(req.body, "query");
        if (rawQuery.empty()) {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Empty query\"}", "application/json");
            return;
        }

        // Lowercase and sanitize query
        std::string q = rawQuery;
        for (char& c : q) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string cleanQ;
        for (char c : q) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || std::isspace(static_cast<unsigned char>(c))) {
                cleanQ += c;
            } else {
                cleanQ += ' ';
            }
        }

        std::stringstream ssTokens(cleanQ);
        std::string word;
        std::vector<std::string> tokens;
        while (ssTokens >> word) tokens.push_back(word);

        auto hasWord = [&](const std::string& target) {
            for (const auto& t : tokens) {
                if (t == target) return true;
            }
            return false;
        };

        auto hasSub = [&](const std::string& target) {
            return cleanQ.find(target) != std::string::npos;
        };

        bool isLeast = hasWord("least") || hasWord("lowest") || hasWord("min") || hasWord("minimum") ||
                       hasWord("fewest") || hasWord("safest") || hasWord("healthiest") || hasWord("cleanest");
        bool isMost = hasWord("most") || hasWord("highest") || hasWord("max") || hasWord("maximum") ||
                      hasWord("worst") || hasWord("top") || hasWord("greatest") || hasWord("critical");
        (void)isMost;

        std::string responseText;
        bool isCode = false;

        // 0. Handle Incomplete / Ambiguous Queries (e.g. "which", "which is", "what", "help")
        if (tokens.empty() || (tokens.size() <= 2 && (hasWord("which") || hasWord("what") || hasWord("who") || hasWord("help") || hasWord("show")) && !hasWord("hood") && !hasWord("dsa") && !hasWord("downtime") && !hasWord("risk") && !hasWord("fault") && !hasWord("spof") && !hasWord("heap") && !hasWord("tree") && !hasWord("queue"))) {
            responseText = "Please specify what you would like to analyze. I can assist with both live laboratory diagnostics and deep \"Under the Hood\" Data Structures & Algorithms:\n\n"
                           "UNDER THE HOOD & DSA ARCHITECTURE:\n"
                           "• \"How does this work under the hood?\" — 5-stage real-time pipeline\n"
                           "• \"What data structures are used?\" — All 8 custom C++ data structures (Zero STL)\n"
                           "• \"How is risk score calculated?\" — Mathematical formula & blast multiplier\n"
                           "• \"Explain Big-O time complexity\" — Algorithmic bounds breakdown\n"
                           "• \"How does the Max-Heap / Hash Table / AVL Tree work?\" — Data structure internals\n\n"
                           "LIVE FLEET DIAGNOSTICS:\n"
                           "• \"Which is most faulty?\" / \"Which is least faulty?\"\n"
                           "• \"Which has the highest downtime?\" (Merge Sort)\n"
                           "• \"Which is highest risk?\" (Max-Heap triage root)\n"
                           "• \"Which machine is a Single Point of Failure?\" (Graph BFS)\n"
                           "• \"Which supplier is worst (Lemon)?\" (Vendor quality scorecard)\n"
                           "• \"Predict next failure\" (Remaining Useful Life Forecast)";
        }
        // UNDER THE HOOD: Architecture & Pipeline
        else if (hasSub("add to graph") || hasSub("add graph") || hasSub("new addition") || hasSub("add equipment") || hasSub("how to add") || hasSub("connect equipment")) {
            responseText = "How Graph Additions & New Dependencies Work:\n\n"
                           "1. Registering New Equipment (Tab 1):\n"
                           "   In the \"Register Equipment\" form, pick an \"Upstream Dependency (Relies On)\":\n"
                           "   • If an upstream dependency is selected, a directed edge (Provider ──> Dependent) is created automatically.\n"
                           "   • If (None) is selected, the equipment is registered as a standalone root backbone node.\n\n"
                           "2. Connecting Existing Equipment (Tab 2):\n"
                           "   Use the \"Connect Dependency Link\" form under the Equipment Dependency Map:\n"
                           "   • Select Provider (e.g. LAB-UPS-01)\n"
                           "   • Select Dependent (e.g. LAB-CENT-009)\n"
                           "   • Click \"Add Dependency Link\" — the graph immediately renders the new link and recalculates blast radius & SPOFs!\n\n"
                           "3. Dynamic Subsystem Filtering:\n"
                           "   New nodes dynamically inherit downstream reachability and appear in the Sub-Graph Reference Keys.";
        }
        // UNDER THE HOOD: Reset Graph Interactions
        else if (hasSub("reset graph") || hasSub("reset map") || hasSub("clear graph") || hasSub("reset interaction") || hasSub("deselect") || hasSub("unselect")) {
            responseText = "How to Reset Graph Interactions:\n\n"
                           "1. \"Reset Map View\" Button: Click the button at the top-right of Tab 2 to clear all selections, restore the default view, uncheck Isolate View, and reset the Reference Key to \"ALL\".\n"
                           "2. Click Empty Canvas Background: Click anywhere on the gray canvas background outside bubbles to immediately deselect the current node and clear blast highlights.\n"
                           "3. Re-click Active Bubble: Clicking an already-selected equipment bubble toggles it off.\n"
                           "4. Escape Key: Press [Esc] anytime to clear active selection and wave animations.";
        }
        else if (hasSub("under the hood") || hasSub("how does it work") || hasSub("how it works") ||
                 hasWord("architecture") || hasWord("pipeline") || hasWord("internals") || hasSub("how does this work") ||
                 hasSub("how this works") || hasSub("explain system") || hasSub("explain engine")) {
            responseText = "Under the Hood: 5-Stage Real-Time Pipeline Architecture\n\n"
                           "1. Circular Queue [O(1) Intake]:\n"
                           "   Bounded ring buffer ingests sensor alerts & engineer tickets without memory spikes or heap reallocations.\n\n"
                           "2. Hash Table [O(1) Avg Lookup]:\n"
                           "   Asset code mapped to hardware records using separate chaining collision resolution & dynamic prime rehashing.\n\n"
                           "3. Doubly Linked List [O(1) History & Drift]:\n"
                           "   Chronological telemetry chain appends readings; computes physical Drift Velocity: Delta(Dev) / Delta(Time).\n\n"
                           "4. Directed Graph [BFS O(V + E) Blast Radius]:\n"
                           "   Traverses downstream power/data dependencies to identify Single Points of Failure (SPOFs) and cascade multipliers.\n\n"
                           "5. Binary Max-Heap [O(log N) Priority Queue]:\n"
                           "   Ranks tasks by Effective Risk Score = BaseRisk × BlastMultiplier. Highest-risk emergency stays at root for O(1) peek and O(log N) dispatch.";
        }
        // UNDER THE HOOD: Data Structures Overview / Zero STL
        else if (hasSub("data structure") || hasWord("dsa") || hasSub("data structures") || hasWord("structures") || hasSub("zero stl") || hasSub("from scratch")) {
            responseText = "Custom Data Structures Implemented (100% From Scratch, Zero STL Containers):\n\n"
                           "1. Circular Queue: Bounded O(1) buffer with FIFO wraparound & batch dequeue.\n"
                           "2. Hash Table: O(1) average lookup with separate chaining linked lists & dynamic rehashing.\n"
                           "3. Doubly Linked List: O(1) append & bidirectional traversal for sensor telemetry & drift velocity.\n"
                           "4. AVL Tree: O(log N) self-balancing BST ordered by calendar due dates with O(log N + K) range queries.\n"
                           "5. Binary Max-Heap: O(1) root access and O(log N) bubble-up/down for emergency triage.\n"
                           "6. Directed Graph: Adjacency list representation with Breadth-First Search (BFS) for blast-radius calculation.\n"
                           "7. Undo Stack: O(1) LIFO transaction rollback for fault reports & technician dispatches.\n"
                           "8. Merge Sort: Guaranteed O(N log N) divide-and-conquer stable sorting for audits.";
        }
        // UNDER THE HOOD: Big-O Time & Space Complexity
        else if (hasWord("complexity") || hasSub("big o") || hasSub("big-o") || hasSub("time complexity") || hasSub("space complexity")) {
            responseText = "Theoretical & Practical Complexity Bounds (Big-O Analysis):\n\n"
                           "• Fault Report Intake: O(1) time, O(Capacity) space (Circular Queue)\n"
                           "• Asset Record Lookup: O(1) average time, O(N) space (Hash Table)\n"
                           "• Telemetry History Append: O(1) time (Doubly Linked List tail pointer)\n"
                           "• Drift Velocity Calculation: O(1) time (Linear interpolation between consecutive nodes)\n"
                           "• Blast Radius Impact: O(V + E) time (Graph BFS downstream reachability)\n"
                           "• Priority Queue Insertion: O(log N) time (Max-Heap bubble-up)\n"
                           "• Top Urgency Dispatch: O(log N) time (Max-Heap extract root & heapify)\n"
                           "• Calibration Window Range Query: O(log N + K) time (AVL Tree)\n"
                           "• Audit Compliance Sorting: O(N log N) time, O(N) temp space (Merge Sort)\n"
                           "• Transaction Undo Rollback: O(1) time, O(History) space (Undo Stack)";
            isCode = true;
        }
        // UNDER THE HOOD: Risk Score Formula
        else if (hasSub("risk score") || hasSub("risk formula") || hasSub("calculate risk") || hasSub("how is risk") || hasSub("how is priority") || hasSub("priority formula") || hasSub("how is it calculated")) {
            responseText = "Emergency Prioritization Risk Formula:\n\n"
                           "Base Risk = (Severity × 5.0) + (DaysOverdue × 1.5) + (|DriftVelocity| × 10.0)\n\n"
                           "Blast Multiplier = 1.0 + (DownstreamDependents × 0.20)\n\n"
                           "Effective Risk Score = Base Risk × Blast Multiplier\n\n"
                           "Why each term matters:\n"
                           "• Severity (1-5): Reflects immediate hardware breakdown danger.\n"
                           "• Days Overdue: Penalizes deferred preventive maintenance.\n"
                           "• Drift Velocity (mm/s): Catches rapidly accelerating wear before sensor tolerance breach.\n"
                           "• Blast Multiplier: Elevates root infrastructure (e.g. UPS) whose failure halts multiple dependent instruments.";
            isCode = true;
        }
        // UNDER THE HOOD: Circular Queue
        else if ((hasWord("circular") && hasWord("queue")) || (hasWord("queue") && !hasWord("triage") && !hasWord("priority") && !hasWord("emergency"))) {
            responseText = "Circular Queue (Under the Hood):\n\n"
                           "• Fixed-size contiguous array buffer with front, rear, and count indices.\n"
                           "• Wraparound Logic: rear = (rear + 1) % capacity. Eliminates costly element shifting.\n"
                           "• Bounded Memory Invariant: When count == capacity, new fault arrivals are rejected O(1) or flagged to prevent memory exhaustion.\n"
                           "• Batch Dequeue: Supports O(K) bulk retrieval of K elements in strict FIFO order across circular boundaries.";
        }
        // UNDER THE HOOD: Hash Table
        else if (hasSub("hash table") || hasSub("hash map") || hasWord("rehashing") || (hasWord("collision") && !hasWord("packet")) || hasWord("collisions")) {
            HashStats stats = engine.getRegistryStats();
            std::ostringstream rep;
            rep << "Hash Table with Separate Chaining (Under the Hood):\n\n"
                << "• Key: Asset Code string (e.g. \"LAB-PC-017\") hashed via polynomial accumulation modulo bucket count.\n"
                << "• Collision Handling: Linked list chaining per bucket. Avoids clustering seen in linear probing.\n"
                << "• Dynamic Rehashing: Automatically doubles capacity to the next prime when Load Factor exceeds 0.75.\n"
                << "• Live Engine Telemetry: " << stats.totalElements << " assets stored across " << stats.bucketCount
                << " buckets (Load Factor: " << stats.loadFactor << ", Collisions: " << stats.totalCollisions << ", Max Chain: " << stats.maxChainLength << ").";
            responseText = rep.str();
        }
        // UNDER THE HOOD: Doubly Linked List
        else if (hasSub("doubly linked") || (hasWord("linked") && hasWord("list")) || hasWord("telemetry")) {
            responseText = "Doubly Linked List (Under the Hood):\n\n"
                           "• Every asset maintains a bi-directional chain of historical fault & calibration events.\n"
                           "• Structure: Each node stores prev, next, timestamp, deviation (mm), drift velocity, and description.\n"
                           "• O(1) Append: Fast tail pointer insertion without iterating through historical records.\n"
                           "• Drift Velocity Formula: (CurrentDev - PrevDev) / (CurrentTime - PrevTime). Negative values detect sudden rotor shock; positive values indicate progressive wear.";
        }
        // UNDER THE HOOD: AVL Tree
        else if (hasWord("avl") || (hasWord("tree") && !hasWord("heap")) || hasSub("self balancing") || hasSub("rotations")) {
            responseText = "Self-Balancing AVL Tree (Under the Hood):\n\n"
                           "• Strict height-balanced BST ordered by calendar Due Date (days from today).\n"
                           "• Balance Invariant: |Height(Left) - Height(Right)| <= 1 at every node.\n"
                           "• Rebalancing Rotations: Executes single (LL, RR) or double (LR, RL) rotations in O(1) during insertion/deletion.\n"
                           "• Range Query: Finds all calibrations within [startDay, endDay] in O(log N + K) time.\n"
                           "• Search Operations: findMin() [earliest due], findMax() [furthest due], inorderPredecessor(), and inorderSuccessor().";
        }
        // UNDER THE HOOD: Binary Max-Heap
        else if (hasSub("max heap") || hasSub("max-heap") || hasWord("heapify") || (hasWord("binary") && hasWord("heap"))) {
            responseText = "Binary Max-Heap Priority Queue (Under the Hood):\n\n"
                           "• Contiguous dynamic array storing complete binary tree where Parent.Risk >= Child.Risk.\n"
                           "• Root Position (Index 0): Always guarantees O(1) instant access to the single highest-urgency emergency in the lab.\n"
                           "• Bubble-Up Insertion: O(log N) compares new task with parent at index (i - 1) / 2.\n"
                           "• Extract-Max (Dispatch): O(log N) swaps root with last element, pops, and heapifies down comparing children at 2*i + 1 and 2*i + 2.";
        }
        // UNDER THE HOOD: Graph BFS & Blast Radius
        else if ((hasWord("graph") || hasWord("bfs") || hasWord("topology")) && !hasWord("spof")) {
            responseText = "Directed Dependency Graph (Under the Hood):\n\n"
                           "• Modeled as an Adjacency List: Key -> Array of downstream dependent equipment codes.\n"
                           "• Edge Direction: LAB-UPS-01 ──> LAB-NET-SW means Switch relies on UPS for power.\n"
                           "• Breadth-First Search (BFS): Evaluates downstream cascade impact in O(V + E) time using a FIFO traversal queue.\n"
                           "• SPOF Vulnerability: Nodes with in-degree 0 and large reachable BFS sets are flagged as Single Points of Failure.";
        }
        // UNDER THE HOOD: Undo Stack
        else if ((hasWord("undo") || hasWord("stack") || hasWord("rollback")) && !hasWord("dispatch")) {
            responseText = "Undo Stack (Under the Hood):\n\n"
                           "• LIFO (Last-In-First-Out) transaction stack providing O(1) rollback for technician actions.\n"
                           "• Transaction Records: Stores action type (Fault Enqueued, Job Dispatched, Batch Dequeued), target asset, and snapshot data.\n"
                           "• Reversibility: Dispatched jobs can be pushed back into the Max-Heap; erroneous fault tickets can be cleanly purged from telemetry.";
        }
        // UNDER THE HOOD: Merge Sort
        else if (hasSub("merge sort") || (hasWord("sort") && !hasWord("fault") && !hasWord("downtime") && !hasWord("calib"))) {
            responseText = "Merge Sort (Under the Hood):\n\n"
                           "• Divide-and-Conquer algorithm providing guaranteed O(N log N) worst-case sorting (unlike QuickSort's O(N^2) worst case).\n"
                           "• Stability: Preserves the relative order of equipment with identical downtime or fault counts.\n"
                           "• Standalone Scratch Implementation: Zero std::sort or STL vectors. Uses recursive array splitting and auxiliary buffer merging.";
        }
        // UNDER THE HOOD: Lemon Index
        else if (hasSub("lemon index") || hasSub("lemon formula") || hasSub("supplier formula")) {
            responseText = "Supplier Lemon Index Scorecard Formula:\n\n"
                           "Score = 100.0 - (15.0 × AvgFaults + 20.0 × |AvgDrift| + 2.0 × AvgDowntimeHours)\n\n"
                           "Quality Tiers:\n"
                           "• Tier A (Score >= 80): Certified reliable; recommended for repeat procurement.\n"
                           "• Tier B (Score 50-79): Acceptable with regular scheduled calibrations.\n"
                           "• Tier C / LEMON (Score < 50): DEFECTIVE VENDOR. Triggers procurement freeze and warranty claim.";
            isCode = true;
        }
        // 1. Fault Query (Most vs Least Faulty)
        else if (hasWord("fault") || hasWord("faults") || hasWord("faulty") || hasWord("defect") || hasWord("defects") || hasWord("issues")) {
            CustomArray<Asset*> faultyAssets;
            engine.getFaultyEquipmentReport(SORT_BY_FAULT_COUNT_DESC, faultyAssets);

            if (isLeast) {
                std::vector<Asset*> cleanAssets;
                for (size_t i = 0; i < faultyAssets.size(); ++i) {
                    if (faultyAssets[i]->totalFaultCount == 0) cleanAssets.push_back(faultyAssets[i]);
                }
                std::ostringstream rep;
                if (!cleanAssets.empty()) {
                    rep << "Least Faulty Equipment (Nominal Clean Fleet):\n"
                        << "Found " << cleanAssets.size() << " unit(s) with 0 logged faults:\n";
                    for (auto* a : cleanAssets) {
                        rep << "• " << a->name << " (" << a->assetCode << ") — Downtime: " << a->totalDowntimeHours << "h | Status: NOMINAL\n";
                    }
                } else if (faultyAssets.size() > 0) {
                    Asset* lowest = faultyAssets[faultyAssets.size() - 1];
                    rep << "Least Faulty Equipment: " << lowest->name << " (" << lowest->assetCode << ") with " << lowest->totalFaultCount << " logged fault(s).";
                }
                responseText = rep.str();
            } else {
                if (faultyAssets.size() > 0) {
                    Asset* top = faultyAssets[0];
                    int blast = engine.getGraph().getBlastRadiusCount(top->assetCode);
                    std::ostringstream rep;
                    rep << "Most Faulty Equipment (Fault Frequency Leader via Merge Sort):\n"
                        << "Equipment: " << top->name << " (" << top->assetCode << ")\n"
                        << "Total Logged Faults: " << top->totalFaultCount << "\n"
                        << "Accumulated Downtime: " << top->totalDowntimeHours << " hours\n"
                        << "Location: " << top->location << "\n"
                        << "Supplier: " << top->supplier << "\n"
                        << "Downstream Blast Impact: " << blast << " machines";
                    responseText = rep.str();
                } else {
                    responseText = "No equipment faults recorded on file.";
                }
            }
        }
        // 2. Downtime Query (Highest vs Lowest Downtime)
        else if (hasWord("downtime") || hasWord("offline") || hasWord("outage") || hasWord("hours")) {
            CustomArray<Asset*> assetsByDowntime;
            engine.getFaultyEquipmentReport(SORT_BY_DOWNTIME_DESC, assetsByDowntime);
            if (assetsByDowntime.size() > 0) {
                if (isLeast) {
                    Asset* lowest = assetsByDowntime[assetsByDowntime.size() - 1];
                    std::ostringstream rep;
                    rep << "Lowest Downtime Equipment: " << lowest->name << " (" << lowest->assetCode
                        << ") with only " << lowest->totalDowntimeHours << "h downtime (" << lowest->totalFaultCount << " faults).";
                    responseText = rep.str();
                } else {
                    Asset* top = assetsByDowntime[0];
                    std::ostringstream rep;
                    rep << "Highest Downtime Equipment (Downtime Leader via Merge Sort):\n"
                        << "Equipment: " << top->name << " (" << top->assetCode << ")\n"
                        << "Accumulated Downtime: " << top->totalDowntimeHours << " hours\n"
                        << "Faults: " << top->totalFaultCount << " | Supplier: " << top->supplier;
                    responseText = rep.str();
                }
            } else {
                responseText = "No downtime records available.";
            }
        }
        // 3. Risk / Triage Queue Query (Highest vs Lowest Risk)
        else if (hasWord("risk") || hasWord("urgent") || hasWord("urgency") || hasWord("emergency") || hasWord("triage") || hasWord("priority")) {
            CustomArray<TriageTask> tasks;
            engine.getTriageTasksSnapshot(tasks);
            if (tasks.empty()) {
                responseText = "Triage Queue is currently empty. No emergency jobs pending.";
            } else {
                if (isLeast) {
                    const TriageTask& lowest = tasks[tasks.size() - 1];
                    std::ostringstream rep;
                    rep << "Lowest Risk Triage Task: [" << lowest.taskId << "] " << lowest.assetName << " (" << lowest.assetCode << ")\n"
                        << "Computed Risk Score: " << lowest.riskScore << " | Severity: " << lowest.severity << " | Overdue: " << lowest.daysOverdue << "d";
                    responseText = rep.str();
                } else {
                    const TriageTask& highest = tasks[0];
                    std::ostringstream rep;
                    rep << "Highest Urgency Job (Max-Heap Root — Dispatches First):\n"
                        << "Task ID: " << highest.taskId << " — " << highest.assetName << " (" << highest.assetCode << ")\n"
                        << "Effective Risk Score: " << highest.riskScore << " (Base Risk factored with " << highest.blastMultiplier << "x Blast Multiplier)\n"
                        << "Severity: " << highest.severity << "/5 | Overdue: " << highest.daysOverdue << " days | Drift: " << highest.driftVelocity << " mm/s\n"
                        << "Description: " << highest.description;
                    responseText = rep.str();
                }
            }
        }
        // 4. Specific Entity Lookup (by Asset Code or Name keyword)
        else if ((hasSub("lab-") || hasWord("ups") || hasWord("switch") || hasWord("centrifuge") ||
                  hasWord("spectrometer") || hasWord("chiller") || hasWord("laser") || hasWord("workstation")) &&
                 !hasWord("service") && !hasWord("safely") && !hasWord("maintenance") && !hasWord("window")) {
            CustomArray<Asset*> allAssets;
            engine.getAllAssets(allAssets);
            Asset* matched = nullptr;

            for (size_t i = 0; i < allAssets.size(); ++i) {
                std::string codeLower = allAssets[i]->assetCode;
                std::string nameLower = allAssets[i]->name;
                for (char& c : codeLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                for (char& c : nameLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                if (hasSub(codeLower)) { matched = allAssets[i]; break; }
                if (hasWord("ups") && codeLower.find("ups") != std::string::npos) { matched = allAssets[i]; break; }
                if (hasWord("switch") && codeLower.find("net-sw") != std::string::npos) { matched = allAssets[i]; break; }
                if (hasWord("centrifuge") && codeLower.find("cent") != std::string::npos) { matched = allAssets[i]; break; }
                if (hasWord("spectrometer") && codeLower.find("spec") != std::string::npos) { matched = allAssets[i]; break; }
                if (hasWord("chiller") && codeLower.find("cool") != std::string::npos) { matched = allAssets[i]; break; }
                if (hasWord("laser") && codeLower.find("laser") != std::string::npos) { matched = allAssets[i]; break; }
                if (hasWord("workstation") && codeLower.find("pc") != std::string::npos) { matched = allAssets[i]; break; }
            }

            if (matched) {
                int blast = engine.getGraph().getBlastRadiusCount(matched->assetCode);
                double latestDev = 0.0, latestDrift = 0.0;
                const HistoryEntry* latest = matched->history->getLatestEntry();
                if (latest) {
                    latestDev = latest->measuredDeviation;
                    latestDrift = latest->computedDrift;
                }
                std::ostringstream rep;
                rep << "Equipment Diagnostic Report for " << matched->name << " (" << matched->assetCode << "):\n"
                    << "• Location: " << matched->location << " | Supplier: " << matched->supplier << "\n"
                    << "• Logged Faults: " << matched->totalFaultCount << " | Total Downtime: " << matched->totalDowntimeHours << "h\n"
                    << "• Latest Sensor Deviation: " << latestDev << " mm (Drift Velocity: " << (latestDrift >= 0 ? "+" : "") << latestDrift << " mm/s)\n"
                    << "• Transitive Blast Radius: " << blast << " downstream dependent(s)\n"
                    << "• Next Calibration Due: in " << matched->nextCalibrationDue << " days";
                responseText = rep.str();
            } else {
                responseText = "Could not find matching equipment in the Hash Table asset registry.";
            }
        }
        // 5. Single Point of Failure (SPOF) / Graph Blast Radius
        else if (hasWord("spof") || hasSub("single point") || hasWord("blast") || hasWord("cascade") || hasWord("depend")) {
            CustomArray<SourceAnalysisInfo> sources;
            engine.getSourceEquipmentReport(sources);
            std::ostringstream rep;
            rep << "Critical Single Points of Failure (via Graph BFS):\n";
            int count = 0;
            for (size_t i = 0; i < sources.size(); ++i) {
                Asset* a = engine.getAsset(sources[i].assetCode);
                std::string aName = a ? a->name : sources[i].assetCode;
                if (sources[i].criticality.find("CRITICAL") != std::string::npos || sources[i].totalBlastRadius >= 3) {
                    rep << "• [" << sources[i].assetCode << "] " << aName
                        << ": Blast Radius = " << sources[i].totalBlastRadius << " machines (" << sources[i].criticality << ")\n";
                    ++count;
                }
            }
            if (count > 0) {
                rep << "Recommendation: Prioritize redundant power/network links to eliminate failure cascades.";
            } else if (sources.size() > 0) {
                Asset* a0 = engine.getAsset(sources[0].assetCode);
                std::string a0Name = a0 ? a0->name : sources[0].assetCode;
                rep << "Top Source: [" << sources[0].assetCode << "] " << a0Name << " with blast radius of " << sources[0].totalBlastRadius << " machines.";
            } else {
                rep << "No dependency cascades found.";
            }
            responseText = rep.str();
        }
        // 6. Predict Failure / RUL
        else if (hasWord("predict") || hasWord("next") || hasWord("expect") || hasWord("rul") || hasWord("fail") || hasWord("failure")) {
            CustomArray<Asset*> assetsByDowntime;
            engine.getFaultyEquipmentReport(SORT_BY_DOWNTIME_DESC, assetsByDowntime);
            if (!assetsByDowntime.empty()) {
                Asset* crit = nullptr;
                for (size_t i = 0; i < assetsByDowntime.size(); ++i) {
                    const HistoryEntry* h = assetsByDowntime[i]->history->getLatestEntry();
                    if (h && std::abs(h->computedDrift) > 0.10) {
                        crit = assetsByDowntime[i];
                        break;
                    }
                }
                if (!crit) crit = assetsByDowntime[0];
                double dev = 0.0, drift = 0.0;
                const HistoryEntry* h = crit->history->getLatestEntry();
                if (h) { dev = h->measuredDeviation; drift = h->computedDrift; }
                double margin = std::max(0.0, 5.0 - std::abs(dev));
                double ettf = std::abs(drift) > 0.0001 ? (margin / std::abs(drift)) : 999.0;

                std::ostringstream rep;
                rep << "Predictive RUL Failure Forecast:\n"
                    << "Asset: " << crit->name << " (" << crit->assetCode << ")\n"
                    << "Current Deviation: " << dev << " mm (Tolerance: 5.0 mm)\n"
                    << "Drift Velocity: " << (drift >= 0 ? "+" : "") << drift << " mm/s\n"
                    << "Safe Margin Remaining: " << margin << " mm\n"
                    << "Estimated Time to Critical Failure: " << (ettf <= 3.0 ? "IMMINENT (< 3 days)" : (std::to_string(static_cast<int>(ettf)) + " days")) << "\n\n"
                    << "Trend Progression:\n"
                    << "[Baseline 0.0mm] ───> [Dev " << dev << "mm] ───> [BREACH 5.0mm]";
                responseText = rep.str();
                isCode = true;
            } else {
                responseText = "All laboratory equipment is operating within nominal safe margins.";
            }
        }
        // 7. Calibration Due Queries
        else if (hasWord("calib") || hasWord("calibration") || hasWord("due") || hasWord("schedule")) {
            CustomArray<CalibrationItem> upcoming;
            engine.getUpcomingCalibrations(0, 30, upcoming);
            if (!upcoming.empty()) {
                std::ostringstream rep;
                rep << "Upcoming Calibrations (via AVL Tree Range Query):\n";
                for (size_t i = 0; i < upcoming.size(); ++i) {
                    rep << "• [" << upcoming[i].assetCode << "] " << upcoming[i].assetName
                        << ": Due in " << upcoming[i].dueDate << " day(s)\n";
                }
                responseText = rep.str();
            } else {
                responseText = "No assets require calibration within the next 30 days.";
            }
        }
        // 8. Explain Drift
        else if (hasWord("drift") || hasWord("velocity") || hasWord("formula") || hasWord("interpolation")) {
            responseText = "Drift Velocity & Trend Explainability:\n"
                           "• Measurement Drift is computed in the Doubly Linked List via linear interpolation: Delta(Dev) / Delta(Time).\n"
                           "• A positive drift (+0.05 mm/s) indicates progressive mechanical wear or photodetector degradation.\n"
                           "• A negative drift (-0.50 mm/s) indicates sudden rotor wobble or structural shock.\n"
                           "• When recalibration is performed, deviation resets toward baseline, proving the effectiveness of the calibration cycle.";
        }
        // 9. Maintenance Window Safety Evaluation
        else if (hasWord("service") || hasWord("safely") || hasWord("maintenance") || hasWord("window")) {
            std::string target = "LAB-COOL-01";
            if (cleanQ.find("ups") != std::string::npos) target = "LAB-UPS-01";
            else if (cleanQ.find("net") != std::string::npos) target = "LAB-NET-01";
            else if (cleanQ.find("pc") != std::string::npos) target = "LAB-PC-17";
            else if (cleanQ.find("laser") != std::string::npos) target = "LAB-LASER-02";
            else if (cleanQ.find("cent") != std::string::npos) target = "LAB-CENT-01";
            else if (cleanQ.find("spec") != std::string::npos) target = "LAB-SPEC-03";

            Asset* targetAsset = engine.getAsset(target);
            std::string name = targetAsset ? targetAsset->name : target;

            CustomArray<std::string> affected;
            engine.getGraph().bfs(target, affected);

            CustomArray<TriageTask> tasks;
            engine.getTriageTasksSnapshot(tasks);
            int activeConflicts = 0;
            for (size_t i = 0; i < tasks.size(); ++i) {
                if (tasks[i].assetCode == target) activeConflicts++;
            }

            bool isSafe = (activeConflicts == 0 && affected.size() == 0);
            std::string verdict = isSafe ? "SAFE FOR SERVICE" : "UNSAFE: SYSTEMIC DISRUPTION";
            std::string guidance = isSafe ? 
                "Zero downstream equipment will be affected. Safe to schedule shutdown for maintenance." :
                "WARNING: Taking this asset offline halts " + std::to_string(affected.size()) + " downstream machines. Active conflicts: " + std::to_string(activeConflicts) + ".";

            std::ostringstream rep;
            rep << "Maintenance Window Evaluation for " << name << " (" << target << "):\n"
                << "Verdict: " << verdict << "\n"
                << "Impact: Taking this offline halts " << affected.size() << " downstream machines.\n"
                << "Advice: " << guidance;
            responseText = rep.str();
        }
        // 10. Clarification for Incomplete "Which" Queries
        else if (cleanQ == "which" || cleanQ == "which is" || cleanQ == "which one" || cleanQ == "what" || cleanQ == "what is") {
            responseText = "Please specify what you would like to identify:\n\n"
                           "• \"Which is most faulty?\" (Finds equipment with highest logged fault count)\n"
                           "• \"Which is least faulty?\" (Finds reliable nominal equipment with zero faults)\n"
                           "• \"Which is highest risk?\" (Finds the current Max-Heap triage emergency root)\n"
                           "• \"Which machine is a Single Point of Failure?\" (Finds high-blast bottleneck assets)\n"
                           "• \"Which supplier is the worst?\" (Finds suppliers with lowest Lemon Index scores)";
        }
        // 9. Default Fallback
        else {
            std::ostringstream rep;
            rep << "I parsed your request regarding \"" << rawQuery << "\".\n\n"
                << "Here are questions I can answer directly:\n\n"
                << "UNDER THE HOOD & DSA ARCHITECTURE:\n"
                << "• \"How does this work under the hood?\" (5-stage pipeline)\n"
                << "• \"What data structures are used?\" (All 8 custom C++ structures)\n"
                << "• \"How is risk score calculated?\" (Mathematical formula & blast multiplier)\n"
                << "• \"Explain Big-O time complexity\" (Algorithmic bounds table)\n"
                << "• \"How does Max-Heap / Hash Table / AVL Tree work?\"\n\n"
                << "LIVE FLEET DIAGNOSTICS:\n"
                << "• \"Which is most faulty?\" / \"Which is least faulty?\"\n"
                << "• \"Which is highest risk?\" (Max-Heap triage root)\n"
                << "• \"Which machine is a Single Point of Failure?\" (Graph BFS SPOF)\n"
                << "• \"Which supplier is the worst (Lemon Index)?\"\n"
                << "• \"Predict next failure\" (Remaining Useful Life Forecast)\n"
                << "• \"Dispatch top job\" (Max-Heap dispatch action)";
            responseText = rep.str();
        }

        std::ostringstream outJson;
        outJson << "{\"status\":\"success\",\"reply\":\"" << escapeJson(responseText) << "\",\"isCode\":" << (isCode ? "true" : "false") << "}";
        res.set_content(outJson.str(), "application/json");
    });

    // =========================================================================
    // GET /api/diagnostics/hash-table — Collision & Occupancy Statistics
    // =========================================================================
    server.Get("/api/diagnostics/hash-table", [this](const httplib_single::Request&, httplib_single::Response& res) {
        HashStats stats = engine.getRegistryStats();
        std::ostringstream ss;
        ss << "{"
           << "\"totalElements\":" << stats.totalElements << ","
           << "\"bucketCount\":" << stats.bucketCount << ","
           << "\"occupiedBuckets\":" << stats.occupiedBuckets << ","
           << "\"totalCollisions\":" << stats.totalCollisions << ","
           << "\"maxChainLength\":" << stats.maxChainLength << ","
           << "\"averageChainLength\":" << stats.averageChainLength << ","
           << "\"loadFactor\":" << stats.loadFactor
           << "}";
        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/blast-radius-demo — Standalone Graph BFS/DFS demo
    // =========================================================================
    server.Get("/api/blast-radius-demo", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string asset = req.get_param_value("asset");
        if (asset.empty()) asset = "LAB-UPS-01";

        CustomArray<std::string> bfsAffected;
        CustomArray<std::string> dfsAffected;

        engine.getGraph().bfs(asset, bfsAffected);
        engine.getGraph().dfs(asset, dfsAffected);

        std::ostringstream ss;
        ss << "{"
           << "\"sourceAsset\":\"" << asset << "\","
           << "\"bfsCount\":" << bfsAffected.size() << ","
           << "\"bfsAffected\":[";
        for (size_t i = 0; i < bfsAffected.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "\"" << bfsAffected[i] << "\"";
        }
        ss << "],\"dfsAffected\":[";
        for (size_t i = 0; i < dfsAffected.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "\"" << dfsAffected[i] << "\"";
        }
        ss << "]}";

        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/compliance-sort-demo — Standalone MergeSort demo
    // =========================================================================
    server.Get("/api/compliance-sort-demo", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string criterionStr = req.get_param_value("criterion");
        SortCriterion crit = SORT_BY_DOWNTIME_DESC;
        if (criterionStr == "faults") crit = SORT_BY_FAULT_COUNT_DESC;
        else if (criterionStr == "calib") crit = SORT_BY_CALIBRATION_DUE_ASC;

        CustomArray<Asset*> list;
        engine.getAllAssets(list);
        MergeSort::sort(list, crit);

        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{"
               << "\"assetCode\":\"" << list[i]->assetCode << "\","
               << "\"name\":\"" << list[i]->name << "\","
               << "\"totalDowntimeHours\":" << list[i]->totalDowntimeHours << ","
               << "\"totalFaultCount\":" << list[i]->totalFaultCount << ","
               << "\"nextCalibrationDue\":" << list[i]->nextCalibrationDue
               << "}";
        }
        ss << "]";
        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/graph — Complete dependency topology & source analysis
    // =========================================================================
    server.Get("/api/graph", [this](const httplib_single::Request&, httplib_single::Response& res) {
        CustomArray<std::string> vertexCodes;
        CustomArray<GraphEdgeInfo> edges;
        CustomArray<SourceAnalysisInfo> sources;

        engine.getGraph().getAllVertexCodes(vertexCodes);
        engine.getGraph().getAllEdges(edges);
        engine.getGraph().analyzeAllSources(sources);

        std::ostringstream ss;
        ss << "{\"nodes\":[";
        for (size_t i = 0; i < vertexCodes.size(); ++i) {
            if (i > 0) ss << ",";
            Asset* a = engine.getAsset(vertexCodes[i]);
            std::string name = a ? a->name : vertexCodes[i];
            std::string loc = a ? a->location : "Main Lab";
            int faults = a ? a->totalFaultCount : 0;
            double dt = a ? a->totalDowntimeHours : 0.0;
            int blast = engine.getGraph().getBlastRadiusCount(vertexCodes[i]);

            ss << "{"
               << "\"id\":\"" << vertexCodes[i] << "\","
               << "\"name\":\"" << name << "\","
               << "\"location\":\"" << loc << "\","
               << "\"faults\":" << faults << ","
               << "\"downtime\":" << dt << ","
               << "\"blastRadius\":" << blast
               << "}";
        }
        ss << "],\"edges\":[";
        for (size_t i = 0; i < edges.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{"
               << "\"from\":\"" << edges[i].from << "\","
               << "\"to\":\"" << edges[i].to << "\","
               << "\"weight\":" << edges[i].weight
               << "}";
        }
        ss << "],\"sources\":[";
        for (size_t i = 0; i < sources.size(); ++i) {
            if (i > 0) ss << ",";
            Asset* a = engine.getAsset(sources[i].assetCode);
            std::string name = a ? a->name : sources[i].assetCode;
            ss << "{"
               << "\"assetCode\":\"" << sources[i].assetCode << "\","
               << "\"name\":\"" << name << "\","
               << "\"outDegree\":" << sources[i].outDegree << ","
               << "\"inDegree\":" << sources[i].inDegree << ","
               << "\"totalBlastRadius\":" << sources[i].totalBlastRadius << ","
               << "\"criticality\":\"" << sources[i].criticality << "\""
               << "}";
        }
        ss << "]}";

        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/reports/faulty — Faulty Equipment Audit (Merge Sort Powered)
    // =========================================================================
    server.Get("/api/reports/faulty", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string sortBy = req.get_param_value("sort");
        SortCriterion crit = SORT_BY_DOWNTIME_DESC;
        if (sortBy == "faults") crit = SORT_BY_FAULT_COUNT_DESC;
        else if (sortBy == "calib") crit = SORT_BY_CALIBRATION_DUE_ASC;

        CustomArray<Asset*> faultyAssets;
        engine.getFaultyEquipmentReport(crit, faultyAssets);

        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < faultyAssets.size(); ++i) {
            if (i > 0) ss << ",";
            double latestDrift = 0.0;
            double latestDev = 0.0;
            const HistoryEntry* latest = faultyAssets[i]->history->getLatestEntry();
            if (latest != nullptr) {
                latestDrift = latest->computedDrift;
                latestDev = latest->measuredDeviation;
            }
            int blast = engine.getGraph().getBlastRadiusCount(faultyAssets[i]->assetCode);

            double safeTolerance = 5.0; // Critical tolerance limit (mm/units)
            double remainingMargin = std::max(0.0, safeTolerance - std::abs(latestDev));
            double ettfDays = 999.0;
            if (std::abs(latestDrift) > 0.0001) {
                ettfDays = remainingMargin / std::abs(latestDrift);
            }

            std::string failureUrgency = "NOMINAL";
            if (std::abs(latestDev) >= safeTolerance || ettfDays <= 3.0) {
                failureUrgency = "CRITICAL IMMINENT";
            } else if (ettfDays <= 14.0 || std::abs(latestDrift) >= 0.10) {
                failureUrgency = "ACCELERATING DRIFT";
            }

            ss << "{"
               << "\"assetCode\":\"" << faultyAssets[i]->assetCode << "\","
               << "\"name\":\"" << faultyAssets[i]->name << "\","
               << "\"location\":\"" << faultyAssets[i]->location << "\","
               << "\"supplier\":\"" << faultyAssets[i]->supplier << "\","
               << "\"totalFaultCount\":" << faultyAssets[i]->totalFaultCount << ","
               << "\"totalDowntimeHours\":" << faultyAssets[i]->totalDowntimeHours << ","
               << "\"latestDeviation\":" << latestDev << ","
               << "\"latestDriftVelocity\":" << latestDrift << ","
               << "\"blastRadiusCount\":" << blast << ","
               << "\"remainingMargin\":" << remainingMargin << ","
               << "\"ettfDays\":" << ettfDays << ","
               << "\"failureUrgency\":\"" << failureUrgency << "\","
               << "\"nextCalibrationDue\":" << faultyAssets[i]->nextCalibrationDue
               << "}";
        }
        ss << "]";

        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/reports/suppliers — Supplier Reliability Scorecard & Lemon Index
    // =========================================================================
    server.Get("/api/reports/suppliers", [this](const httplib_single::Request&, httplib_single::Response& res) {
        CustomArray<Asset*> allAssets;
        engine.getAllAssets(allAssets);

        struct SupplierMetrics {
            std::string name;
            int assetCount;
            int totalFaults;
            double totalDowntime;
            double totalDrift;
        };

        CustomArray<SupplierMetrics> suppliers;

        for (size_t i = 0; i < allAssets.size(); ++i) {
            std::string sName = allAssets[i]->supplier.empty() ? "Standard Lab Corp" : allAssets[i]->supplier;
            int foundIdx = -1;
            for (size_t s = 0; s < suppliers.size(); ++s) {
                if (suppliers[s].name == sName) {
                    foundIdx = static_cast<int>(s);
                    break;
                }
            }

            double drift = 0.0;
            const HistoryEntry* h = allAssets[i]->history->getLatestEntry();
            if (h) drift = std::abs(h->computedDrift);

            if (foundIdx >= 0) {
                suppliers[foundIdx].assetCount++;
                suppliers[foundIdx].totalFaults += allAssets[i]->totalFaultCount;
                suppliers[foundIdx].totalDowntime += allAssets[i]->totalDowntimeHours;
                suppliers[foundIdx].totalDrift += drift;
            } else {
                SupplierMetrics sm;
                sm.name = sName;
                sm.assetCount = 1;
                sm.totalFaults = allAssets[i]->totalFaultCount;
                sm.totalDowntime = allAssets[i]->totalDowntimeHours;
                sm.totalDrift = drift;
                suppliers.push_back(sm);
            }
        }

        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < suppliers.size(); ++i) {
            if (i > 0) ss << ",";
            double avgFaults = static_cast<double>(suppliers[i].totalFaults) / suppliers[i].assetCount;
            double avgDowntime = suppliers[i].totalDowntime / suppliers[i].assetCount;
            double avgDrift = suppliers[i].totalDrift / suppliers[i].assetCount;

            // Score = 100 - (15*AvgFaults + 20*AvgDrift + 2*AvgDowntime)
            double penalty = (15.0 * avgFaults) + (20.0 * avgDrift) + (2.0 * avgDowntime);
            double score = 100.0 - penalty;
            if (score < 0.0) score = 0.0;
            if (score > 100.0) score = 100.0;

            std::string tier = "Tier A (Certified / High Reliability)";
            std::string rec = "Certified manufacturer. Low drift and failure rate; approved for future procurement.";
            bool isLemon = false;

            if (score < 60.0 || avgFaults >= 1.5) {
                tier = "Tier C (Defective / 'Lemon')";
                rec = "CRITICAL: Excessive failure rate detected. Recommendation: File immediate warranty claim and pause procurement.";
                isLemon = true;
            } else if (score < 85.0) {
                tier = "Tier B (Watchlist)";
                rec = "MODERATE: Elevated drift velocity observed. Increase calibration inspection frequency.";
            }

            ss << "{"
               << "\"supplierName\":\"" << suppliers[i].name << "\","
               << "\"assetCount\":" << suppliers[i].assetCount << ","
               << "\"totalFaults\":" << suppliers[i].totalFaults << ","
               << "\"totalDowntime\":" << suppliers[i].totalDowntime << ","
               << "\"avgDrift\":" << avgDrift << ","
               << "\"score\":" << score << ","
               << "\"tier\":\"" << tier << "\","
               << "\"isLemon\":" << (isLemon ? "true" : "false") << ","
               << "\"recommendation\":\"" << rec << "\""
               << "}";
        }
        ss << "]";

        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/what-if/service — Maintenance Window Safety Inspector
    // =========================================================================
    server.Get("/api/what-if/service", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string target = req.get_param_value("asset");
        if (target.empty()) target = "LAB-COOL-01";

        Asset* targetAsset = engine.getAsset(target);
        if (!targetAsset) {
            res.status = 404;
            res.set_content("{\"status\":\"error\",\"message\":\"Asset not found\"}", "application/json");
            return;
        }

        CustomArray<std::string> affected;
        engine.getGraph().bfs(target, affected);

        CustomArray<TriageTask> triageTasks;
        engine.getTriageTasksSnapshot(triageTasks);

        std::ostringstream ss;
        ss << "{"
           << "\"assetCode\":\"" << target << "\","
           << "\"assetName\":\"" << targetAsset->name << "\","
           << "\"location\":\"" << targetAsset->location << "\","
           << "\"dependentCount\":" << affected.size() << ","
           << "\"affectedAssets\":[";
        for (size_t i = 0; i < affected.size(); ++i) {
            if (i > 0) ss << ",";
            Asset* a = engine.getAsset(affected[i]);
            std::string name = a ? a->name : affected[i];
            ss << "{\"code\":\"" << affected[i] << "\",\"name\":\"" << name << "\"}";
        }
        ss << "],";

        int activeConflicts = 0;
        std::string conflictSummary = "";
        for (size_t i = 0; i < triageTasks.size(); ++i) {
            if (triageTasks[i].assetCode == target) {
                activeConflicts++;
                conflictSummary += "Target " + target + " has active triage ticket (" + triageTasks[i].taskId + "). ";
            }
            for (size_t j = 0; j < affected.size(); ++j) {
                if (triageTasks[i].assetCode == affected[j]) {
                    activeConflicts++;
                    conflictSummary += "Dependent " + affected[j] + " has active triage ticket (" + triageTasks[i].taskId + "). ";
                }
            }
        }

        bool isSafe = (activeConflicts == 0 && affected.size() == 0);
        std::string verdict = isSafe ? "SAFE FOR SERVICE" : "UNSAFE: SYSTEMIC DISRUPTION";
        std::string guidance = isSafe ? 
            "Zero downstream equipment will be affected. Safe to schedule shutdown for maintenance." :
            "WARNING: Taking this asset offline halts " + std::to_string(affected.size()) + " downstream machines. Active conflicts: " + std::to_string(activeConflicts) + ".";

        ss << "\"activeConflicts\":" << activeConflicts << ","
           << "\"isSafe\":" << (isSafe ? "true" : "false") << ","
           << "\"verdict\":\"" << verdict << "\","
           << "\"conflictSummary\":\"" << conflictSummary << "\","
           << "\"guidance\":\"" << guidance << "\""
           << "}";

        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/reports/sources — Critical Dependency Sources & SPOF Audit
    // =========================================================================
    server.Get("/api/reports/sources", [this](const httplib_single::Request&, httplib_single::Response& res) {
        CustomArray<SourceAnalysisInfo> sources;
        engine.getSourceEquipmentReport(sources);

        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < sources.size(); ++i) {
            if (i > 0) ss << ",";
            Asset* a = engine.getAsset(sources[i].assetCode);
            std::string name = a ? a->name : sources[i].assetCode;
            std::string loc = a ? a->location : "Main Lab";

            ss << "{"
               << "\"assetCode\":\"" << sources[i].assetCode << "\","
               << "\"name\":\"" << name << "\","
               << "\"location\":\"" << loc << "\","
               << "\"outDegree\":" << sources[i].outDegree << ","
               << "\"inDegree\":" << sources[i].inDegree << ","
               << "\"totalBlastRadius\":" << sources[i].totalBlastRadius << ","
               << "\"criticality\":\"" << sources[i].criticality << "\""
               << "}";
        }
        ss << "]";

        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/reports/calibrations — AVL Tree Range Calibration Audit
    // =========================================================================
    server.Get("/api/reports/calibrations", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        long long startDue = 0;
        long long endDue = 9999999999LL;

        std::string startParam = req.get_param_value("start");
        std::string endParam = req.get_param_value("end");
        if (!startParam.empty()) startDue = std::stoll(startParam);
        if (!endParam.empty()) endDue = std::stoll(endParam);

        CustomArray<CalibrationItem> items;
        engine.getUpcomingCalibrations(startDue, endDue, items);

        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) ss << ",";
            Asset* a = engine.getAsset(items[i].assetCode);
            std::string loc = a ? a->location : "-";
            ss << "{"
               << "\"dueDate\":" << items[i].dueDate << ","
               << "\"assetCode\":\"" << items[i].assetCode << "\","
               << "\"assetName\":\"" << items[i].assetName << "\","
               << "\"location\":\"" << loc << "\""
               << "}";
        }
        ss << "]";

        res.set_content(ss.str(), "application/json");
    });

    // =========================================================================
    // GET /api/reports/export-csv — Downloadable RFC 4180 CSV Export
    // =========================================================================
    server.Get("/api/reports/export-csv", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string type = req.get_param_value("type");
        std::ostringstream csv;

        if (type == "sources") {
            csv << "Asset Code,Equipment Name,Location,Direct Dependents (Out-Degree),Upstream Inputs (In-Degree),Total Blast Radius,Criticality Status\r\n";
            CustomArray<SourceAnalysisInfo> sources;
            engine.getSourceEquipmentReport(sources);
            for (size_t i = 0; i < sources.size(); ++i) {
                Asset* a = engine.getAsset(sources[i].assetCode);
                std::string name = a ? a->name : sources[i].assetCode;
                std::string loc = a ? a->location : "Main Lab";
                csv << "\"" << sources[i].assetCode << "\",\""
                    << name << "\",\""
                    << loc << "\","
                    << sources[i].outDegree << ","
                    << sources[i].inDegree << ","
                    << sources[i].totalBlastRadius << ",\""
                    << sources[i].criticality << "\"\r\n";
            }
        } else if (type == "calibrations") {
            csv << "Calibration Due Date (Days/Epoch),Asset Code,Equipment Name,Lab Location\r\n";
            CustomArray<CalibrationItem> items;
            engine.getUpcomingCalibrations(0, 9999999999LL, items);
            for (size_t i = 0; i < items.size(); ++i) {
                Asset* a = engine.getAsset(items[i].assetCode);
                std::string loc = a ? a->location : "-";
                csv << items[i].dueDate << ",\""
                    << items[i].assetCode << "\",\""
                    << items[i].assetName << "\",\""
                    << loc << "\"\r\n";
            }
        } else if (type == "suppliers") {
            csv << "Supplier Name,Asset Count,Total Faults,Total Downtime Hours,Average Drift Velocity,Reliability Score (0-100),Quality Tier,Warranty Action Recommendation\r\n";
            CustomArray<Asset*> allAssets;
            engine.getAllAssets(allAssets);

            struct SupplierMetrics {
                std::string name;
                int assetCount;
                int totalFaults;
                double totalDowntime;
                double totalDrift;
            };

            CustomArray<SupplierMetrics> suppliers;
            for (size_t i = 0; i < allAssets.size(); ++i) {
                std::string sName = allAssets[i]->supplier.empty() ? "Standard Lab Corp" : allAssets[i]->supplier;
                int foundIdx = -1;
                for (size_t s = 0; s < suppliers.size(); ++s) {
                    if (suppliers[s].name == sName) {
                        foundIdx = static_cast<int>(s);
                        break;
                    }
                }
                double drift = 0.0;
                const HistoryEntry* h = allAssets[i]->history->getLatestEntry();
                if (h) drift = std::abs(h->computedDrift);

                if (foundIdx >= 0) {
                    suppliers[foundIdx].assetCount++;
                    suppliers[foundIdx].totalFaults += allAssets[i]->totalFaultCount;
                    suppliers[foundIdx].totalDowntime += allAssets[i]->totalDowntimeHours;
                    suppliers[foundIdx].totalDrift += drift;
                } else {
                    SupplierMetrics sm;
                    sm.name = sName;
                    sm.assetCount = 1;
                    sm.totalFaults = allAssets[i]->totalFaultCount;
                    sm.totalDowntime = allAssets[i]->totalDowntimeHours;
                    sm.totalDrift = drift;
                    suppliers.push_back(sm);
                }
            }

            for (size_t i = 0; i < suppliers.size(); ++i) {
                double avgFaults = static_cast<double>(suppliers[i].totalFaults) / suppliers[i].assetCount;
                double avgDowntime = suppliers[i].totalDowntime / suppliers[i].assetCount;
                double avgDrift = suppliers[i].totalDrift / suppliers[i].assetCount;
                double score = 100.0 - (15.0 * avgFaults + 20.0 * avgDrift + 2.0 * avgDowntime);
                if (score < 0.0) score = 0.0;
                if (score > 100.0) score = 100.0;

                std::string tier = "Tier A (Certified)";
                std::string rec = "Certified";
                if (score < 60.0 || avgFaults >= 1.5) {
                    tier = "Tier C (Defective / Lemon)";
                    rec = "File immediate warranty claim";
                } else if (score < 85.0) {
                    tier = "Tier B (Watchlist)";
                    rec = "Monitor sensor drift";
                }

                csv << "\"" << suppliers[i].name << "\","
                    << suppliers[i].assetCount << ","
                    << suppliers[i].totalFaults << ","
                    << suppliers[i].totalDowntime << ","
                    << avgDrift << ","
                    << score << ",\""
                    << tier << "\",\""
                    << rec << "\"\r\n";
            }
        } else if (type == "all-assets" || type == "assets") {
            csv << "Asset Code,Equipment Name,Supplier,Location,Total Faults,Total Downtime (Hours),Next Calibration Due\r\n";
            CustomArray<Asset*> all;
            engine.getAllAssets(all);
            for (size_t i = 0; i < all.size(); ++i) {
                csv << "\"" << all[i]->assetCode << "\",\""
                    << all[i]->name << "\",\""
                    << (all[i]->supplier.empty() ? "Standard Lab Corp" : all[i]->supplier) << "\",\""
                    << all[i]->location << "\","
                    << all[i]->totalFaultCount << ","
                    << all[i]->totalDowntimeHours << ","
                    << all[i]->nextCalibrationDue << "\r\n";
            }
        } else {
            // Default: Faulty equipment report
            csv << "Asset Code,Equipment Name,Location,Total Faults,Total Downtime (Hours),Latest Deviation,Latest Drift Velocity,Blast Radius (Affected Machines),Next Calibration Due\r\n";
            CustomArray<Asset*> list;
            engine.getFaultyEquipmentReport(SORT_BY_DOWNTIME_DESC, list);
            for (size_t i = 0; i < list.size(); ++i) {
                double drift = 0.0, dev = 0.0;
                const HistoryEntry* h = list[i]->history->getLatestEntry();
                if (h) { drift = h->computedDrift; dev = h->measuredDeviation; }
                int blast = engine.getGraph().getBlastRadiusCount(list[i]->assetCode);

                csv << "\"" << list[i]->assetCode << "\",\""
                    << list[i]->name << "\",\""
                    << list[i]->location << "\","
                    << list[i]->totalFaultCount << ","
                    << list[i]->totalDowntimeHours << ","
                    << dev << ","
                    << drift << ","
                    << blast << ","
                    << list[i]->nextCalibrationDue << "\r\n";
            }
        }

        res.set_content(csv.str(), "text/csv");
    });

    // =========================================================================
    // POST /api/simulate-cascade — Cascade failure simulation on root node
    // =========================================================================
    server.Post("/api/simulate-cascade", [this](const httplib_single::Request& req, httplib_single::Response& res) {
        std::string asset = extractJsonString(req.body, "assetCode");
        if (asset.empty()) asset = "LAB-UPS-01";

        TriageTask topTask;
        CustomArray<std::string> affected;
        bool ok = engine.simulateFailureCascade(asset, topTask, affected);

        if (ok) {
            std::ostringstream ss;
            ss << "{\"status\":\"success\","
               << "\"simulatedAsset\":\"" << asset << "\","
               << "\"taskId\":\"" << topTask.taskId << "\","
               << "\"riskScore\":" << topTask.riskScore << ","
               << "\"blastMultiplier\":" << topTask.blastMultiplier << ","
               << "\"blastRadiusCount\":" << topTask.blastRadiusCount << ","
               << "\"affectedDependents\":[";
            for (size_t i = 0; i < affected.size(); ++i) {
                if (i > 0) ss << ",";
                ss << "\"" << affected[i] << "\"";
            }
            ss << "]}";
            res.set_content(ss.str(), "application/json");
        } else {
            res.status = 404;
            res.set_content("{\"status\":\"error\",\"message\":\"Asset not found for simulation.\"}", "application/json");
        }
    });

    server.listen("0.0.0.0", port);
}
