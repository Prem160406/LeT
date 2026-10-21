# Project Work Division & Contribution Matrix
**Project Title:** Lab Operations, Emergency Fault Triage & Calibration Engine  
**Course:** Data Structures & Algorithms (IT Department, Semester 3)  
**Institution:** Vishwakarma Institute of Technology (VIT), Pune  
**Group:** Group 8  

---

## Executive Summary

To ensure academic rigor and fair distribution of engineering effort, the architecture has been divided into **five distinct, balanced modules**. Every group member is assigned primary ownership of specific from-scratch data structures, algorithms, verification tests, and viva defense topics.

---

## 5-Member Contribution Matrix

| Member | Primary Role | Core Data Structures & Modules Owned | Key Source Files | Time / Space Complexities Owned |
|---|---|---|---|---|
| **Member 1** | **Systems Architect & Ingestion Engine** | • Circular Queue<br>• Hash Table (Separate Chaining)<br>• Core Entities & Custom Dynamic Array | `CircularQueue.h`, `CircularQueue.cpp`, `HashTable.h`, `HashTable.cpp`, `Asset.h`, `Asset.cpp`, `Common.h` | • CircularQueue: $O(1)$ push/pop<br>• HashTable: $O(1)$ avg search/insert<br>• Dynamic Rehashing at Load Factor $\ge 0.75$ |
| **Member 2** | **Triage & Drift Analytics Engineer** | • Doubly Linked List<br>• Linear Measurement Drift Rate<br>• Binary Max-Heap (Priority Queue)<br>• Pipeline Steps 3–4 Integration | `DoublyLinkedList.h`, `DoublyLinkedList.cpp`, `MaxHeap.h`, `MaxHeap.cpp`, `Engine.cpp` (Intake & Drift) | • DLL: $O(1)$ append / drift rate<br>• Max-Heap: $O(1)$ peekMax(), $O(\log N)$ insert / extractMax |
| **Member 3** | **Network Topology & Failure Propagation Engineer** | • Directed Graph (Adjacency List)<br>• BFS & DFS Blast Radius Engine<br>• Single Point of Failure (SPOF) Analysis<br>• Pipeline Step 5 Live Blast Factoring | `Graph.h`, `Graph.cpp`, `Engine.cpp` (Blast Multiplier & Cascade) | • Graph Traversal: $O(V + E)$ BFS/DFS<br>• Out-degree / In-degree source analysis<br>• Live Blast Multiplier Formula |
| **Member 4** | **Compliance, Scheduling & Integrity Engineer** | • AVL Tree (Self-Balancing BST)<br>• Undo Stack (LIFO Rollback Engine)<br>• Merge Sort (Standalone Compliance Sorter) | `AVLTree.h`, `AVLTree.cpp`, `UndoStack.h`, `UndoStack.cpp`, `MergeSort.h`, `MergeSort.cpp` | • AVL Tree: $O(\log N)$ rotations (LL, RR, LR, RL), $O(\log N + K)$ range queries<br>• Undo Stack: $O(1)$ push/pop<br>• MergeSort: $O(N \log N)$ stable sort |
| **Member 5** | **Transport, Intelligence & Dashboard Engineer** | • WinSock2 HTTP Transport Layer<br>• Interactive HTML5 Topology Canvas<br>• Lab AI Copilot & Supplier Lemon Index<br>• Maintenance Safety Advisor & Executive PDF | `HttpServer.h`, `HttpServer.cpp`, `httplib_single.h`, `frontend/index.html`, `frontend/styles.css`, `frontend/app.js` | • Single-threaded synchronous request handling<br>• Live telemetry aggregation & CSV exports<br>• Supplier Lemon Scorecard $[0, 100]$ |

---

## Detailed Individual Work Breakdown

### Member 1: Systems Architect & Ingestion Engine
* **Assigned Student Name:** ___________________________ (Roll No: _____________)
* **Responsibilities:**
  1. **Generic Custom Dynamic Array (`CustomArray<T>`)**: Implemented scratch memory allocation, dynamic resizing (capacity doubling), and bounds checking to eliminate `std::vector`.
  2. **Bounded Circular Queue (`CircularQueue`)**: Designed fixed-capacity ring buffer using modulo arithmetic `(tail + 1) % capacity` to buffer incoming service desk incident reports and prevent memory saturation during high-volume fault storms.
  3. **Separate Chaining Hash Table (`HashTable`)**: Implemented primary-key lookup by unique equipment code (e.g. `LAB-PC-017`). Implemented polynomial rolling hash function (djb2 variant: `hash * 33 + char`), custom singly-linked `HashNode` bucket chains, and dynamic rehashing when load factor exceeds $0.75$.
  4. **Pipeline Steps 1 & 2**: Connected Circular Queue intake into Hash Table lookup.
* **Viva Defense Preparation:**
  - *Why Separate Chaining instead of Linear Probing?* Separate chaining degrades gracefully to $O(K)$ where $K$ is chain depth and eliminates clustering cascades. It also avoids complex tombstone markers upon deletion.
  - *Why polynomial rolling hash?* Ensures uniform distribution across bucket arrays and minimizes hash collisions on alphanumeric asset codes.

---

### Member 2: Triage & Drift Analytics Engineer
* **Assigned Student Name:** ___________________________ (Roll No: _____________)
* **Responsibilities:**
  1. **Doubly Linked List (`DoublyLinkedList`)**: Implemented bi-directional linked list of `HistoryEntry` nodes for each asset, maintaining interleaved fault reports and recalibration milestones.
  2. **Linear Drift Velocity Engine**: Implemented physical measurement drift velocity formula over chronological history nodes:
     $$\text{Drift Rate} = \frac{\Delta(\text{Deviation})}{\Delta(\text{Time})} = \frac{\text{Dev}_{\text{latest}} - \text{Dev}_{\text{previous}}}{t_{\text{latest}} - t_{\text{previous}}}$$
  3. **Custom Binary Max-Heap Priority Queue (`MaxHeap`)**: Implemented array-backed complete binary tree for emergency triage. Programmed sift-up percolation on insertion and sift-down percolation on dispatch.
  4. **Composite Risk Score Computation**:
     $$\text{Base Risk} = 3 \times \text{Severity} + 2 \times \text{DaysOverdue} + 5 \times |\text{DriftVelocity}|$$
     Documented and verified the absolute value design assumption to ensure negative measurement drift does not reduce triage urgency.
* **Viva Defense Preparation:**
  - *How does Max-Heap maintain priority?* Array representation where parent of index $i$ is at $(i-1)/2$, left child at $2i+1$, and right child at $2i+2$. Every parent is guaranteed $\ge$ its children.
  - *What is the time complexity of peekMax() vs extractMax()?* Peek is $O(1)$ root array lookup; extract is $O(\log N)$ swapping root with last element and sifting down.

---

### Member 3: Network Topology & Failure Propagation Engineer
* **Assigned Student Name:** ___________________________ (Roll No: _____________)
* **Responsibilities:**
  1. **Directed Dependency Graph (`Graph`)**: Implemented scratch adjacency list using custom linked vertices and edges to model equipment dependency relationships (e.g., `LAB-UPS-01 -> LAB-COOL-01 -> LAB-LASER-01`).
  2. **BFS & DFS Blast-Radius Algorithms**: Implemented Breadth-First Search (using custom queue) and Depth-First Search (using recursion) to compute all direct and transitive downstream machines affected when an upstream power or network asset fails.
  3. **Pipeline Step 5 Live Prioritization Multiplier**:
     $$\text{Effective Risk} = \text{Base Risk} \times (1.0 + 0.20 \times \text{Blast Radius Count})$$
  4. **Single Point of Failure (SPOF) Root Cause Analysis**: Built algorithms to calculate out-degree, in-degree, and classify systemic risk (e.g. tagging source nodes affecting $\ge 4$ machines as `CRITICAL (SPOF)`).
* **Viva Defense Preparation:**
  - *Why use BFS for blast radius calculation?* BFS explores transitive failure propagation level-by-level, making it ideal for determining ripple effect stages.
  - *Time complexity of BFS on Adjacency List?* Strictly $O(V + E)$ where $V$ is equipment count and $E$ is dependency connections.

---

### Member 4: Compliance, Scheduling & Integrity Engineer
* **Assigned Student Name:** ___________________________ (Roll No: _____________)
* **Responsibilities:**
  1. **Self-Balancing AVL Tree (`AVLTree`)**: Implemented binary search tree indexed by `next_calibration_due` date. Programmed height calculations, balance factor verification ($|BF| \le 1$), and all four rotation cases:
     - Left-Left (LL) Single Right Rotation
     - Right-Right (RR) Single Left Rotation
     - Left-Right (LR) Double Rotation
     - Right-Left (RL) Double Rotation
  2. **Range Query Engine**: Implemented $O(\log N + K)$ in-order traversal to retrieve all assets due for calibration within any query window $[A, B]$.
  3. **Undo Stack (`UndoStack`)**: Implemented scratch LIFO stack storing inverse delta transaction records for atomic $O(1)$ rollback of accidental fault reports and job dispatches.
  4. **Merge Sort Engine (`MergeSort`)**: Implemented divide-and-conquer stable sorting algorithm to rank equipment by accumulated downtime, fault count, or calibration urgency in $O(N \log N)$ time.
* **Viva Defense Preparation:**
  - *Why AVL Tree instead of standard BST?* Standard BST can degenerate into an $O(N)$ linked list on sorted insertions (like ascending due dates). AVL rotations guarantee $O(\log N)$ height balance.
  - *How does the Undo Stack work?* It stores inverse delta records (e.g. if a dispatch popped a task, undo pushes it back; if a report was filed, undo removes it from DLL and heap).

---

### Member 5: Transport, Intelligence & Dashboard Engineer
* **Assigned Student Name:** ___________________________ (Roll No: _____________)
* **Responsibilities:**
  1. **WinSock2 HTTP REST Transport Layer (`HttpServer`)**: Configured single-threaded synchronous HTTP request handling, endpoint routing, and JSON serialization with zero third-party dependencies.
  2. **Interactive Web Dashboard & HTML5 Canvas (`/frontend`)**: Developed responsive dark-theme dashboard with interactive topology canvas, animated BFS ripple waves, and in-page toast notifications without emojis.
  3. **Supplier Reliability & Lemon Scorecard**: Developed vendor grading algorithm:
     $$\text{Score} = 100 - (15 \times \text{AvgFaults} + 20 \times |\text{AvgDrift}| + 2 \times \text{AvgDowntime})$$
     Classifying suppliers into Tier A (Certified), Tier B (Watchlist), and Tier C (Defective Lemon).
  4. **Predictive Failure Forecast (RUL) & Maintenance Safety Advisor**: Built ETTF forecasting and what-if outage check before equipment shutdowns.
  5. **Offline Local AI Copilot & Official PDF Certificate**: Built offline assistant with prompt chips and printable executive audit certificate formatted for VIT Pune academic submission.
* **Viva Defense Preparation:**
  - *Why single-threaded synchronous HTTP?* Eliminates multi-threading data races on scratch data structures (Heap, AVL Tree, Hash Table) without needing mutex overhead for project scope.
  - *How does the Supplier Lemon Scorecard work?* Penalizes vendors with high fault frequencies, rapid sensor drift, and accumulated downtime, recommending warranty claims for scores $< 60$.

---

## Academic Assessment & Verification Checklist

- [x] All 8 Data Structures built **100% from scratch** (No STL containers).
- [x] All 11 verification test suites passing 100% via `mingw32-make test`.
- [x] Clean multi-file structure (`/include`, `/src`, `/frontend`, `Makefile`).
- [x] Live single-threaded server running on `http://localhost:8080/`.
- [x] Strictly **0 emojis** across all code, UI, and documentation.
- [x] Complete viva defense question preparation for all 5 team members.
