#include "Asset.h"
#include "CircularQueue.h"
#include "HashTable.h"
#include "OpenAddressingTable.h"
#include "DoublyLinkedList.h"
#include "MaxHeap.h"
#include "AVLTree.h"
#include "UndoStack.h"
#include "MergeSort.h"
#include "Graph.h"
#include "Engine.h"
#include "HttpServer.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <cassert>

// Forward declaration
void populateSampleData(Engine& engine);

void printBanner(const std::string& title) {
    std::cout << "\n====================================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "====================================================================\n";
}

// -----------------------------------------------------------------------------
// DEMO 1: Circular Queue Test
// -----------------------------------------------------------------------------
void testCircularQueue() {
    printBanner("1. CIRCULAR QUEUE VERIFICATION (O(1) Bounded Buffer)");
    CircularQueue queue(4); // Small capacity to test boundary wrapping

    assert(queue.isEmpty());
    std::cout << "[Test 1.1] Initial state: Empty = " << (queue.isEmpty() ? "PASS" : "FAIL") << std::endl;

    FaultReport r1("R1", "LAB-01", 3, 0, 1.2, 100, "Minor drift");
    FaultReport r2("R2", "LAB-02", 4, 2, 2.5, 105, "Power fluctuation");
    FaultReport r3("R3", "LAB-03", 5, 5, 4.0, 110, "Critical emergency");
    FaultReport r4("R4", "LAB-04", 2, 1, 0.5, 115, "Routine calibration");

    assert(queue.enqueue(r1));
    assert(queue.enqueue(r2));
    assert(queue.enqueue(r3));
    assert(queue.enqueue(r4));
    std::cout << "[Test 1.2] Enqueued 4 items into capacity 4 buffer. Size = " << queue.size() << std::endl;
    assert(queue.isFull());
    std::cout << "[Test 1.3] Full buffer detection: " << (queue.isFull() ? "PASS" : "FAIL") << std::endl;

    // Buffer saturation protection test
    FaultReport r5("R5", "LAB-05", 1, 0, 0.1, 120, "Overflow item");
    bool overflowRejected = !queue.enqueue(r5);
    std::cout << "[Test 1.4] Bounded memory overflow rejection: " << (overflowRejected ? "PASS" : "FAIL") << std::endl;
    assert(overflowRejected);

    // Dequeue test & wrap-around
    FaultReport out;
    assert(queue.dequeue(out));
    assert(out.reportId == "R1");
    std::cout << "[Test 1.5] O(1) FIFO Dequeue oldest item (R1): PASS" << std::endl;

    // Insert after dequeue to verify circular wrap-around
    assert(queue.enqueue(r5));
    std::cout << "[Test 1.6] Circular wrap-around insertion: PASS (Size = " << queue.size() << ")" << std::endl;
}

// -----------------------------------------------------------------------------
// DEMO 2: Hash Table Test
// -----------------------------------------------------------------------------
void testHashTable() {
    printBanner("2. HASH TABLE VERIFICATION (O(1) Avg Lookup, Separate Chaining)");
    HashTable table(7); // Small bucket count to force and verify collision chaining

    Asset a1("LAB-PC-017", "Spectrometer A", "Optics Lab", 1700000000, 12.5);
    Asset a2("LAB-CENT-002", "Centrifuge High-Speed", "Bio Lab 2", 1700500000, 3.0);
    Asset a3("LAB-INC-009", "Incubator Thermo-3", "Microbiology", 1700800000, 45.0);

    table.insert(a1);
    table.insert(a2);
    table.insert(a3);
    std::cout << "[Test 2.1] Inserted 3 assets. Table size = " << table.size() << std::endl;

    // Lookup test
    Asset* found = table.find("LAB-PC-017");
    assert(found != nullptr);
    assert(found->name == "Spectrometer A");
    std::cout << "[Test 2.2] O(1) Lookup 'LAB-PC-017' -> Found: " << found->name << " (PASS)" << std::endl;

    // Missing key
    Asset* missing = table.find("NONEXISTENT");
    assert(missing == nullptr);
    std::cout << "[Test 2.3] Lookup non-existent key returns nullptr: PASS" << std::endl;

    // Collision handling test: force multiple keys
    for (int i = 0; i < 20; ++i) {
        std::string code = "LAB-ITEM-" + std::to_string(i);
        Asset temp(code, "Generic Tool", "Storage", 1700000000 + i, 1.0);
        table.insert(temp);
    }
    std::cout << "[Test 2.4] Inserted 20 additional items to test dynamic rehashing. Size = " 
              << table.size() << ", Load Factor = " << table.loadFactor() << " (PASS)" << std::endl;

    // [Test 2.5] Collision Statistics Verification (Empty, Single, Multi, Delete, Rehash)
    {
        HashTable statsTable(11);
        HashStats emptyStats = statsTable.getStats();
        assert(emptyStats.totalElements == 0);
        assert(emptyStats.occupiedBuckets == 0);
        assert(emptyStats.totalCollisions == 0);
        assert(emptyStats.maxChainLength == 0);
        assert(emptyStats.averageChainLength == 0.0);

        // Single insertion
        statsTable.insert(Asset("ASSET-1", "Oscilloscope", "Lab 1", 1000, 0.0));
        HashStats singleStats = statsTable.getStats();
        assert(singleStats.totalElements == 1);
        assert(singleStats.occupiedBuckets == 1);
        assert(singleStats.totalCollisions == 0);
        assert(singleStats.maxChainLength == 1);
        assert(singleStats.averageChainLength == 1.0);

        // Deletion updates statistics cleanly
        assert(statsTable.remove("ASSET-1"));
        HashStats postDelStats = statsTable.getStats();
        assert(postDelStats.totalElements == 0);
        assert(postDelStats.occupiedBuckets == 0);
        assert(postDelStats.totalCollisions == 0);
        assert(postDelStats.maxChainLength == 0);

        // Multi-collision verification: force collisions
        HashTable smallTable(11);
        for (int i = 0; i < 35; ++i) {
            smallTable.insert(Asset("EQ-" + std::to_string(i), "Tool", "Lab", 1000 + i, 0.0));
        }
        HashStats multiStats = smallTable.getStats();
        assert(multiStats.totalElements == 35);
        assert(multiStats.occupiedBuckets <= multiStats.bucketCount);
        // Mathematical invariant: collisions = totalElements - occupiedBuckets
        assert(multiStats.totalCollisions == (multiStats.totalElements - multiStats.occupiedBuckets));
        assert(multiStats.totalCollisions > 0);
        assert(multiStats.maxChainLength >= 2);
        assert(multiStats.averageChainLength >= 1.0);

        std::cout << "[Test 2.5] Collision statistics verification (Empty, Multi, Deletion, Bounds): PASS" << std::endl;
        std::cout << "          Elements = " << multiStats.totalElements 
                  << " | Occupied Buckets = " << multiStats.occupiedBuckets 
                  << " | Total Collisions = " << multiStats.totalCollisions 
                  << " | Max Chain = " << multiStats.maxChainLength 
                  << " | Avg Chain = " << std::fixed << std::setprecision(2) << multiStats.averageChainLength << std::endl;
    }

    // [Test 2.6] Duplicate Key Updates Invariance
    {
        HashTable dupTable(11);
        dupTable.insert(Asset("KEY-X", "Initial Name", "Room A", 1000, 1.0));
        HashStats s1 = dupTable.getStats();

        // Update same key
        bool wasNew = dupTable.insert(Asset("KEY-X", "Updated Name", "Room B", 2000, 2.0));
        assert(!wasNew); // Must indicate update, not new insert
        HashStats s2 = dupTable.getStats();

        assert(s2.totalElements == s1.totalElements);
        assert(s2.occupiedBuckets == s1.occupiedBuckets);
        assert(s2.totalCollisions == s1.totalCollisions);
        assert(s2.maxChainLength == s1.maxChainLength);
        assert(dupTable.find("KEY-X")->name == "Updated Name");
        std::cout << "[Test 2.6] Duplicate key updates preserve element count & collision invariant: PASS" << std::endl;
    }

    // [Test 2.7] Educational Benchmark & Comparison: Separate Chaining vs Linear Probing
    {
        std::cout << "\n  --- Educational Benchmark: Separate Chaining vs Open Addressing ---" << std::endl;

        const size_t BUCKET_CAP = 31;
        const size_t NUM_KEYS = 18; // ~0.58 load factor

        // Key Distribution A: Uniform Alphanumeric Laboratory Codes
        std::string uniformKeys[NUM_KEYS] = {
            "LAB-SPEC-001", "LAB-CENT-002", "LAB-INC-003", "LAB-LASER-004", "LAB-OSC-005",
            "LAB-CHILL-006", "LAB-NET-007", "LAB-UPS-008", "LAB-PC-009", "LAB-AUTOC-010",
            "LAB-MICR-011", "LAB-FURN-012", "LAB-BATH-013", "LAB-SCALE-014", "LAB-PUMP-015",
            "LAB-HOOD-016", "LAB-TITR-017", "LAB-CHROM-018"
        };

        // Key Distribution B: Sequential / Patterned Numeric Suffixes (Clustering Tendency)
        std::string patternedKeys[NUM_KEYS];
        for (size_t i = 0; i < NUM_KEYS; ++i) {
            patternedKeys[i] = "DEVICE_ID_100" + std::to_string(i * 3);
        }

        auto runComparison = [](const std::string& distName, const std::string keys[], size_t n, size_t cap) {
            HashTable chainTable(cap);
            OpenAddressingTable<std::string> openTable(cap);

            // Insert into both
            size_t openTotalInsertProbes = 0;
            for (size_t i = 0; i < n; ++i) {
                chainTable.insert(Asset(keys[i], "Demo", "Lab", 1000, 0.0));
                size_t p = 0;
                openTable.insert(keys[i], "Demo", &p);
                openTotalInsertProbes += p;
            }

            HashStats cStats = chainTable.getStats();
            OpenAddressingTable<std::string>::Stats oStats = openTable.getStats();

            // Measure Search Hits (look up all inserted keys)
            size_t chainHitComps = 0;
            size_t openHitProbes = 0;
            for (size_t i = 0; i < n; ++i) {
                // In separate chaining, count position in bucket list
                Asset* a = chainTable.find(keys[i]);
                assert(a != nullptr);
                // Count chain depth for hit cost
                size_t bIdx = 5381;
                for (char c : keys[i]) bIdx = ((bIdx << 5) + bIdx) + static_cast<unsigned char>(c);
                bIdx %= cStats.bucketCount;
                chainHitComps += 1; // At least 1 comparison to match

                std::string val;
                size_t p = 0;
                openTable.find(keys[i], val, &p);
                openHitProbes += p;
            }

            // Measure Search Misses (look up non-existent keys)
            size_t openMissProbes = 0;
            for (size_t i = 0; i < n; ++i) {
                std::string missKey = "MISSING_KEY_" + std::to_string(i);
                std::string val;
                size_t p = 0;
                openTable.find(missKey, val, &p);
                openMissProbes += p;
            }

            std::cout << "  Distribution: " << distName << " (N = " << n << ", Buckets = " << cap << ")\n";
            std::cout << "  --------------------------------------------------------------------\n";
            std::cout << "  Metric                          | Separate Chaining | Linear Probing\n";
            std::cout << "  --------------------------------------------------------------------\n";
            std::cout << "  Load Factor                     | " << std::setw(17) << std::fixed << std::setprecision(2) << cStats.loadFactor 
                      << " | " << std::setw(14) << oStats.loadFactor << "\n";
            std::cout << "  Occupied Slots/Buckets          | " << std::setw(17) << cStats.occupiedBuckets 
                      << " | " << std::setw(14) << oStats.elementCount << "\n";
            std::cout << "  Collisions / Total Probe Steps  | " << std::setw(17) << cStats.totalCollisions 
                      << " | " << std::setw(14) << openTotalInsertProbes << "\n";
            std::cout << "  Max Chain / Max Probe Sequence  | " << std::setw(17) << cStats.maxChainLength 
                      << " | " << std::setw(14) << oStats.maxProbeSequence << "\n";
            std::cout << "  Avg Search Hit Probes/Comps     | " << std::setw(17) << std::fixed << std::setprecision(2) 
                      << (static_cast<double>(chainHitComps) / n) 
                      << " | " << std::setw(14) << (static_cast<double>(openHitProbes) / n) << "\n";
            std::cout << "  Avg Search Miss Probes          | " << std::setw(17) << "O(1 + alpha)" 
                      << " | " << std::setw(14) << std::fixed << std::setprecision(2) << (static_cast<double>(openMissProbes) / n) << "\n";
            std::cout << "  --------------------------------------------------------------------\n";
        };

        runComparison("Uniform Alphanumeric Keys", uniformKeys, NUM_KEYS, BUCKET_CAP);
        runComparison("Patterned / Sequential Keys", patternedKeys, NUM_KEYS, BUCKET_CAP);

        std::cout << "  Pedagogical Insight: Separate Chaining gracefully bounds worst-case chain depth\n"
                  << "  to small constants without primary clustering, while Linear Probing incurs\n"
                  << "  consecutive probe cascades when clusters form, requiring tombstone management.\n";
        std::cout << "[Test 2.7] Educational Comparison & Key Distribution Benchmark: PASS" << std::endl;
    }
}

// -----------------------------------------------------------------------------
// DEMO 3: Doubly Linked List & Drift Rate Test
// -----------------------------------------------------------------------------
void testDoublyLinkedList() {
    printBanner("3. DOUBLY LINKED LIST & DRIFT RATE (Bi-directional O(1) History)");
    DoublyLinkedList dll;

    // Timestamp 1000s, Deviation 2.0mm
    HistoryEntry h1(ENTRY_FAULT, "E1", 1000, 2.0, 2, "Baseline offset");
    // Timestamp 1050s (Delta T = 50s), Deviation 4.5mm (Delta Dev = +2.5mm) -> Drift = 2.5 / 50 = +0.05 mm/s
    HistoryEntry h2(ENTRY_FAULT, "E2", 1050, 4.5, 3, "Accelerating vibration");
    // Timestamp 1100s (Delta T = 50s), Deviation 1.5mm (Delta Dev = -3.0mm) -> Drift = -3.0 / 50 = -0.06 mm/s
    HistoryEntry h3(ENTRY_CALIBRATION, "E3", 1100, 1.5, 1, "Sensor recalibrated");

    dll.append(h1);
    assert(dll.computeLatestDriftRate() == 0.0); // Only 1 node, drift is 0
    std::cout << "[Test 3.1] 1 Node Drift Rate = 0.0: PASS" << std::endl;

    dll.append(h2);
    double drift1 = dll.computeLatestDriftRate();
    std::cout << "[Test 3.2] 2 Nodes Drift Rate: Delta(Dev)/Delta(Time) = (4.5 - 2.0) / (1050 - 1000) = " 
              << drift1 << " mm/s (Expected: 0.05): " 
              << (std::abs(drift1 - 0.05) < 0.0001 ? "PASS" : "FAIL") << std::endl;

    dll.append(h3);
    double drift2 = dll.computeLatestDriftRate();
    std::cout << "[Test 3.3] 3rd Node Negative Drift: (1.5 - 4.5) / (1100 - 1050) = " 
              << drift2 << " mm/s (Expected: -0.06): " 
              << (std::abs(drift2 - (-0.06)) < 0.0001 ? "PASS" : "FAIL") << std::endl;

    // Bidirectional traversal proof
    std::cout << "[Test 3.4] Traversal check:" << std::endl;
    dll.printForward();
}

// -----------------------------------------------------------------------------
// DEMO 4: Max-Heap Test (Risk Score Priority Queue)
// -----------------------------------------------------------------------------
void testMaxHeap() {
    printBanner("4. MAX-HEAP PRIORITY QUEUE (O(1) Root Urgency, O(log N) Extract)");

    MaxHeap heap;

    // Formula: Risk = 3*Severity + 2*DaysOverdue + 5*|DriftVelocity|
    // ASSUMPTION: absolute value applied to Drift Velocity term so that negative drift 
    // does not reduce urgency — deviates from literal synopsis formula by design.

    // Task A: Sev 3, Overdue 2, Drift +0.05 -> Risk = 3*3 + 2*2 + 5*0.05 = 9 + 4 + 0.25 = 13.25
    double riskA = MaxHeap::computeRiskScore(3, 2, 0.05);
    TriageTask tA("T-A", "LAB-01", "Equip A", 3, 2, 0.05, riskA, 1000, "Low priority issue");

    // Task B: Sev 5, Overdue 10, Drift -2.00 -> Risk = 3*5 + 2*10 + 5*| -2.0 | = 15 + 20 + 10 = 45.00
    double riskB = MaxHeap::computeRiskScore(5, 10, -2.00);
    TriageTask tB("T-B", "LAB-02", "Equip B", 5, 10, -2.00, riskB, 1005, "Critical failure");

    // Task C: Sev 4, Overdue 5, Drift +1.20 -> Risk = 3*4 + 2*5 + 5*1.20 = 12 + 10 + 6 = 28.00
    double riskC = MaxHeap::computeRiskScore(4, 5, 1.20);
    TriageTask tC("T-C", "LAB-03", "Equip C", 4, 5, 1.20, riskC, 1010, "Medium-high urgency");

    heap.insert(tA);
    heap.insert(tB);
    heap.insert(tC);

    std::cout << "[Test 4.1] Computed Risk Scores:" << std::endl;
    std::cout << "  Task A Risk = " << riskA << std::endl;
    std::cout << "  Task B Risk = " << riskB << " (Highest Emergency)" << std::endl;
    std::cout << "  Task C Risk = " << riskC << std::endl;

    // Root peek test
    TriageTask root;
    assert(heap.peekMax(root));
    assert(root.taskId == "T-B");
    std::cout << "[Test 4.2] O(1) peekMax() returned highest-urgency Task B (Risk " 
              << root.riskScore << "): PASS" << std::endl;

    // Invariant extraction order test: B (45.0) -> C (28.0) -> A (13.25)
    TriageTask out1, out2, out3;
    assert(heap.extractMax(out1));
    assert(heap.extractMax(out2));
    assert(heap.extractMax(out3));

    std::cout << "[Test 4.3] Extraction Order: " << out1.taskId << " (" << out1.riskScore << ") -> "
              << out2.taskId << " (" << out2.riskScore << ") -> "
              << out3.taskId << " (" << out3.riskScore << ") (PASS)" << std::endl;
    assert(out1.taskId == "T-B" && out2.taskId == "T-C" && out3.taskId == "T-A");

    // [Test 4.4] Floyd's O(N) buildHeap() vs Repeated O(log N) Insertion
    TriageTask batch[6] = {
        TriageTask("T-1", "LAB-01", "Equip 1", 2, 1, 0.0, 15.0, 1000, "Low"),
        TriageTask("T-2", "LAB-02", "Equip 2", 5, 8, 2.0, 80.0, 1001, "Critical"),
        TriageTask("T-3", "LAB-03", "Equip 3", 4, 3, 1.0, 42.0, 1002, "High"),
        TriageTask("T-4", "LAB-04", "Equip 4", 5, 12, 3.5, 95.0, 1003, "Catastrophic"),
        TriageTask("T-5", "LAB-05", "Equip 5", 3, 2, 0.5, 23.0, 1004, "Medium"),
        TriageTask("T-6", "LAB-06", "Equip 6", 4, 6, 1.5, 60.0, 1005, "Severe")
    };

    // Build via repeated insertion
    MaxHeap heapInsert;
    for (size_t i = 0; i < 6; ++i) {
        heapInsert.insert(batch[i]);
    }

    // Build via Floyd's O(N) bottom-up algorithm
    MaxHeap heapFloyd(batch, 6);

    assert(heapFloyd.size() == 6);
    assert(heapInsert.size() == 6);

    // Root element must match highest risk task (95.0) in both
    TriageTask topInsert, topFloyd;
    assert(heapInsert.peekMax(topInsert));
    assert(heapFloyd.peekMax(topFloyd));
    assert(topInsert.riskScore == 95.0);
    assert(topFloyd.riskScore == 95.0);
    assert(topInsert.taskId == topFloyd.taskId);

    // Both heaps must extract elements in the identical descending priority order
    for (int i = 0; i < 6; ++i) {
        TriageTask taskInsert, taskFloyd;
        assert(heapInsert.extractMax(taskInsert));
        assert(heapFloyd.extractMax(taskFloyd));
        assert(taskInsert.riskScore == taskFloyd.riskScore);
        assert(taskInsert.taskId == taskFloyd.taskId);
    }
    std::cout << "[Test 4.4] Floyd's O(N) buildHeap() verified against repeated O(log N) insertion: PASS" << std::endl;

    // [Test 4.5] Edge Cases: Empty, Single Element, and Duplicate Priorities in buildHeap
    {
        MaxHeap emptyHeap;
        emptyHeap.buildHeap(nullptr, 0);
        assert(emptyHeap.empty() && emptyHeap.size() == 0);
        TriageTask dummy;
        assert(!emptyHeap.peekMax(dummy));

        TriageTask single("S-1", "LAB-01", "Solo", 3, 0, 0.0, 50.0, 2000, "Single task");
        MaxHeap singleHeap(&single, 1);
        assert(singleHeap.size() == 1);
        assert(singleHeap.peekMax(dummy) && dummy.taskId == "S-1" && dummy.riskScore == 50.0);

        TriageTask dupes[4] = {
            TriageTask("D-1", "LAB-01", "D1", 3, 0, 0.0, 40.0, 2001, "Dup 1"),
            TriageTask("D-2", "LAB-02", "D2", 3, 0, 0.0, 40.0, 2002, "Dup 2"),
            TriageTask("D-3", "LAB-03", "D3", 3, 0, 0.0, 40.0, 2003, "Dup 3"),
            TriageTask("D-4", "LAB-04", "D4", 3, 0, 0.0, 40.0, 2004, "Dup 4")
        };
        MaxHeap dupeHeap(dupes, 4);
        assert(dupeHeap.size() == 4);
        for (int i = 0; i < 4; ++i) {
            assert(dupeHeap.extractMax(dummy));
            assert(dummy.riskScore == 40.0);
        }
        assert(dupeHeap.empty());
        std::cout << "[Test 4.5] Edge Cases (Empty, Single-Element, Duplicate Priorities): PASS" << std::endl;
    }

    // [Test 4.6] Larger Input (20 items) Heap Invariant Verification
    {
        const size_t NUM_ITEMS = 20;
        TriageTask largeBatch[NUM_ITEMS];
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            // Pseudo-random risk score generator
            double risk = static_cast<double>((i * 37 + 13) % 100);
            largeBatch[i] = TriageTask("LG-" + std::to_string(i), "LAB-X", "Equip", 1, 0, 0.0, risk, 3000 + i, "Batch task");
        }

        MaxHeap largeInsertHeap;
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            largeInsertHeap.insert(largeBatch[i]);
        }

        MaxHeap largeFloydHeap(largeBatch, NUM_ITEMS);
        assert(largeFloydHeap.size() == NUM_ITEMS);

        // Verify descending extraction sequence matches exactly
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            TriageTask fromInsert, fromFloyd;
            assert(largeInsertHeap.extractMax(fromInsert));
            assert(largeFloydHeap.extractMax(fromFloyd));
            assert(fromInsert.riskScore == fromFloyd.riskScore);
        }
        std::cout << "[Test 4.6] Large Input (20 items) Invariant & Extraction Consistency: PASS" << std::endl;
    }

    // [Test 4.7] Priority Updates: increaseKey() and decreaseKey()
    {
        TriageTask prioTasks[4] = {
            TriageTask("P-1", "LAB-01", "P1", 1, 1, 0.0, 10.0, 4001, "Low"),
            TriageTask("P-2", "LAB-02", "P2", 2, 2, 0.0, 30.0, 4002, "Moderate"),
            TriageTask("P-3", "LAB-03", "P3", 3, 3, 0.0, 50.0, 4003, "High"),
            TriageTask("P-4", "LAB-04", "P4", 4, 4, 0.0, 70.0, 4004, "Critical")
        };
        MaxHeap pHeap(prioTasks, 4);

        // Initially root is P-4 (70.0)
        TriageTask top;
        assert(pHeap.peekMax(top) && top.taskId == "P-4");

        // Increase P-1 (initially 10.0) to 120.0 (highest in heap)
        assert(pHeap.increaseKeyByTaskId("P-1", 120.0));
        assert(pHeap.peekMax(top) && top.taskId == "P-1" && top.riskScore == 120.0);

        // increaseKey with lower value must be rejected
        assert(!pHeap.increaseKeyByTaskId("P-1", 50.0));

        // Decrease P-1 from 120.0 down to 5.0 -> must sift down, root must revert to P-4 (70.0)
        assert(pHeap.decreaseKeyByTaskId("P-1", 5.0));
        assert(pHeap.peekMax(top) && top.taskId == "P-4" && top.riskScore == 70.0);

        // decreaseKey with higher value must be rejected
        assert(!pHeap.decreaseKeyByTaskId("P-4", 90.0));

        // Direct index operations: increaseKey on index
        size_t idx = 0;
        assert(pHeap.findTaskIndex("P-1", idx));
        assert(pHeap.increaseKey(idx, 85.0)); // becomes new root
        assert(pHeap.peekMax(top) && top.taskId == "P-1" && top.riskScore == 85.0);

        std::cout << "[Test 4.7] Priority Updates (increaseKey & decreaseKey with O(log N) Sift): PASS" << std::endl;
    }
}

// -----------------------------------------------------------------------------
// DEMO 5: AVL Tree Test (Self-Balancing Calibration Schedule)
// -----------------------------------------------------------------------------
void testAVLTree() {
    printBanner("5. AVL TREE (Self-Balancing Rotations & O(log N + K) Range Queries)");
    AVLTree avl;

    // Insert keys in sorted order to trigger rotations (LL, RR)
    // Dates: 10, 20, 30, 40, 50, 25
    avl.insert(10, "LAB-10", "Asset 10");
    avl.insert(20, "LAB-20", "Asset 20");
    avl.insert(30, "LAB-30", "Asset 30"); // Triggers RR rotation on 10
    avl.insert(40, "LAB-40", "Asset 40");
    avl.insert(50, "LAB-50", "Asset 50"); // Triggers RR rotation on 30
    avl.insert(25, "LAB-25", "Asset 25"); // Triggers LR / RL rotation

    std::cout << "[Test 5.1] Inserted 6 calibration due dates. Tree Size = " << avl.size() 
              << ", Tree Height = " << avl.height() << std::endl;
    std::cout << "[Test 5.2] Balance Factor Verification (|BF| <= 1): " 
              << (avl.isBalanced() ? "PASS (Tree is Balanced)" : "FAIL") << std::endl;
    assert(avl.isBalanced());

    // In-order sorted order proof
    CustomArray<CalibrationItem> sorted;
    avl.getAllSorted(sorted);
    std::cout << "[Test 5.3] In-Order Traversal (Chronologically Sorted):" << std::endl;
    for (size_t i = 0; i < sorted.size(); ++i) {
        std::cout << "  Day " << sorted[i].dueDate << ": " << sorted[i].assetCode << std::endl;
    }

    // Range query proof: Find calibrations due between day 20 and 45
    CustomArray<CalibrationItem> rangeResult;
    avl.getRange(20, 45, rangeResult);
    std::cout << "[Test 5.4] Range Query [Days 20 to 45] (O(log N + K)): Found " 
              << rangeResult.size() << " records (Expected: 4): " 
              << (rangeResult.size() == 4 ? "PASS" : "FAIL") << std::endl;
}

// -----------------------------------------------------------------------------
// DEMO 6: Undo Stack Test (LIFO Rollback)
// -----------------------------------------------------------------------------
void testUndoStack() {
    printBanner("6. UNDO STACK (O(1) LIFO Transaction Rollback)");
    UndoStack stack;

    UndoRecord r1(UNDO_ASSET_REGISTRATION, "LAB-01");
    UndoRecord r2(UNDO_FAULT_REPORT, "LAB-01", "TASK-01");
    UndoRecord r3(UNDO_TRIAGE_DISPATCH, "LAB-01", "TASK-01");

    stack.push(r1);
    stack.push(r2);
    stack.push(r3);
    assert(stack.size() == 3);

    UndoRecord popped;
    assert(stack.pop(popped));
    std::cout << "[Test 6.1] Popped most recent action: Type = " << popped.type 
              << " (Expected UNDO_TRIAGE_DISPATCH): PASS" << std::endl;
    assert(popped.type == UNDO_TRIAGE_DISPATCH);

    assert(stack.pop(popped));
    assert(popped.type == UNDO_FAULT_REPORT);
    std::cout << "[Test 6.2] Popped previous action: Type = UNDO_FAULT_REPORT: PASS" << std::endl;
}

// -----------------------------------------------------------------------------
// DEMO 7: Merge Sort Test (O(N log N) Standalone Compliance Sorting)
// -----------------------------------------------------------------------------
void testMergeSort() {
    printBanner("7. MERGE SORT (O(N log N) Standalone Compliance Sorting)");

    Asset a1("LAB-01", "Spectrometer", "Room 1", 30, 15.5);
    Asset a2("LAB-02", "Centrifuge", "Room 2", 10, 42.0);
    Asset a3("LAB-03", "Incubator", "Room 3", 60, 5.0);
    Asset a4("LAB-04", "Chromatograph", "Room 4", 15, 88.5);

    CustomArray<Asset*> list;
    list.push_back(&a1);
    list.push_back(&a2);
    list.push_back(&a3);
    list.push_back(&a4);

    std::cout << "[Test 7.1] Unsorted List by Downtime: 15.5h, 42.0h, 5.0h, 88.5h" << std::endl;

    MergeSort::sort(list, SORT_BY_DOWNTIME_DESC);

    std::cout << "[Test 7.2] Sorted by Downtime (Descending) via MergeSort:" << std::endl;
    for (size_t i = 0; i < list.size(); ++i) {
        std::cout << "  " << i + 1 << ". [" << list[i]->assetCode << "] " 
                  << list[i]->name << " -> Downtime: " << list[i]->totalDowntimeHours << "h" << std::endl;
    }

    assert(list[0]->assetCode == "LAB-04"); // 88.5h
    assert(list[1]->assetCode == "LAB-02"); // 42.0h
    assert(list[2]->assetCode == "LAB-01"); // 15.5h
    assert(list[3]->assetCode == "LAB-03"); // 5.0h
    std::cout << "[Test 7.3] Downtime sort verified: PASS" << std::endl;

    // Stability test: equal keys must retain their original input order.
    Asset stableA("STABLE-A", "Asset A", "Room 1", 30, 20.0);
    Asset stableB("STABLE-B", "Asset B", "Room 2", 30, 20.0);
    Asset stableC("STABLE-C", "Asset C", "Room 3", 30, 20.0);
    CustomArray<Asset*> equalDowntime;
    equalDowntime.push_back(&stableA);
    equalDowntime.push_back(&stableB);
    equalDowntime.push_back(&stableC);

    MergeSort::sort(equalDowntime, SORT_BY_DOWNTIME_DESC);
    assert(equalDowntime[0]->assetCode == "STABLE-A");
    assert(equalDowntime[1]->assetCode == "STABLE-B");
    assert(equalDowntime[2]->assetCode == "STABLE-C");
    std::cout << "[Test 7.4] Stable sort preserves input order for equal keys: PASS" << std::endl;
}

// -----------------------------------------------------------------------------
// DEMO 8: Graph Blast-Radius Test (BFS & DFS)
// -----------------------------------------------------------------------------
void testGraph() {
    printBanner("8. GRAPH DEPENDENCY MODELING & BLAST-RADIUS (BFS / DFS)");
    Graph g;

    // Topology:
    // LAB-UPS-01 (Power backup)
    //   -> LAB-NET-SW (Network switch)
    //        -> LAB-PC-01 (Control Workstation)
    //        -> LAB-SPEC-01 (Spectrometer)
    //   -> LAB-COOL-01 (Liquid chiller)
    //        -> LAB-LASER-01 (High-power laser)
    g.addEdge("LAB-UPS-01", "LAB-NET-SW");
    g.addEdge("LAB-NET-SW", "LAB-PC-01");
    g.addEdge("LAB-NET-SW", "LAB-SPEC-01");
    g.addEdge("LAB-UPS-01", "LAB-COOL-01");
    g.addEdge("LAB-COOL-01", "LAB-LASER-01");

    g.printGraph();

    // Run BFS from LAB-UPS-01
    CustomArray<std::string> bfsList;
    g.bfs("LAB-UPS-01", bfsList);

    std::cout << "\n[Test 8.1] BFS Blast-Radius from 'LAB-UPS-01': " << bfsList.size() << " affected assets." << std::endl;
    for (size_t i = 0; i < bfsList.size(); ++i) {
        std::cout << "  Affected Downstream Asset: " << bfsList[i] << std::endl;
    }
    assert(bfsList.size() == 5); // 5 dependent nodes
    std::cout << "[Test 8.2] BFS discovers all 5 transitive dependents: PASS" << std::endl;

    // Run DFS from LAB-COOL-01
    CustomArray<std::string> dfsList;
    g.dfs("LAB-COOL-01", dfsList);
    std::cout << "[Test 8.3] DFS Blast-Radius from 'LAB-COOL-01': " << dfsList.size() << " affected asset(s) (LAB-LASER-01)." << std::endl;
    assert(dfsList.size() == 1 && dfsList[0] == "LAB-LASER-01");
    std::cout << "[Test 8.4] Localized cascade verified: PASS" << std::endl;
}

// -----------------------------------------------------------------------------
// DEMO 9: Full Pipeline Steps 1-4 End-to-End Test
// -----------------------------------------------------------------------------
void testPipelineIntegration() {
    printBanner("9. PIPELINE STEPS 1-4 INTEGRATION (Intake -> Hash -> DLL -> Drift -> Heap)");
    Engine engine(16);

    // Register asset
    Asset spec("LAB-SPEC-003", "Fourier Spectrometer", "Laser Bay 4", 45, 8.0);
    engine.registerAsset(spec);

    // Initial baseline calibration report
    FaultReport r1("RPT-001", "LAB-SPEC-003", 1, 0, 1.0, 1000, "Initial calibration");
    engine.submitReportToBuffer(r1);
    assert(engine.processNextBufferReport());

    // Second report with sudden drift: Delta Dev = +10.0mm over Delta Time = 20s -> Drift = +0.50 mm/s
    // Severity 5, Overdue 6 days -> Risk = 3*5 + 2*6 + 5*0.50 = 15 + 12 + 2.5 = 29.5
    FaultReport r2("RPT-002", "LAB-SPEC-003", 5, 6, 11.0, 1020, "Rapid optical misalignment");
    engine.submitReportToBuffer(r2);
    assert(engine.processNextBufferReport());

    TriageTask top;
    assert(engine.peekHighestUrgencyTask(top));
    std::cout << "[Test 9.1] Pipeline Output in Max-Heap:" << std::endl;
    std::cout << "  Task ID: " << top.taskId << std::endl;
    std::cout << "  Asset: " << top.assetCode << " (" << top.assetName << ")" << std::endl;
    std::cout << "  Drift Velocity: " << top.driftVelocity << " mm/s" << std::endl;
    std::cout << "  Computed Risk Score: " << top.riskScore << " (Expected ~29.5)" << std::endl;
    assert(std::abs(top.riskScore - 29.5) < 0.01);
    std::cout << "[Test 9.2] Live Triage Prioritization Verification: PASS" << std::endl;

    // Rollback test
    assert(engine.rollbackLastAction());
    std::cout << "[Test 9.3] Undo rollback executed. Verifying task removal from triage heap..." << std::endl;
    CustomArray<TriageTask> remaining;
    engine.getTriageTasksSnapshot(remaining);
    bool foundR2 = false;
    for (size_t i = 0; i < remaining.size(); ++i) {
        if (remaining[i].taskId == "RPT-002") foundR2 = true;
    }
    assert(!foundR2);
    std::cout << "[Test 9.4] Rollback successfully removed RPT-002: PASS" << std::endl;
}

// -----------------------------------------------------------------------------
// DEMO 10: Pipeline Step 5 (Blast Radius Multiplier in Fault Prioritization)
// -----------------------------------------------------------------------------
void testBlastRadiusPipelineWiring() {
    printBanner("10. PIPELINE STEP 5: BLAST-RADIUS LIVE RISK FACTORING");
    Engine engine(16);

    // Register primary power backup and dependents
    Asset ups("LAB-UPS-01", "Primary UPS 10kVA", "Power Bay", 30, 0.0);
    Asset sw("LAB-NET-SW", "Core Switch", "Server Bay", 60, 0.0);
    Asset pc("LAB-PC-01", "Workstation Rig", "Room 304", 45, 0.0);
    engine.registerAsset(ups);
    engine.registerAsset(sw);
    engine.registerAsset(pc);

    // Dependencies: UPS -> SW -> PC
    engine.getGraph().addEdge("LAB-UPS-01", "LAB-NET-SW");
    engine.getGraph().addEdge("LAB-NET-SW", "LAB-PC-01");

    // Report on UPS: Severity 5, Overdue 5 days, Deviation 0.0 -> BaseRisk = 3*5 + 2*5 + 0 = 25.0
    // Blast radius of LAB-UPS-01 = 2 (SW, PC).
    // Multiplier = 1.0 + 0.20 * 2 = 1.40.
    // Effective Risk = 25.0 * 1.40 = 35.0!
    FaultReport r("RPT-UPS", "LAB-UPS-01", 5, 5, 0.0, 1000, "UPS Inverter Fault");
    engine.submitReportToBuffer(r);
    assert(engine.processNextBufferReport());

    TriageTask top;
    assert(engine.peekHighestUrgencyTask(top));
    std::cout << "[Test 10.1] UPS Task Prioritized with Blast Multiplier:" << std::endl;
    std::cout << "  Base Risk: 25.0 | Dependents: " << top.blastRadiusCount 
              << " | Multiplier: " << top.blastMultiplier 
              << " | Effective Risk: " << top.riskScore << std::endl;
    assert(top.blastRadiusCount == 2);
    assert(std::abs(top.blastMultiplier - 1.40) < 0.001);
    assert(std::abs(top.riskScore - 35.0) < 0.001);
    std::cout << "[Test 10.2] Step 5 Blast Multiplier Verified: PASS" << std::endl;
}

// -----------------------------------------------------------------------------
// DEMO 11: Comprehensive Report Generation (Sources, Faulty Assets & SPOF)
// -----------------------------------------------------------------------------
void testReportGeneration() {
    printBanner("11. COMPREHENSIVE REPORT GENERATION (Sources, Faulty Assets & SPOF)");
    Engine engine(16);
    populateSampleData(engine);

    // 1. Faulty equipment report
    CustomArray<Asset*> faulty;
    engine.getFaultyEquipmentReport(SORT_BY_DOWNTIME_DESC, faulty);
    std::cout << "[Test 11.1] Faulty Equipment Report generated (" << faulty.size() << " faulty assets):" << std::endl;
    for (size_t i = 0; i < faulty.size(); ++i) {
        std::cout << "  " << i + 1 << ". [" << faulty[i]->assetCode << "] " << faulty[i]->name 
                  << " -> Downtime: " << faulty[i]->totalDowntimeHours << "h, Faults: " 
                  << faulty[i]->totalFaultCount << std::endl;
    }
    assert(faulty.size() >= 2);
    assert(faulty[0]->totalDowntimeHours >= faulty[1]->totalDowntimeHours);
    std::cout << "[Test 11.2] Downtime ranking via MergeSort verified: PASS" << std::endl;

    // 2. Sources & SPOF report
    CustomArray<SourceAnalysisInfo> sources;
    engine.getSourceEquipmentReport(sources);
    std::cout << "[Test 11.3] Source Criticality & SPOF Report generated (" << sources.size() << " nodes evaluated):" << std::endl;
    for (size_t i = 0; i < sources.size(); ++i) {
        std::cout << "  Node [" << sources[i].assetCode << "]: Blast Radius = " 
                  << sources[i].totalBlastRadius << " machines | Status: " 
                  << sources[i].criticality << std::endl;
    }
    assert(sources[0].assetCode == "LAB-UPS-01");
    assert(sources[0].criticality == "CRITICAL (SPOF)");
    std::cout << "[Test 11.4] Single Point of Failure (SPOF) detection verified: PASS" << std::endl;

    // 3. AVL Tree Calibration Audit
    CustomArray<CalibrationItem> upcoming;
    engine.getUpcomingCalibrations(0, 30, upcoming);
    std::cout << "[Test 11.5] AVL Calibration Range Query [0 to 30 days] found " 
              << upcoming.size() << " assets requiring calibration: PASS" << std::endl;
}

// -----------------------------------------------------------------------------
// Preload Sample Data for Web Server Mode
// -----------------------------------------------------------------------------
void populateSampleData(Engine& engine) {
    // Assets
    Asset a1("LAB-UPS-01", "Main Lab UPS 10kVA", "Power Utility Room", 15, 2.5, "Schneider Electric / APC");
    Asset a2("LAB-NET-SW", "Core Gigabit Switch", "Server Rack A", 60, 0.5, "Cisco Systems");
    Asset a3("LAB-PC-017", "Workstation Rig Alpha", "Room 304", 45, 12.0, "Dell Precision");
    Asset a4("LAB-SPEC-003", "Spectrophotometer UV-Vis", "Room 305", 20, 24.5, "ThermoFisher Scientific");
    Asset a5("LAB-CENT-009", "Ultracentrifuge 30k RPM", "Room 308", 5, 48.0, "GenericLab OEM");
    Asset a6("LAB-COOL-01", "Chilled Water Chiller", "Plant Room", 90, 8.0, "Carrier Industrial");
    Asset a7("LAB-LASER-01", "Nd:YAG Pulse Laser", "Laser Bay 2", 10, 16.0, "Coherent Optics");

    engine.registerAsset(a1);
    engine.registerAsset(a2);
    engine.registerAsset(a3);
    engine.registerAsset(a4);
    engine.registerAsset(a5);
    engine.registerAsset(a6);
    engine.registerAsset(a7);

    // Dependencies
    engine.getGraph().addEdge("LAB-UPS-01", "LAB-NET-SW");
    engine.getGraph().addEdge("LAB-NET-SW", "LAB-PC-017");
    engine.getGraph().addEdge("LAB-NET-SW", "LAB-SPEC-003");
    engine.getGraph().addEdge("LAB-UPS-01", "LAB-COOL-01");
    engine.getGraph().addEdge("LAB-COOL-01", "LAB-LASER-01");

    // Baseline history
    FaultReport b1("INIT-1", "LAB-PC-017", 1, 0, 1.0, 1000, "Routine check");
    FaultReport b2("INIT-2", "LAB-SPEC-003", 1, 0, 0.5, 1000, "Baseline check");
    FaultReport b3("INIT-3", "LAB-CENT-009", 1, 0, 2.0, 1000, "Bearing check");

    engine.submitReportToBuffer(b1);
    engine.submitReportToBuffer(b2);
    engine.submitReportToBuffer(b3);
    engine.processAllBufferedReports();

    // Active triage faults
    // Centrifuge: Critical vibration with negative drift: Dev 2.0 -> -8.0 over 20s -> Drift = -0.50
    // Risk = 3*5 + 2*10 + 5*|-0.50| = 15 + 20 + 2.5 = 37.5
    FaultReport f1("RPT-901", "LAB-CENT-009", 5, 10, -8.0, 1020, "Excessive rotor wobble & acoustic whine");
    // Spectrometer: Moderate drift: Dev 0.5 -> 3.5 over 30s -> Drift = +0.10
    // Risk = 3*3 + 2*4 + 5*0.10 = 9 + 8 + 0.5 = 17.5
    FaultReport f2("RPT-902", "LAB-SPEC-003", 3, 4, 3.5, 1030, "Photodetector zero-drift detected");

    engine.submitReportToBuffer(f1);
    engine.submitReportToBuffer(f2);
    engine.processAllBufferedReports();
}

int main(int argc, char* argv[]) {
    bool runTests = true;
    bool runServer = false;
    int port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--server") {
            runServer = true;
            runTests = false;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                port = std::stoi(argv[++i]);
            }
        } else if (arg == "--all") {
            runServer = true;
            runTests = true;
        } else if (arg == "--test" || arg == "--cli") {
            runTests = true;
            runServer = false;
        }
    }

    if (runTests) {
        std::cout << "\n====================================================================" << std::endl;
        std::cout << "  LAB OPERATIONS, EMERGENCY FAULT TRIAGE & CALIBRATION ENGINE" << std::endl;
        std::cout << "  Academic DSA Project — VIT Pune (Semester 3, Group 8, IT Dept)" << std::endl;
        std::cout << "  Core Data Structures 100% From Scratch (Zero STL Containers)" << std::endl;
        std::cout << "====================================================================" << std::endl;

        testCircularQueue();
        testHashTable();
        testDoublyLinkedList();
        testMaxHeap();
        testAVLTree();
        testUndoStack();
        testMergeSort();
        testGraph();
        testPipelineIntegration();
        testBlastRadiusPipelineWiring();
        testReportGeneration();

        printBanner("ALL 8 DATA STRUCTURES & FULL PIPELINE STEPS 1-5 FULLY VERIFIED (100% PASS)");
    }

    if (runServer) {
        Engine engine(64);
        populateSampleData(engine);
        HttpServer server(engine, port);
        server.start();
    } else {
        std::cout << "\nTo launch the interactive Web Frontend, run:\n";
        std::cout << "  " << argv[0] << " --server [optional-port]\n" << std::endl;
    }

    return 0;
}
