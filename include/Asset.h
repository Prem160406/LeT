#ifndef ASSET_H
#define ASSET_H

#include <string>
#include <iostream>

// Forward declaration of custom DoublyLinkedList
class DoublyLinkedList;

/**
 * @brief Categorizes history entries for laboratory equipment.
 */
enum EntryType {
    ENTRY_FAULT = 0,
    ENTRY_CALIBRATION = 1
};

/**
 * @brief Node payload for an asset's interleaved history tracking.
 */
struct HistoryEntry {
    EntryType type;
    std::string entryId;
    long long timestamp;        // Epoch in seconds or sequence time
    double measuredDeviation;    // Physical measurement offset (mm, mV, ppm, degC, etc.)
    int severity;               // Scale 1 (minor) to 5 (critical emergency)
    std::string description;
    double computedDrift;       // Calculated drift rate at this moment

    HistoryEntry();
    HistoryEntry(EntryType t, const std::string& id, long long ts, 
                 double dev, int sev, const std::string& desc, double drift = 0.0);
};

/**
 * @brief Represents incoming service desk intake ticket.
 * Enqueued into CircularQueue before Hash Table resolution.
 */
struct FaultReport {
    std::string reportId;
    std::string assetCode;
    int severity;               // 1-5
    int daysOverdue;            // Calibration/inspection overdue days
    double measuredDeviation;   // Sensor/instrument deviation
    long long timestamp;        // Report logging time
    std::string description;

    FaultReport();
    FaultReport(const std::string& rId, const std::string& aCode, int sev,
                int overdue, double dev, long long ts, const std::string& desc);
};

/**
 * @brief Priority task queued inside the custom Max-Heap.
 */
struct TriageTask {
    std::string taskId;
    std::string assetCode;
    std::string assetName;
    int severity;
    int daysOverdue;
    double driftVelocity;
    double riskScore;
    long long timestamp;
    std::string description;
    int blastRadiusCount;       // Transitive dependents affected (Step 5)
    double blastMultiplier;     // Multiplier applied to base risk (Step 5)

    TriageTask();
    TriageTask(const std::string& tId, const std::string& aCode, const std::string& aName,
               int sev, int overdue, double drift, double risk, long long ts, const std::string& desc,
               int blastCount = 0, double blastMult = 1.0);

    // Comparator for Max-Heap ordering
    bool operator<(const TriageTask& other) const {
        return this->riskScore < other.riskScore;
    }
    bool operator>(const TriageTask& other) const {
        return this->riskScore > other.riskScore;
    }
};

/**
 * @brief Core laboratory asset entity.
 */
class Asset {
public:
    std::string assetCode;          // Primary key (e.g. "LAB-PC-017")
    std::string name;               // e.g. "Centrifuge CF-200"
    std::string location;           // e.g. "Bio-Lab 3B"
    long long nextCalibrationDue;   // Epoch timestamp used by AVL Tree
    int totalFaultCount;
    double totalDowntimeHours;
    DoublyLinkedList* history;      // Custom DLL pointer for history

    Asset();
    Asset(const std::string& code, const std::string& name, const std::string& loc, 
          long long calibDue = 0, double downtime = 0.0);
    ~Asset();

    // Prevent shallow-copy memory errors on custom DLL
    Asset(const Asset& other);
    Asset& operator=(const Asset& other);
    Asset(Asset&& other) noexcept;
    Asset& operator=(Asset&& other) noexcept;

    void printSummary() const;
};

#endif // ASSET_H
