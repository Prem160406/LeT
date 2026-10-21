#include "Asset.h"
#include "DoublyLinkedList.h"
#include <iostream>

HistoryEntry::HistoryEntry()
    : type(ENTRY_FAULT), entryId(""), timestamp(0), measuredDeviation(0.0), 
      severity(1), description(""), computedDrift(0.0) {}

HistoryEntry::HistoryEntry(EntryType t, const std::string& id, long long ts,
                           double dev, int sev, const std::string& desc, double drift)
    : type(t), entryId(id), timestamp(ts), measuredDeviation(dev),
      severity(sev), description(desc), computedDrift(drift) {}

FaultReport::FaultReport()
    : reportId(""), assetCode(""), severity(1), daysOverdue(0), 
      measuredDeviation(0.0), timestamp(0), description("") {}

FaultReport::FaultReport(const std::string& rId, const std::string& aCode, int sev,
                         int overdue, double dev, long long ts, const std::string& desc)
    : reportId(rId), assetCode(aCode), severity(sev), daysOverdue(overdue),
      measuredDeviation(dev), timestamp(ts), description(desc) {}

TriageTask::TriageTask()
    : taskId(""), assetCode(""), assetName(""), severity(1), daysOverdue(0),
      driftVelocity(0.0), riskScore(0.0), timestamp(0), description(""),
      blastRadiusCount(0), blastMultiplier(1.0) {}

TriageTask::TriageTask(const std::string& tId, const std::string& aCode, const std::string& aName,
                       int sev, int overdue, double drift, double risk, long long ts, const std::string& desc,
                       int blastCount, double blastMult)
    : taskId(tId), assetCode(aCode), assetName(aName), severity(sev), daysOverdue(overdue),
      driftVelocity(drift), riskScore(risk), timestamp(ts), description(desc),
      blastRadiusCount(blastCount), blastMultiplier(blastMult) {}

Asset::Asset()
    : assetCode(""), name(""), location(""), supplier("Standard Lab Corp"), nextCalibrationDue(0),
      totalFaultCount(0), totalDowntimeHours(0.0) {
    history = new DoublyLinkedList();
}

Asset::Asset(const std::string& code, const std::string& n, const std::string& loc,
             long long calibDue, double downtime, const std::string& supp)
    : assetCode(code), name(n), location(loc), supplier(supp), nextCalibrationDue(calibDue),
      totalFaultCount(0), totalDowntimeHours(downtime) {
    history = new DoublyLinkedList();
}

Asset::~Asset() {
    delete history;
    history = nullptr;
}

Asset::Asset(const Asset& other)
    : assetCode(other.assetCode), name(other.name), location(other.location),
      supplier(other.supplier), nextCalibrationDue(other.nextCalibrationDue),
      totalFaultCount(other.totalFaultCount), totalDowntimeHours(other.totalDowntimeHours) {
    if (other.history != nullptr) {
        history = new DoublyLinkedList(*other.history);
    } else {
        history = new DoublyLinkedList();
    }
}

Asset& Asset::operator=(const Asset& other) {
    if (this != &other) {
        assetCode = other.assetCode;
        name = other.name;
        location = other.location;
        supplier = other.supplier;
        nextCalibrationDue = other.nextCalibrationDue;
        totalFaultCount = other.totalFaultCount;
        totalDowntimeHours = other.totalDowntimeHours;
        delete history;
        if (other.history != nullptr) {
            history = new DoublyLinkedList(*other.history);
        } else {
            history = new DoublyLinkedList();
        }
    }
    return *this;
}

Asset::Asset(Asset&& other) noexcept
    : assetCode(std::move(other.assetCode)), name(std::move(other.name)),
      location(std::move(other.location)), supplier(std::move(other.supplier)),
      nextCalibrationDue(other.nextCalibrationDue), totalFaultCount(other.totalFaultCount),
      totalDowntimeHours(other.totalDowntimeHours), history(other.history) {
    other.history = nullptr;
}

Asset& Asset::operator=(Asset&& other) noexcept {
    if (this != &other) {
        assetCode = std::move(other.assetCode);
        name = std::move(other.name);
        location = std::move(other.location);
        supplier = std::move(other.supplier);
        nextCalibrationDue = other.nextCalibrationDue;
        totalFaultCount = other.totalFaultCount;
        totalDowntimeHours = other.totalDowntimeHours;
        delete history;
        history = other.history;
        other.history = nullptr;
    }
    return *this;
}

void Asset::printSummary() const {
    std::cout << "[" << assetCode << "] " << name 
              << " (" << supplier << ")"
              << " | Loc: " << location 
              << " | CalibDue: " << nextCalibrationDue 
              << " | Faults: " << totalFaultCount 
              << " | Downtime: " << totalDowntimeHours << "h" << std::endl;
}
