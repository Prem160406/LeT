# Project Progress & Handoff Tracker
**Project:** Lab Operations, Emergency Fault Triage & Calibration Engine  
**Institution:** VIT Pune, Semester 3, Group 8, IT Department  
**Current Scope Milestone:** 100% Complete Implementation & Verified

---

## 1. Implementation Status Overview

| Component | Role / Purpose | Status | Complexity Proofed |
|---|---|---|---|
| **Data Models (`Asset.h`, `Asset.cpp`)** | Core records (Asset, FaultReport, TriageTask, HistoryEntry) | Completed | Constructors, deep copy, move semantics, blast tracking |
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
| **Comprehensive Report Engine (`Engine.h`, `.cpp`)** | Faulty equipment audit, Sources & SPOF analysis | Completed | Multi-criteria sorting & CSV export |
| **Interactive Web UI & Canvas (`/frontend`)** | Dashboard, Canvas Graph, Reports, Timeline | Completed | Verified via browser & REST test suite |

---

## 2. Completed Scope (100%)

- [x] **Zero-STL Container Rule**: All 8 core data structures implemented 100% from scratch. No `std::unordered_map`, `std::priority_queue`, `std::stack`, `std::map`, `std::vector`, or `std::list`.
- [x] **Hash Table Collision Handling**: Implemented with **Separate Chaining** using custom singly-linked bucket chains and polynomial rolling hash (`hash * 33 + c`). Rehashes dynamically when load factor $\ge 0.75$.
- [x] **Drift Calculation**:
  $$\text{Drift Rate} = \frac{\Delta(\text{Deviation})}{\Delta(\text{Time})} = \frac{\text{Dev}_{\text{latest}} - \text{Dev}_{\text{previous}}}{t_{\text{latest}} - t_{\text{previous}}}$$
  Verified on positive and negative sensor measurement drift.
- [x] **Composite Risk Score (Documented Assumption)**:
  $$\text{Base Risk} = 3 \times \text{Severity} + 2 \times \text{DaysOverdue} + 5 \times |\text{DriftVelocity}|$$
  *Design Assumption:* Absolute value applied to Drift Velocity term so that negative drift does not reduce triage urgency.
- [x] **Pipeline Step 5 (Blast Radius Multiplier in Fault Prioritization)**:
  $$\text{Effective Risk} = \text{Base Risk} \times (1.0 + 0.20 \times \text{Blast Radius Count})$$
  Discovers all affected downstream machines via Breadth-First Search (BFS) on the dependency graph and amplifies urgency accordingly.
- [x] **Faulty Equipment Report Generation**:
  Filters faulty machines, gathers historical accumulated downtime, latest drift velocity, and ranks them in strictly $O(N \log N)$ time using custom Merge Sort.
- [x] **Root Source Equipment & Single Point of Failure (SPOF) Analysis**:
  Evaluates out-degree, in-degree, and transitive downstream failure blast radius across the graph. Automatically tags critical nodes whose failure collapses $\ge 4$ machines as `CRITICAL (SPOF)`.
- [x] **AVL Tree Calibration Schedule**:
  Supports guaranteed $O(\log N + K)$ range queries on calibration due dates, segmenting equipment into Overdue, Due Soon ($\le 30$ days), and Compliant ($> 30$ days).
- [x] **Downloadable RFC 4180 CSV Reports**:
  Export buttons for Faulty Equipment CSV, Sources & SPOF CSV, and Calibration Due CSV.
- [x] **Interactive Dependency Topology Canvas**:
  HTML5 `<canvas>` rendering nodes, directed arrows, real-time BFS pulse wave animations on click, and failure cascade visualizer.
- [x] **Catastrophic Failure Simulation**:
  One-click cascade simulation on root infrastructure (e.g. `LAB-UPS-01`) showing intake buffering, graph blast evaluation, $2.0\times$ risk score amplification, and Max-Heap root placement.
- [x] **Single-Threaded HTTP REST Server**: Zero race-condition risk; handles requests synchronously on WinSock2.

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
