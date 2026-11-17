import subprocess
import time
import urllib.request
import urllib.error
import json
import sys

def run_tests():
    print("==================================================================")
    print("  COMPREHENSIVE END-TO-END (E2E) TEST SUITE")
    print("  Testing All Endpoints, Forms, Buttons, Filters, & Edge Cases")
    print("==================================================================")

    # 1. Start server process on test port 8088
    server_proc = subprocess.Popen(['./lab_engine.exe', '--server', '8088'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.5)

    passed = 0
    failed = 0
    findings = []

    def log_test(name, success, detail=""):
        nonlocal passed, failed
        if success:
            passed += 1
            print(f"[PASS] {name} {detail}", flush=True)
        else:
            failed += 1
            print(f"[FAIL] {name} - {detail}", flush=True)
            findings.append(f"{name}: {detail}")

    def get(url):
        req = urllib.request.Request(f"http://localhost:8088{url}")
        with urllib.request.urlopen(req) as res:
            return res.getcode(), res.read().decode('utf-8')

    def post(url, payload):
        data = json.dumps(payload).encode('utf-8')
        req = urllib.request.Request(f"http://localhost:8088{url}", data=data, headers={'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(req) as res:
                return res.getcode(), json.loads(res.read().decode('utf-8'))
        except urllib.error.HTTPError as e:
            return e.code, json.loads(e.read().decode('utf-8'))

    try:
        # TEST GROUP 1: Static Assets & Web Delivery
        code, html = get("/")
        log_test("Static HTML Serving", code == 200 and "<!DOCTYPE html>" in html, f"Length: {len(html)} bytes")

        code, css = get("/styles.css")
        log_test("Static CSS Serving", code == 200 and ".node-bubble" in css, f"Length: {len(css)} bytes")

        code, js = get("/app.js")
        log_test("Static JS Serving", code == 200 and "drawTopologyGraph" in js, f"Length: {len(js)} bytes")

        # TEST GROUP 2: Fleet Assets & Registration Edge Cases
        code, assets_raw = get("/api/assets")
        assets = json.loads(assets_raw)
        log_test("Initial Asset Registry (Hash Table)", code == 200 and len(assets) >= 7, f"{len(assets)} assets loaded")

        # 2.1 Valid asset with dependency
        code, res = post("/api/assets", {
            "assetCode": "LAB-HPLC-099",
            "name": "High Pressure Chromatograph",
            "location": "Room 401",
            "supplier": "Agilent",
            "nextCalibrationDue": 25,
            "totalDowntimeHours": 4.5,
            "dependsOn": "LAB-UPS-01"
        })
        log_test("Register Equipment with Upstream Dependency", code == 200 and res.get("status") == "success")

        # 2.2 Valid asset without dependency (Standalone Root)
        code, res = post("/api/assets", {
            "assetCode": "LAB-SCALE-001",
            "name": "Precision Analytical Balance",
            "location": "Room 102",
            "supplier": "Mettler Toledo",
            "nextCalibrationDue": 14,
            "totalDowntimeHours": 0.0,
            "dependsOn": ""
        })
        log_test("Register Standalone Equipment (No Upstream)", code == 200 and res.get("status") == "success")

        # 2.3 Edge Case: Duplicate Asset Code
        code, res = post("/api/assets", {
            "assetCode": "LAB-UPS-01",
            "name": "Duplicate UPS",
            "location": "Room 101"
        })
        log_test("Reject Duplicate Asset Code (Hash Collision Invariant)", code == 400 and "already exists" in res.get("message", ""))

        # 2.4 Edge Case: Missing Asset Code
        code, res = post("/api/assets", {
            "assetCode": "",
            "name": "Ghost Equipment"
        })
        log_test("Reject Empty Asset Code", code == 400 and "assetCode is required" in res.get("error", ""))

        # TEST GROUP 3: Circular Queue & Emergency Triage Pipeline
        code, triage_raw = get("/api/triage")
        triage = json.loads(triage_raw)
        log_test("Triage Queue Snapshot (Max-Heap)", code == 200 and len(triage) >= 1, f"{len(triage)} active jobs")

        # 3.1 Submit new emergency report
        code, res = post("/api/reports", {
            "reportId": "RPT-TEST-01",
            "assetCode": "LAB-HPLC-099",
            "severity": 4,
            "daysOverdue": 7,
            "measuredDeviation": 3.8,
            "timestamp": 1700000000,
            "description": "High pressure pump cavitation warning"
        })
        log_test("Submit Emergency Report (Circular Queue -> Max-Heap)", code == 200 and res.get("status") == "success")

        # 3.2 Dispatch Top Emergency Job (Extract Max)
        code, res = post("/api/dispatch", {})
        dispatched_id = res.get("dispatched", {}).get("taskId", "")
        log_test("Dispatch Top Emergency (O(log N) Extract Max)", code == 200 and bool(dispatched_id), f"Dispatched Task: {dispatched_id}")

        # 3.3 Undo Dispatch (LIFO Stack Rollback)
        code, res = post("/api/undo", {})
        log_test("Undo Action Rollback (O(1) Undo Stack)", code == 200 and res.get("status") == "success", res.get("message", ""))

        # 3.4 Simulate Catastrophic Root Cascade
        code, res = post("/api/simulate-cascade", {"assetCode": "LAB-UPS-01"})
        log_test("Simulate Root Outage (Graph BFS Cascade)", code == 200 and res.get("blastRadiusCount", 0) >= 5, f"Affected: {res.get('blastRadiusCount')} machines")

        # 3.5 Rollback Cascade Simulation
        code, res = post("/api/undo", {})
        log_test("Rollback Root Outage Cascade", code == 200 and res.get("status") == "success")

        # TEST GROUP 4: Topology Graph & Dependency Edge Additions
        code, graph_raw = get("/api/graph")
        graph = json.loads(graph_raw)
        nodes = graph.get("nodes", [])
        edges = graph.get("edges", [])
        sources = graph.get("sources", [])
        log_test("Fetch Dependency Topology Graph", code == 200 and len(nodes) >= 9 and len(edges) >= 6, f"Nodes: {len(nodes)}, Edges: {len(edges)}, Sources: {len(sources)}")

        # 4.1 Add New Dependency Connection (POST /api/graph/edge)
        code, res = post("/api/graph/edge", {
            "from": "LAB-COOL-01",
            "to": "LAB-CENT-009"
        })
        log_test("Add Dependency Connection (Directed Graph Insert)", code == 200 and res.get("status") == "success")

        # 4.1b Verify Edge Count Incremented
        code, graph_raw2 = get("/api/graph")
        edges2 = json.loads(graph_raw2).get("edges", [])
        log_test("Verify Topology Graph Edge Incremented", len(edges2) >= 7, f"Total Edges: {len(edges2)}")

        # 4.2 Edge Case: Self-Loop Dependency
        code, res = post("/api/graph/edge", {
            "from": "LAB-UPS-01",
            "to": "LAB-UPS-01"
        })
        log_test("Reject Self-Loop Dependency (A -> A)", code == 400 and "Self-loop" in res.get("message", ""))

        # 4.3 Edge Case: Empty Dependency Parameters
        code, res = post("/api/graph/edge", {"from": "", "to": "LAB-UPS-01"})
        log_test("Reject Empty Dependency Endpoint", code == 400 and "required" in res.get("message", ""))

        # 4.4 Blast Radius BFS demo
        code, res_raw = get("/api/blast-radius-demo?asset=LAB-UPS-01")
        res = json.loads(res_raw)
        log_test("Blast Radius Calculation (BFS Root)", code == 200 and res.get("bfsCount", 0) >= 5)

        # 4.5 What-If Safety Advisor: Unsafe Shutdown (Root)
        code, res_raw = get("/api/what-if/service?asset=LAB-UPS-01")
        res = json.loads(res_raw)
        log_test("What-If Safety Advisor: Unsafe Asset", code == 200 and res.get("isSafe") == False, f"Verdict: {res.get('verdict')}")

        # 4.6 What-If Safety Advisor: Safe Shutdown (Leaf)
        code, res_raw = get("/api/what-if/service?asset=LAB-SCALE-001")
        res = json.loads(res_raw)
        log_test("What-If Safety Advisor: Safe Leaf Asset", code == 200 and res.get("isSafe") == True, f"Verdict: {res.get('verdict')}")

        # TEST GROUP 5: Reports, Quality Audits & CSV Exports
        code, faulty_dt = get("/api/reports/faulty?sort=downtime")
        log_test("Audit Report: Sort by Downtime (Merge Sort)", code == 200 and len(json.loads(faulty_dt)) >= 7)

        code, faulty_flt = get("/api/reports/faulty?sort=faults")
        log_test("Audit Report: Sort by Faults (Merge Sort)", code == 200 and len(json.loads(faulty_flt)) >= 7)

        code, suppliers = get("/api/reports/suppliers")
        log_test("Supplier Quality Report (Lemon Index)", code == 200 and len(json.loads(suppliers)) >= 3)

        code, calib_30 = get("/api/reports/calibrations?start=0&end=30")
        log_test("Calibration Schedule [0-30d] (AVL Tree Range Query)", code == 200 and len(json.loads(calib_30)) >= 3)

        code, sources_rpt = get("/api/reports/sources")
        sources_data = json.loads(sources_rpt)
        log_test("Source Criticality & SPOF Report", code == 200 and isinstance(sources_data, list) and len(sources_data) >= 1)

        # 5.1 CSV Export Endpoints
        for csv_type in ['faulty', 'suppliers', 'calibrations', 'all-assets']:
            code, csv_data = get(f"/api/reports/export-csv?type={csv_type}")
            log_test(f"Export CSV ({csv_type})", code == 200 and "\n" in csv_data, f"{len(csv_data)} bytes")

        # TEST GROUP 6: Chatbot Intelligence & Edge Cases
        test_queries = [
            ("How does this work under the hood?", "Circular Queue"),
            ("How to add new equipment to the graph?", "Connect Dependency Link"),
            ("How to reset graph interaction?", "Reset Map View"),
            ("What data structures are used?", "Zero STL"),
            ("How is risk score calculated?", "Effective Risk Score"),
            ("Explain Big-O time complexity", "Big-O Analysis"),
            ("Which is most faulty?", "Fault Frequency Leader"),
            ("Which is least faulty?", "Nominal Clean Fleet"),
            ("Which is highest risk?", "Max-Heap Root"),
            ("Which machine is a Single Point of Failure?", "Critical Single Points of Failure"),
            ("Predict next failure", "Predictive RUL"),
            ("Can I safely service LAB-COOL-01?", "Maintenance Window Evaluation"),
            ("Tell me about LAB-UPS-01", "Equipment Diagnostic Report"),
            ("Explain drift velocity", "Drift Velocity"),
            ("which", "Please specify"),
            ("flibbertigibbet gibberish 12345", "I parsed your request regarding")
        ]

        for q, expected in test_queries:
            code, res = post("/api/chat", {"query": q})
            reply = res.get("reply", "")
            log_test(f"Chatbot: \"{q}\"", code == 200 and expected in reply)

        # 6.1 Edge Case: Empty Chat Query
        code, res = post("/api/chat", {"query": ""})
        log_test("Chatbot Edge Case: Empty Query", code == 400 and res.get("status") == "error")

        print("==================================================================")
        print(f"  E2E TEST RUN SUMMARY: {passed} PASSED, {failed} FAILED")
        print("==================================================================")

        return passed, failed, findings

    finally:
        server_proc.terminate()

if __name__ == "__main__":
    p, f, errs = run_tests()
    if f > 0:
        sys.exit(1)
    sys.exit(0)
