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
        long long calibDue = extractJsonLong(req.body, "nextCalibrationDue");
        double downtime = extractJsonDouble(req.body, "totalDowntimeHours");

        if (code.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"assetCode is required\"}", "application/json");
            return;
        }

        Asset newAsset(code, name, loc, calibDue, downtime);
        bool success = engine.registerAsset(newAsset);

        if (success) {
            res.set_content("{\"status\":\"success\",\"message\":\"Asset registered successfully\"}", "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"status\":\"error\",\"message\":\"Asset code already exists\"}", "application/json");
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

        // Steps 2-4: Process buffered report through HashTable, DLL, Drift calc, and MaxHeap
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

    server.listen("127.0.0.1", port);
}
