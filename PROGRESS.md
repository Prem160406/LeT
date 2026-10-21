# Project Progress & Handoff Tracker
**Project:** Lab Operations, Emergency Fault Triage & Calibration Engine  
**Institution:** VIT Pune, Semester 3, Group 8, IT Department  
**Current Scope Milestone:** 100% Complete Implementation & Verified with Intelligence Suite

---

## 1. Implementation Status Overview

| Component | Role / Purpose | Status | Complexity Proofed |
|---|---|---|---|
| **Data Models (`Asset.h`, `Asset.cpp`)** | Core records (Asset, FaultReport, TriageTask, HistoryEntry) | Completed | Constructors, deep copy, move semantics, supplier & blast tracking |
| **1. Circular Queue (`CircularQueue.h`, `.cpp`)** | Fixed-capacity FIFO service report intake | Completed | $O(1)$ push/pop, bounded wrap-around verified |
| **2. Hash Table (`HashTable.h`, `.cpp`)** | Asset lookup by unique asset code | Completed | Average $O(1)$ lookup via Separate Chaining, verified |
| **3. Doubly Linked List (`DoublyLinkedList.h`, `.cpp`)** | Asset history + linear drift rate calculation | Completed | $O(1)$ append, $O(1)$ drift $\Delta(\text{Dev})/\Delta(\text{Time})$ verified |
| **4. Max-Heap (`MaxHeap.h`, `.cpp`)** | Priority Queue for live Risk Score triage | Completed | $O(1)$ root urgency, $O(\log N)$ extraction verified |
| **5. AVL Tree (`AVLTree.h`, `.cpp`)** | Calibration scheduling indexed by due date | Completed | $O(\log N)$ rotations (LL, RR, LR, RL) & range queries |
| **6. Undo Stack (`UndoStack.h`, `.cpp`)** | LIFO rollback of data entries & triage | Completed | $O(1)$ push/pop transaction rollback verified |
| **7. Merge Sort (`MergeSort.h`, `.cpp`)** | Asset compliance and downtime audit sorting | Completed | $O(N \log N)$ recursive divide-and-conquer verified |
| **8. Graph (`Graph.h`, `.cpp`)** | Dependency modeling & blast-radius analysis | Completed | $O(V + E)$ BFS & DFS traversal verified |
| **Engine Pipeline Steps 1-4 (`Engine.h`, `.cpp`)** | Intake -> Hash -> DLL -> Drift -> Heap | Completed | Fully wired and verified |
| **Pipeline Step 5 (`Engine.h`, `.cpp`)** | Blast-Radius Live Prioritization Multiplier | Completed | $\text{EffectiveRisk} = \text{BaseRisk} \times (1.0 + 0.20 \times \text{Blast})$ |
| **Supplier Lemon Scorecard (`HttpServer.cpp`)** | Vendor quality grading, Lemon detection & warranty actions | Completed | $\text{Score} = 100 - (15\times\text{Faults} + 20\times\text{Drift} + 2\times\text{Downtime})$ |
| **Predictive RUL Forecast (`HttpServer.cpp`)** | Remaining Useful Life & Estimated Time to Failure (ETTF) | Completed | $\text{ETTF} = (\text{Tolerance} - |\text{Dev}|) / |\text{Drift}|$ |
| **Maintenance Safety Advisor (`HttpServer.cpp`)** | What-if outage check before taking assets offline | Completed | BFS dependent discovery + Max-Heap conflict cross-check |
| **DSA Glass-Box Inspector (`/frontend`)** | Reveal-on-demand algorithm mode & viva defense sheet | Completed | Dynamic UI overlay + Big-O defense modal |
| **Offline Lab AI Copilot (`frontend/app.js`)** | Actionable local assistant for queries, dispatch & simulations | Completed | Instant natural-language parser & query chips |
| **Official Executive PDF Certificate** | Print-formatted lab compliance & health audit | Completed | High-res print styling with official VIT Pune header & sign-offs |

---

## 2. Completed Scope (100% + Intelligence Suite)

- [x] **Zero-STL Container Rule**: All 8 core data structures implemented 100% from scratch. No `std::unordered_map`, `std::priority_queue`, `std::stack`, `std::map`, `std::vector`, or `std::list`.
- [x] **Hash Table Collision Handling**: Implemented with **Separate Chaining** using custom singly-linked bucket chains and polynomial rolling hash (`hash * 33 + c`). Rehashes dynamically when load factor $\ge 0.75$.
- [x] **Drift Calculation**:
  $$\text{Drift Rate} = \frac{\Delta(\text{Deviation})}{\Delta(\text{Time})} = \frac{\text{Dev}_{\text{latest}} - \text{Dev}_{\text{previous}}}{t_{\text{latest}} - t_{\text{previous}}}$$
- [x] **Composite Risk Score (Documented Assumption)**:
  $$\text{Base Risk} = 3 \times \text{Severity} + 2 \times \text{DaysOverdue} + 5 \times |\text{DriftVelocity}|$$
  *Design Assumption:* Absolute value applied to Drift Velocity term so that negative drift does not reduce triage urgency.
- [x] **Pipeline Step 5 (Blast Radius Multiplier in Fault Prioritization)**:
  $$\text{Effective Risk} = \text{Base Risk} \times (1.0 + 0.20 \times \text{Blast Radius Count})$$
- [x] **Supplier Reliability & Lemon Index**:
  Aggregates equipment by manufacturer and computes a reliability score $[0, 100]$. Categorizes into Tier A (Certified), Tier B (Watchlist), and Tier C (Defective "Lemon" with warranty claim recommendations).
- [x] **Predictive Maintenance & Failure Forecasting (RUL)**:
  Uses live linear drift rates to compute Estimated Time to Failure (ETTF) before an instrument exceeds its $5.0\text{ mm}$ physical tolerance limit.
- [x] **Maintenance Window Safety Advisor (What-If Engine)**:
  Pre-flight check before taking an instrument offline for maintenance, verifying downstream dependents and cross-checking pending jobs in the triage queue.
- [x] **DSA Architecture Inspector**:
  Header toggle that reveals the underlying custom data structures, Big-O complexities, and opens an in-depth viva defense modal.
- [x] **Actionable Offline Lab AI Copilot**:
  Interactive assistant supporting one-click prompt chips, answering questions on fault history, worst suppliers, next expected fault, and executing live job dispatches.
- [x] **Official Executive PDF / Print Audit Certificate**:
  Printable laboratory audit document complete with executive KPI bar, supplier scorecard, SPOF list, calibration schedule, and official sign-off lines.
- [x] **Single-Threaded HTTP REST Server**: Synchronous request handling on WinSock2 with zero race-condition risk.
- [x] **Zero Emojis**: 100% clean, professional enterprise typography throughout.

---

## 3. How to Build & Run

### Quick Launch via Batch Scripts (Windows)
- **Start Server & Open Browser:** Double-click or run `start.bat`
- **Stop Server:** Double-click or run `stop.bat`

### Building the Project Manually
```bash
mingw32-make clean
mingw32-make
```

### Running the Complete CLI Verification Suite (All 11 Demos)
```bash
mingw32-make test
```

### Launching the Web Server and Frontend UI Manually
```bash
./lab_engine.exe --server 8080
# OR
mingw32-make server
```
Then open your web browser at: **`http://localhost:8080/`**
