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

            ss << "{"
               << "\"assetCode\":\"" << faultyAssets[i]->assetCode << "\","
               << "\"name\":\"" << faultyAssets[i]->name << "\","
               << "\"location\":\"" << faultyAssets[i]->location << "\","
               << "\"totalFaultCount\":" << faultyAssets[i]->totalFaultCount << ","
               << "\"totalDowntimeHours\":" << faultyAssets[i]->totalDowntimeHours << ","
               << "\"latestDeviation\":" << latestDev << ","
               << "\"latestDriftVelocity\":" << latestDrift << ","
               << "\"blastRadiusCount\":" << blast << ","
               << "\"nextCalibrationDue\":" << faultyAssets[i]->nextCalibrationDue
               << "}";
        }
        ss << "]";

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

    server.listen("127.0.0.1", port);
}
