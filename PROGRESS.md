# Project Progress & Handoff Tracker
**Project:** Lab Operations, Emergency Fault Triage & Calibration Engine  
**Institution:** VIT Pune, Semester 3, Group 8, IT Department  
**Current Scope Milestone:** ~70% Target Implementation Completed & Verified

---

## 1. Implementation Status Overview

| Component | Role / Purpose | Status | Complexity Proofed |
|---|---|---|---|
| **Data Models (`Asset.h`, `Asset.cpp`)** | Core records (Asset, FaultReport, TriageTask, HistoryEntry) | Completed | Complete constructors, deep copy & move |
| **1. Circular Queue (`CircularQueue.h`, `.cpp`)** | Fixed-capacity FIFO service report intake | Completed | $O(1)$ push/pop, bounded wrap-around verified |
| **2. Hash Table (`HashTable.h`, `.cpp`)** | Asset lookup by unique asset code | Completed | Average $O(1)$ lookup via Separate Chaining, verified |
| **3. Doubly Linked List (`DoublyLinkedList.h`, `.cpp`)** | Asset history + linear drift rate calculation | Completed | $O(1)$ append, $O(1)$ drift $\Delta(\text{Dev})/\Delta(\text{Time})$ verified |
| **4. Max-Heap (`MaxHeap.h`, `.cpp`)** | Priority Queue for live Risk Score triage | Completed | $O(1)$ root urgency, $O(\log N)$ extraction verified |
| **5. AVL Tree (`AVLTree.h`, `.cpp`)** | Calibration scheduling indexed by due date | Completed | $O(\log N)$ rotations (LL, RR, LR, RL) & range queries |
| **6. Undo Stack (`UndoStack.h`, `.cpp`)** | LIFO rollback of data entries & triage | Completed | $O(1)$ push/pop transaction rollback verified |
| **7. Merge Sort (`MergeSort.h`, `.cpp`)** | Standalone asset compliance sorting | Completed | $O(N \log N)$ recursive divide-and-conquer verified |
| **8. Graph (`Graph.h`, `.cpp`)** | Dependency modeling & blast-radius analysis | Completed | $O(V + E)$ BFS & DFS traversal verified |
| **Engine Pipeline (`Engine.h`, `.cpp`)** | Pipeline Steps 1-4 wired end-to-end | Completed | Intake $\rightarrow$ Hash $\rightarrow$ DLL $\rightarrow$ Drift $\rightarrow$ Heap |
| **Web Server & UI (`HttpServer.cpp`, `/frontend`)** | Single-threaded HTTP REST API & dashboard | Completed | Verified via browser & REST test suite |

---

## 2. Completed Scope (70%)

- [x] **Zero-STL Container Rule**: All 8 core data structures implemented 100% from scratch. No `std::unordered_map`, `std::priority_queue`, `std::stack`, `std::map`, `std::vector`, or `std::list`.
- [x] **Hash Table Collision Handling**: Implemented with **Separate Chaining** using custom singly-linked bucket chains and polynomial rolling hash (`hash * 33 + c`). Rehashes dynamically when load factor $\ge 0.75$.
- [x] **Drift Calculation**:
  $$\text{Drift Rate} = \frac{\Delta(\text{Deviation})}{\Delta(\text{Time})} = \frac{\text{Dev}_{\text{latest}} - \text{Dev}_{\text{previous}}}{t_{\text{latest}} - t_{\text{previous}}}$$
  Verified on positive and negative sensor measurement drift.
- [x] **Composite Risk Score (Documented Assumption)**:
  $$\text{Risk} = 3 \times \text{Severity} + 2 \times \text{DaysOverdue} + 5 \times |\text{DriftVelocity}|$$
  *Design Assumption:* Absolute value applied to Drift Velocity term so that negative drift does not reduce triage urgency.
- [x] **AVL Tree Balancing**: Self-balances using all 4 rotation cases (Left-Left, Right-Right, Left-Right, Right-Left) maintaining $|BF| \le 1$. Supports $O(\log N + K)$ range queries on calibration due dates.
- [x] **Undo Stack Rollback**: Reverses transactions (asset registration, fault report submission, triage dispatch) in $O(1)$ LIFO order.
- [x] **Merge Sort**: Standalone $O(N \log N)$ sorting of asset records by downtime, fault count, and calibration due date.
- [x] **Graph BFS/DFS**: Adjacency list dependency graph modeling equipment blast radius with complete breadth-first and depth-first searches.
- [x] **Pipeline Steps 1-4 End-to-End**:
  1. Service reports buffer in Circular Queue ($O(1)$).
  2. Reports locate target asset in Hash Table ($O(1)$) and append to its Doubly Linked List ($O(1)$).
  3. Drift Rate computed from recent history nodes ($O(1)$).
  4. Composite Risk Score computed and task prioritized into Max-Heap ($O(\log N)$).
- [x] **Single-Threaded HTTP REST Server**: Zero race-condition risk; serves static frontend files and JSON endpoints (`/api/assets`, `/api/reports`, `/api/triage`, `/api/dispatch`, `/api/undo`, `/api/blast-radius-demo`, `/api/compliance-sort-demo`).
- [x] **Interactive Frontend**: Clean modern web interface with Add Asset form, Ingest Service Report form, Live Max-Heap Triage table (highlighting root task), Undo button, and standalone demo controls.

---

## 3. Deliberately Stubbed Scope (30% Handoff Safety)

The following 4 components are deliberately left stubbed for future development phases. Every stubbed site in code contains an unambiguous block comment.

### Stub 1: Pipeline Step 5 — Blast-Radius Multiplier in Fault Prioritization
* **Source Location:** `src/Engine.cpp` -> `Engine::calculateBlastRadiusFactor(const std::string& assetCode)`
* **Status:** Graph BFS and DFS are 100% complete and verified standalone. The multiplier calculation is stubbed to return neutral `1.0` to keep steps 1-4 isolated.
* **Exact Inputs/Outputs:**
  - Input: `const std::string& assetCode` (primary key of equipment)
  - Output: `double multiplier` ($\ge 1.0$)
* **Functions to Call:**
  - `dependencyGraph.bfs(assetCode, affectedList)` in `src/Graph.cpp`
  - `affectedList.size()` in `include/Common.h`
* **Finalized Design Decision:**
  - When fault severity $\ge 4$, augment base risk:
    $$\text{Effective Risk} = \text{Base Risk} \times (1.0 + 0.20 \times \text{Dependent Count})$$
* **Exact Steps for Next Developer:**
  1. In `src/Engine.cpp`, open `calculateBlastRadiusFactor()`.
  2. Call `dependencyGraph.bfs(assetCode, affected);`.
  3. Return `1.0 + (0.20 * static_cast<double>(affected.size()));`.
  4. In `Engine::processNextBufferReport()`, multiply `riskScore *= calculateBlastRadiusFactor(targetAsset->assetCode);` before inserting into `triageQueue`.

### Stub 2: Compliance Reporting UI on top of Merge Sort
* **Source Location:** `src/Engine.cpp` -> `Engine::generateComplianceReport()`
* **Status:** `MergeSort` is 100% complete and verified standalone. Automated PDF/HTML compliance document generation and report UI are stubbed.
* **Exact Inputs/Outputs:**
  - Input: `SortCriterion criterion` (`SORT_BY_DOWNTIME_DESC`, `SORT_BY_FAULT_COUNT_DESC`)
  - Output: `CustomArray<Asset*>& outReport`
* **Functions to Call:**
  - `assetRegistry.getAllAssets(outReport)`
  - `MergeSort::sort(outReport, criterion)`
* **Exact Steps for Next Developer:**
  1. Add a dedicated "Compliance Audit" tab in `frontend/index.html`.
  2. Connect to the existing `/api/compliance-sort-demo` endpoint.
  3. Render summary metrics (top 3 highest downtime assets, compliance percentage).

### Stub 3: Frontend View for AVL-Driven Calibration Scheduling
* **Source Location:** `src/Engine.cpp` -> `Engine::getUpcomingCalibrations()`
* **Status:** `AVLTree` balancing and $O(\log N + K)$ range queries are 100% complete and verified. Calendar/timeline UI view is stubbed.
* **Exact Inputs/Outputs:**
  - Input: `long long fromDate`, `long long toDate`
  - Output: `CustomArray<CalibrationItem>& outItems`
* **Functions to Call:**
  - `calibrationIndex.getRange(fromDate, toDate, outItems)`
* **Exact Steps for Next Developer:**
  1. Expose `GET /api/calibrations?start=X&end=Y` in `HttpServer.cpp`.
  2. Add date range selector in `frontend/index.html`.
  3. Highlight equipment whose calibration is overdue (due date < current timestamp).

### Stub 4: Frontend Blast-Radius Graph Canvas Visualization
* **Source Location:** `frontend/app.js`
* **Status:** REST API `/api/blast-radius-demo?asset=ID` returns full list of affected nodes via BFS/DFS. Visual node-link interactive canvas rendering is stubbed (text summary currently displayed).
* **Exact Steps for Next Developer:**
  1. Add an `<svg>` or `<canvas id="dependency-canvas">` inside `frontend/index.html`.
  2. In `frontend/app.js`, parse `bfsAffected` and draw nodes and directed arrows from source asset to impacted dependents.

---

## 4. How to Build & Run

### Prerequisites
- Windows OS
- MinGW GCC (`g++` supporting C++14)
- `mingw32-make`

### Quick Launch via Batch Scripts (Windows)
- **Start Server & Open Browser:** Double-click or run `start.bat`
- **Stop Server:** Double-click or run `stop.bat`

### Building the Project Manually
```bash
mingw32-make clean
mingw32-make
```

### Running the CLI Verification Suite (All 8 Data Structures)
```bash
./lab_engine.exe --test
# OR
mingw32-make test
```

### Launching the Web Server and Frontend UI Manually
```bash
./lab_engine.exe --server 8080
# OR
mingw32-make server
```
Then open your web browser at: **`http://localhost:8080/`**
