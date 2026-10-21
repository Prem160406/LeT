# Lab Operations, Emergency Fault Triage & Calibration Engine

[![VIT Pune](https://img.shields.io/badge/VIT%20Pune-IT%20Dept%20(Sem%203)-blue.svg)](#)
[![C++14](https://img.shields.io/badge/C%2B%2B-14-green.svg)](#)
[![Data Structures](https://img.shields.io/badge/DSA-100%25%20From%20Scratch-orange.svg)](#)
[![Platform](https://img.shields.io/badge/Platform-Windows%20(MinGW)-lightgrey.svg)](#)

An academic Data Structures & Algorithms project (VIT Pune, Semester 3, Group 8, Information Technology Department). The system manages laboratory equipment, detects sensor measurement drift, prioritizes emergency maintenance faults, and schedules calibrations.

> **Strict Rule:** Every core data structure is implemented **100% FROM SCRATCH** in C++ without using STL container substitutes (`std::vector`, `std::map`, `std::unordered_map`, `std::priority_queue`, `std::list`, `std::stack`, `std::queue`).

---

## Core Data Structures Implemented From Scratch

| # | Data Structure | Exact Role in Engine | Time Complexity |
|---|---|---|---|
| 1 | **Circular Queue** | Fixed-capacity, memory-bounded FIFO buffer for incoming service desk intake tickets. | $O(1)$ push / pop / peek |
| 2 | **Hash Table** | Primary key asset lookup by unique equipment code (e.g. `LAB-PC-017`). Uses **Separate Chaining** with custom singly-linked buckets and dynamic rehashing at load factor $\ge 0.75$. | Average $O(1)$ lookup |
| 3 | **Doubly Linked List** | Bi-directional interleaved fault and calibration history nodes per asset; enables linear drift rate calculation $\frac{\Delta(\text{Dev})}{\Delta(\text{Time})}$. | $O(1)$ append / unlink, $O(1)$ drift rate |
| 4 | **Max-Heap** | Priority queue triaging emergency jobs by live composite Risk Score: $\text{BaseRisk} = 3S + 2\text{Overdue} + 5\|\text{Drift}\|$. Root always holds the highest urgency job. | $O(1)$ root peek, $O(\log N)$ extraction / insertion |
| 5 | **AVL Tree** | Self-balancing BST indexed by `next_calibration_due` date. Maintains $|BF| \le 1$ via Left-Left, Right-Right, Left-Right, and Right-Left rotations. | $O(\log N)$ search / insert, $O(\log N + K)$ range queries |
| 6 | **Undo Stack** | LIFO stack recording atomic state snapshots for instant rollback of transactions. | $O(1)$ push / pop rollback |
| 7 | **Merge Sort** | Standalone divide-and-conquer sorter for compliance audit reporting (sorts assets by downtime or fault count). | Strictly $O(N \log N)$ worst / avg / best |
| 8 | **Graph (Adjacency List)** | Directed dependency graph representing equipment operational connectivity. Uses BFS & DFS for blast-radius failure impact analysis. | $O(V + E)$ traversal |

---

## Intelligence & Compliance Suite Features

1. **DSA Architecture "Glass-Box" Mode**:
   - Clean, professional dashboard by default; header toggle `[DSA Inspector: OFF/ON]` reveals scratch C++ containers and Big-O complexities on every widget.
   - Includes interactive **DSA Specification & Viva Defense Sheet** detailing collision handling, heap percolation, AVL rotation cases, and memory bounds.
2. **Supplier Reliability & "Lemon" Scorecard**:
   - Evaluates vendor hardware quality: $\text{Score} = 100 - (15\times\text{AvgFaults} + 20\times\text{AvgDrift} + 2\times\text{AvgDowntime})$.
   - Categorizes manufacturers into Tier A (Certified), Tier B (Watchlist), and Tier C (Defective "Lemon" with warranty claim guidance).
3. **Predictive Failure Forecast (Remaining Useful Life - RUL)**:
   - Uses live linear drift velocities $\Delta(\text{Dev})/\Delta(\text{Time})$ to calculate Estimated Time to Failure (ETTF) before an instrument breaches physical limits.
4. **Maintenance Window Safety Advisor (What-If Outage Engine)**:
   - Evaluates whether shutting down an asset for maintenance will disrupt downstream experiments via Graph BFS and Max-Heap conflict cross-checking.
5. **Offline Local Lab AI Copilot**:
   - Embedded floating assistant with one-click query chips; parses questions on fault history, worst suppliers, predictive drift trends, and executes live dispatches.
6. **Official Executive PDF Compliance Certificate**:
   - Print-formatted compliance audit certificate with official VIT Pune header, fleet health KPI bar, supplier scorecard, SPOFs, and formal sign-off blocks.
7. **Form Input Guidance Micro-Copy**:
   - Clear helper text beneath every input field explaining expected formats and algorithmic impact.

---

## Live Pipeline Working Principle

```text
[Incoming Fault Report]
         │
         ▼
 1. Circular Queue (O(1) Bounded Intake Buffer)
         │
         ▼
 2. Hash Table (O(1) Resolution by Asset Code)
         │
         ▼
 3. Doubly Linked List (O(1) History Append & Linear Drift Calculation)
         │
         ▼
 4. Graph BFS Blast-Radius Multiplier
    EffectiveRisk = BaseRisk * (1.0 + 0.20 * BlastRadius)
         │
         ▼
 5. Max-Heap Priority Urgency Queue (O(log N) Priority Triage)
         │
         ├─► AVL Tree (O(log N) Calibration Schedule Indexing)
         └─► Undo Stack (O(1) LIFO Rollback Snapshot)
```

---

## Quick Start (Windows)

### 1. Launch Web Dashboard (One-Click)
Double-click [`start.bat`](start.bat) or run in terminal:
```cmd
start.bat
```
This automatically compiles the project (if needed), launches the server in the background, and opens your browser to:
**`http://localhost:8080/`**

To stop the background server:
```cmd
stop.bat
```

### 2. Run CLI Verification Suite (All 11 Demos)
```cmd
mingw32-make test
```
*or directly:*
```cmd
lab_engine.exe --test
```

### 3. Manual Build via Makefile
```cmd
mingw32-make clean
mingw32-make
```

---

## Repository Structure

```text
├── include/
│   ├── Common.h            # Scratch dynamic array (CustomArray<T>)
│   ├── Asset.h             # Core entities (Asset, FaultReport, TriageTask)
│   ├── CircularQueue.h     # 1. Bounded FIFO Ring Buffer
│   ├── HashTable.h         # 2. Separate Chaining Hash Table
│   ├── DoublyLinkedList.h  # 3. Bi-directional History DLL
│   ├── MaxHeap.h           # 4. Risk Score Priority Queue
│   ├── AVLTree.h           # 5. Self-Balancing Calibration Index
│   ├── UndoStack.h         # 6. LIFO Rollback Stack
│   ├── MergeSort.h         # 7. O(N log N) Compliance Sorter
│   ├── Graph.h             # 8. Dependency Graph (BFS/DFS)
│   ├── Engine.h            # Core Pipeline Coordinator
│   ├── HttpServer.h        # REST API & Static Server
│   └── httplib_single.h    # Transport layer single-header
├── src/
│   ├── Asset.cpp
│   ├── CircularQueue.cpp
│   ├── HashTable.cpp
│   ├── DoublyLinkedList.cpp
│   ├── MaxHeap.cpp
│   ├── AVLTree.cpp
│   ├── UndoStack.cpp
│   ├── MergeSort.cpp
│   ├── Graph.cpp
│   ├── Engine.cpp
│   ├── HttpServer.cpp
│   └── main.cpp            # Test suite & server entry point
├── frontend/
│   ├── index.html          # Web dashboard (Emoji-free, accessible layout)
│   ├── styles.css          # Clean dark enterprise design system
│   └── app.js              # REST client & interactive topology canvas
├── Makefile                # MinGW build specification
├── PROGRESS.md             # 100% milestone audit trail
├── start.bat               # Windows start launcher
└── stop.bat                # Windows stop script
```

---

## Authors
* **Institution:** Vishwakarma Institute of Technology (VIT), Pune
* **Department:** Information Technology
* **Academic Term:** Semester 3, Group 8
