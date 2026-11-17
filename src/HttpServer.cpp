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
        if (tokens.empty() || (tokens.size() <= 2 && (hasWord("which") || hasWord("what") || hasWord("who") || hasWord("help") || hasWord("show")))) {
            responseText = "Please specify what you would like to know about the laboratory:\n"
                           "• \"Which is most faulty?\" — Equipment with the highest logged faults\n"
                           "• \"Which is least faulty?\" — Cleanest equipment with zero/fewest faults\n"
                           "• \"Which has the highest downtime?\" — Downtime leader (via Merge Sort)\n"
                           "• \"Which is highest risk?\" — Top urgency task in Max-Heap triage\n"
                           "• \"Which is a Single Point of Failure?\" — Graph BFS blast radius analysis\n"
                           "• \"Which supplier is the worst (Lemon)?\" — Vendor quality scorecard\n"
                           "• \"Which asset will fail next?\" — Predictive RUL failure forecast";
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
        else if (hasSub("lab-") || hasWord("ups") || hasWord("switch") || hasWord("centrifuge") ||
                 hasWord("spectrometer") || hasWord("chiller") || hasWord("laser") || hasWord("workstation")) {
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
        // 9. Default Fallback
        else {
            std::ostringstream rep;
            rep << "I parsed your request regarding \"" << rawQuery << "\".\n\n"
                << "Here are specific questions I can answer directly:\n"
                << "• \"Which is most faulty?\" (Fault frequency leader via Merge Sort)\n"
                << "• \"Which is least faulty?\" (Clean equipment with 0 faults)\n"
                << "• \"Which is highest risk?\" (Max-Heap triage root)\n"
                << "• \"Which machine is a Single Point of Failure?\" (Graph BFS SPOF)\n"
                << "• \"Tell me about LAB-UPS-01\" (O(1) Hash Table asset lookup)\n"
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

    server.listen("127.0.0.1", port);
}
