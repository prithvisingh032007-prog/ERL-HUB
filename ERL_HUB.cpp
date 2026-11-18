// ERL HUB: Post Earthquake Multi Agency Emergency Relief Logistics Hub
// Subject: Object Oriented Programming Using C++ (CIC 211), Option 2
// Compile with: g++ -std=c++17 ERL_HUB.cpp -o erlhub

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <string>
#include <exception>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

// console colors (ANSI codes). Set USE_COLOR to false if your console shows weird symbols.
const bool USE_COLOR = true;
const char* const RESET  = USE_COLOR ? "\033[0m"  : "";
const char* const RED    = USE_COLOR ? "\033[31m" : "";
const char* const GREEN  = USE_COLOR ? "\033[32m" : "";
const char* const YELLOW = USE_COLOR ? "\033[33m" : "";
const char* const CYAN   = USE_COLOR ? "\033[36m" : "";

namespace ReliefOps {

string toText(double value) {
    ostringstream out;
    out << fixed << setprecision(2) << value;
    return out.str();
}

// ================= UNIT IV: custom exceptions =================

class CargoOverweightException : public exception {
private:
    string message;
public:
    CargoOverweightException(const string& vehicleName, double newLoad, double maxLoad) {
        message = "Whoa, " + vehicleName + " can't carry that much. The load would be "
                + toText(newLoad) + " tons but the limit is " + toText(maxLoad) + " tons.";
    }
    const char* what() const noexcept override { return message.c_str(); }
};

class FuelDepletionException : public exception {
private:
    string message;
public:
    FuelDepletionException(const string& vehicleName, double needed, double left) {
        message = "Not enough fuel for " + vehicleName + ". The trip needs " + toText(needed)
                + " litres and only " + toText(left) + " litres are left in the tank.";
    }
    const char* what() const noexcept override { return message.c_str(); }
};

class DamagedCorridorException : public exception {
private:
    string message;
public:
    DamagedCorridorException(const string& vehicleName) {
        message = "The road ahead is wrecked, so " + vehicleName
                + " can't use it. Pick another vehicle or wait till the corridor is cleared.";
    }
    const char* what() const noexcept override { return message.c_str(); }
};

class OutOfStockException : public exception {
private:
    string message;
public:
    OutOfStockException(const string& depotName, const string& supply, double asked, double have) {
        message = "Depot " + depotName + " doesn't have enough " + supply + ". You asked for "
                + toText(asked) + " tons but only " + toText(have) + " tons are on the shelf.";
    }
    const char* what() const noexcept override { return message.c_str(); }
};

// ================= UNIT I: namespace ReliefOps::Inventory =================
namespace Inventory {

class IncidentCommander;

enum SupplyType { RATION_KIT = 0, TRIAGE_MEDICINE = 1, WATER_TANK = 2, THERMAL_BLANKET = 3 };
const int TOTAL_SUPPLY_TYPES = 4;

inline const char* supplyName(int type) {
    const char* names[TOTAL_SUPPLY_TYPES] = {"Ration Kit", "Triage Medicine", "Water Tank", "Thermal Blanket"};
    if (type < 0 || type >= TOTAL_SUPPLY_TYPES) return "Unknown";
    return names[type];
}

class ReliefCargo {
private:
    int cargoId;
    int supplyType;
    double weightTons;
    int priority;
    char* label;

    friend class IncidentCommander;
    friend ostream& operator<<(ostream& os, const ReliefCargo& c);
    friend istream& operator>>(istream& is, ReliefCargo& c);

    void copyLabel(const char* text) {
        label = new char[strlen(text) + 1];
        strcpy(label, text);
    }
public:
    ReliefCargo() : cargoId(0), supplyType(0), weightTons(0), priority(1) {
        copyLabel("empty");
    }

    ReliefCargo(int id, int type, double weight, int prio, const char* text)
        : cargoId(id), supplyType(type), weightTons(weight), priority(prio) {
        copyLabel(text);
    }

    ReliefCargo(const ReliefCargo& other)
        : cargoId(other.cargoId), supplyType(other.supplyType),
          weightTons(other.weightTons), priority(other.priority) {
        copyLabel(other.label);
    }

    ReliefCargo& operator=(const ReliefCargo& other) {
        if (this != &other) {
            delete[] label;
            cargoId = other.cargoId;
            supplyType = other.supplyType;
            weightTons = other.weightTons;
            priority = other.priority;
            copyLabel(other.label);
        }
        return *this;
    }

    ~ReliefCargo() { delete[] label; }

    int getId() const { return cargoId; }
    int getType() const { return supplyType; }
    double getWeight() const { return weightTons; }
    int getPriority() const { return priority; }
    const char* getLabel() const { return label; }

    bool operator==(const ReliefCargo& other) const {
        return cargoId == other.cargoId && supplyType == other.supplyType
               && strcmp(label, other.label) == 0;
    }
};

ostream& operator<<(ostream& os, const ReliefCargo& c) {
    os << c.cargoId << " " << c.supplyType << " " << c.weightTons << " "
       << c.priority << " " << c.label;
    return os;
}

istream& operator>>(istream& is, ReliefCargo& c) {
    int id, type, prio;
    double weight;
    string text;
    if (is >> id >> type >> weight >> prio >> text) {
        if (type < 0 || type >= TOTAL_SUPPLY_TYPES || prio < 1 || prio > 5 || weight <= 0) {
            is.setstate(ios::failbit);
            return is;
        }
        ReliefCargo temp(id, type, weight, prio, text.c_str());
        c = temp;
    }
    return is;
}

class DistributionDepot {
private:
    string depotName;
    double stock[TOTAL_SUPPLY_TYPES];
    friend class IncidentCommander;
public:
    DistributionDepot(const string& name = "Unnamed") : depotName(name) {
        for (int i = 0; i < TOTAL_SUPPLY_TYPES; i++) stock[i] = 0;
    }

    const string& getName() const { return depotName; }

    double getStock(int type) const {
        if (type < 0 || type >= TOTAL_SUPPLY_TYPES) return 0;
        return stock[type];
    }

    double& stockRef(int type) {
        if (type < 0 || type >= TOTAL_SUPPLY_TYPES) type = 0;
        return stock[type];
    }

    DistributionDepot& operator+=(const ReliefCargo& cargo) {
        stock[cargo.getType()] += cargo.getWeight();
        return *this;
    }

    DistributionDepot& operator-=(const ReliefCargo& cargo) {
        int type = cargo.getType();
        if (stock[type] < cargo.getWeight()) {
            throw OutOfStockException(depotName, supplyName(type), cargo.getWeight(), stock[type]);
        }
        stock[type] -= cargo.getWeight();
        return *this;
    }

    void showStock() const {
        cout << CYAN << "Depot: " << depotName << RESET << "\n";
        for (int i = 0; i < TOTAL_SUPPLY_TYPES; i++) {
            int barLength = static_cast<int>(stock[i] / 2);
            if (barLength > 25) barLength = 25;
            const char* barColor = GREEN;
            if (stock[i] < 3) barColor = YELLOW;
            if (stock[i] <= 0) barColor = RED;
            cout << "  " << left << setw(18) << supplyName(i) << right << setw(8) << stock[i] << " t  "
                 << barColor << string(barLength, '#') << RESET << "\n";
        }
        cout << left;
    }
};

class IncidentCommander {
public:
    void forcePriority(ReliefCargo& cargo) {
        cargo.priority = 5;
    }

    void emergencyPull(DistributionDepot& depot, const ReliefCargo& cargo) {
        double& shelf = depot.stock[cargo.supplyType];
        shelf = shelf - cargo.weightTons;
        if (shelf < 0) shelf = 0;
    }

    void inspect(const DistributionDepot& depot) const {
        cout << "Commander raw view of " << depot.depotName << ": ";
        for (int i = 0; i < TOTAL_SUPPLY_TYPES; i++) cout << depot.stock[i] << " ";
        cout << "\n";
    }
};

class UrgentMedicalFirst {
public:
    bool operator()(const ReliefCargo& a, const ReliefCargo& b) const {
        if (a.getPriority() != b.getPriority()) return a.getPriority() > b.getPriority();
        bool aIsMedicine = (a.getType() == TRIAGE_MEDICINE);
        bool bIsMedicine = (b.getType() == TRIAGE_MEDICINE);
        if (aIsMedicine != bIsMedicine) return aIsMedicine;
        return a.getWeight() < b.getWeight();
    }
};

inline ostream& tableLine(ostream& os) {
    return os << string(54, '=') << "\n";
}

inline ostream& manifestHeader(ostream& os) {
    tableLine(os);
    os << left << setw(6) << "ID" << setw(18) << "Supply" << right << setw(8) << "Tons"
       << setw(6) << "Prio" << "  " << left << setw(14) << "Label" << "\n";
    return tableLine(os);
}

}

// ================= UNIT I and III: namespace ReliefOps::Logistics =================
namespace Logistics {

using namespace Inventory;

inline double distanceKm(double x1, double y1, double x2, double y2) {
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}
inline double loadPercent(double load, double maxLoad) {
    return (maxLoad > 0) ? (load / maxLoad) * 100.0 : 0.0;
}

inline void addFuel(double& tank, double litres) {
    tank += litres;
}

double allocateSupply(DistributionDepot& depot, int supplyType, double volumeTons) {
    double& shelf = depot.stockRef(supplyType);
    if (shelf < volumeTons) {
        throw OutOfStockException(depot.getName(), supplyName(supplyType), volumeTons, shelf);
    }
    shelf -= volumeTons;
    return volumeTons;
}

double allocateSupply(DistributionDepot& depot, int triageLevel) {
    double wanted = triageLevel * 0.2;
    double& medicine = depot.stockRef(TRIAGE_MEDICINE);
    double given = (medicine < wanted) ? medicine : wanted;
    medicine -= given;
    return given;
}

class TransportVehicle {
private:
    string vehicleName;
    ReliefCargo* manifest;
    int slots;
    int itemCount;
    double maxLoadTons;

    static int totalVehicles;
    static double totalDispatchedTons;
public:
    TransportVehicle(const string& name, int slotCount, double maxLoad)
        : vehicleName(name), manifest(nullptr), slots(slotCount), itemCount(0), maxLoadTons(maxLoad) {
        manifest = new ReliefCargo[slots];
        totalVehicles++;
    }

    TransportVehicle(const TransportVehicle& other)
        : vehicleName(other.vehicleName), manifest(nullptr), slots(other.slots),
          itemCount(other.itemCount), maxLoadTons(other.maxLoadTons) {
        manifest = new ReliefCargo[slots];
        for (int i = 0; i < itemCount; i++) manifest[i] = other.manifest[i];
        totalVehicles++;
    }

    TransportVehicle& operator=(const TransportVehicle& other) {
        if (this != &other) {
            delete[] manifest;
            vehicleName = other.vehicleName;
            slots = other.slots;
            itemCount = other.itemCount;
            maxLoadTons = other.maxLoadTons;
            manifest = new ReliefCargo[slots];
            for (int i = 0; i < itemCount; i++) manifest[i] = other.manifest[i];
        }
        return *this;
    }

    ~TransportVehicle() {
        delete[] manifest;
        totalVehicles--;
    }

    const string& getName() const { return vehicleName; }
    int getItemCount() const { return itemCount; }
    double getMaxLoad() const { return maxLoadTons; }
    const ReliefCargo& getItem(int index) const { return manifest[index]; }

    double getTotalLoad() const {
        double total = 0;
        for (int i = 0; i < itemCount; i++) total += manifest[i].getWeight();
        return total;
    }

    void loadCargo(const ReliefCargo& cargo) {
        if (itemCount >= slots) {
            throw runtime_error("All the manifest slots on " + vehicleName + " are already used.");
        }
        for (int i = 0; i < itemCount; i++) {
            if (manifest[i] == cargo) {
                throw runtime_error("That cargo is already on " + vehicleName + ", no duplicates allowed.");
            }
        }
        double newLoad = getTotalLoad() + cargo.getWeight();
        if (newLoad > maxLoadTons) throw CargoOverweightException(vehicleName, newLoad, maxLoadTons);
        manifest[itemCount] = cargo;
        itemCount++;
    }

    void clearManifest() { itemCount = 0; }

    bool operator==(const TransportVehicle& other) const {
        if (itemCount != other.itemCount) return false;
        for (int i = 0; i < itemCount; i++) {
            if (!(manifest[i] == other.manifest[i])) return false;
        }
        return true;
    }

    static int getTotalVehicles() { return totalVehicles; }
    static double getTotalDispatchedTons() { return totalDispatchedTons; }
    static void addDispatchedTons(double tons) { totalDispatchedTons += tons; }
};

int TransportVehicle::totalVehicles = 0;
double TransportVehicle::totalDispatchedTons = 0.0;

ostream& operator<<(ostream& os, const TransportVehicle& v) {
    os << v.getName() << " " << v.getItemCount() << "\n";
    for (int i = 0; i < v.getItemCount(); i++) os << v.getItem(i) << "\n";
    return os;
}

// >> reads a manifest that was saved with <<.
// It builds the new manifest in a copy first, so if anything is wrong the real vehicle stays untouched.
istream& operator>>(istream& is, TransportVehicle& v) {
    string savedName;
    int count;
    if (!(is >> savedName >> count) || count < 0) {
        is.setstate(ios::failbit);
        return is;
    }
    TransportVehicle temp = v;
    temp.clearManifest();
    for (int i = 0; i < count; i++) {
        ReliefCargo c;
        if (!(is >> c)) return is;
        temp.loadCargo(c);
    }
    v = temp;
    return is;
}

void printManifestTable(const TransportVehicle& v) {
    cout << manifestHeader;
    for (int i = 0; i < v.getItemCount(); i++) {
        const ReliefCargo& c = v.getItem(i);
        cout << left << setw(6) << c.getId() << setw(18) << supplyName(c.getType())
             << right << setw(8) << c.getWeight() << setw(6) << c.getPriority()
             << "  " << left << setw(14) << c.getLabel() << "\n";
    }
    cout << tableLine;
}

class GPSBeacon {
private:
    double x, y;
public:
    GPSBeacon(double startX = 0, double startY = 0) : x(startX), y(startY) {}
    void moveTo(double newX, double newY) { x = newX; y = newY; }
    double getX() const { return x; }
    double getY() const { return y; }
};

class EmergencyLifeSupportUnit {
private:
    int oxygenHours;
    int medKits;
public:
    EmergencyLifeSupportUnit(int hours = 24, int kits = 5) : oxygenHours(hours), medKits(kits) {}
    bool isReady() const { return oxygenHours > 0 && medKits > 0; }
    int getOxygenHours() const { return oxygenHours; }
    int getMedKits() const { return medKits; }
};

// ========== ReliefVehicle, the root of the diamond ==========
class ReliefVehicle {
protected:
    string vehicleName;
    double fuelLitres;
    double speedKmh;
    double fuelPerKm;
    GPSBeacon gps;
    EmergencyLifeSupportUnit lifeSupport;
    TransportVehicle cargoHold;

    void checkFuel(double needed) const {
        if (needed > fuelLitres) throw FuelDepletionException(vehicleName, needed, fuelLitres);
    }
public:
    ReliefVehicle(const string& name, double fuel, double speed, double burn,
                  double maxLoad, double startX, double startY)
        : vehicleName(name), fuelLitres(fuel), speedKmh(speed), fuelPerKm(burn),
          gps(startX, startY), lifeSupport(), cargoHold(name, 10, maxLoad) {}

    virtual ~ReliefVehicle() {
        cout << "  [cleanup] " << vehicleName << " has left the fleet.\n";
    }

    virtual double evaluateRoute(double destX, double destY, bool corridorDamaged, double& fuelNeeded) = 0;
    virtual string vehicleType() const = 0;

    const string& getName() const { return vehicleName; }
    double getFuel() const { return fuelLitres; }
    double& fuelRef() { return fuelLitres; }
    GPSBeacon& getGPS() { return gps; }
    TransportVehicle& getCargoHold() { return cargoHold; }
    const EmergencyLifeSupportUnit& getLifeSupport() const { return lifeSupport; }
};

// "virtual" here is what fixes the diamond problem later on

class AerialTransport : virtual public ReliefVehicle {
public:
    AerialTransport(const string& name, double fuel, double maxLoad, double x, double y)
        : ReliefVehicle(name, fuel, 220.0, 6.0, maxLoad, x, y) {}

    double evaluateRoute(double destX, double destY, bool corridorDamaged, double& fuelNeeded) override {
        double distance = distanceKm(gps.getX(), gps.getY(), destX, destY);
        fuelNeeded = distance * fuelPerKm;
        checkFuel(fuelNeeded);
        return distance / speedKmh;
    }
    string vehicleType() const override { return "Aerial Transport"; }
};

class GroundConvoy : virtual public ReliefVehicle {
public:
    GroundConvoy(const string& name, double fuel, double maxLoad, double x, double y)
        : ReliefVehicle(name, fuel, 45.0, 2.5, maxLoad, x, y) {}

    double evaluateRoute(double destX, double destY, bool corridorDamaged, double& fuelNeeded) override {
        if (corridorDamaged) throw DamagedCorridorException(vehicleName);
        double distance = distanceKm(gps.getX(), gps.getY(), destX, destY);
        fuelNeeded = distance * fuelPerKm;
        checkFuel(fuelNeeded);
        return distance / speedKmh;
    }
    string vehicleType() const override { return "Ground Convoy"; }
};

class AmphibiousCraft : virtual public ReliefVehicle {
public:
    AmphibiousCraft(const string& name, double fuel, double maxLoad, double x, double y)
        : ReliefVehicle(name, fuel, 30.0, 3.5, maxLoad, x, y) {}

    double evaluateRoute(double destX, double destY, bool corridorDamaged, double& fuelNeeded) override {
        double distance = distanceKm(gps.getX(), gps.getY(), destX, destY);
        if (corridorDamaged) distance = distance * 1.5;
        fuelNeeded = distance * fuelPerKm;
        checkFuel(fuelNeeded);
        return distance / speedKmh;
    }
    string vehicleType() const override { return "Amphibious Craft"; }
};

// ========== the diamond: ReliefVehicle on top, AmphibiousMedicalUnit at the bottom ==========
// Because both parents inherit ReliefVehicle virtually, the hybrid has only ONE copy of it.
class AmphibiousMedicalUnit : public GroundConvoy, public AmphibiousCraft {
public:
    // the most derived class builds the virtual base itself, so we call ReliefVehicle here.
    // The ReliefVehicle calls inside GroundConvoy and AmphibiousCraft get skipped.
    AmphibiousMedicalUnit(const string& name, double fuel, double maxLoad, double x, double y)
        : ReliefVehicle(name, fuel, 35.0, 4.0, maxLoad, x, y),
          GroundConvoy(name, fuel, maxLoad, x, y),
          AmphibiousCraft(name, fuel, maxLoad, x, y) {}

    // both parents override evaluateRoute, so we must pick the final one here
    double evaluateRoute(double destX, double destY, bool corridorDamaged, double& fuelNeeded) override {
        try {
            return GroundConvoy::evaluateRoute(destX, destY, corridorDamaged, fuelNeeded);
        } catch (const DamagedCorridorException& problem) {
            cout << "  Hybrid note: " << problem.what() << "\n  Switching to water mode.\n";
            return AmphibiousCraft::evaluateRoute(destX, destY, corridorDamaged, fuelNeeded);
        }
    }
    string vehicleType() const override { return "Amphibious Medical Unit"; }
};

// ================= UNIT IV: binary persistence =================
// One TransitRecord per dispatch (the transit log).
// One AuditRecord per cargo item that left on that dispatch (the manifest audit).
struct TransitRecord {
    int recordNumber;
    char vehicleName[30];
    double destX;
    double destY;
    int cargoItems;
    double tonsDelivered;
    double hours;
};

struct AuditRecord {
    int dispatchNumber;
    char vehicleName[30];
    int cargoId;
    int supplyType;
    double weightTons;
    int priority;
    char label[30];
};

const char* LOG_FILE = "transit_log.bin";
const char* LOG_TEMP = "transit_log.tmp";
const char* AUDIT_FILE = "manifest_audit.bin";
const char* AUDIT_TEMP = "manifest_audit.tmp";

template <class T>
vector<T> readAllRecords(const char* fileName) {
    vector<T> records;
    fstream file(fileName, ios::in | ios::binary);
    if (!file) return records;

    file.seekg(0, ios::end);
    long long fileSize = static_cast<long long>(file.tellg());
    int recordCount = static_cast<int>(fileSize / sizeof(T));
    file.seekg(0, ios::beg);

    for (int i = 0; i < recordCount; i++) {
        T rec;
        file.read(reinterpret_cast<char*>(&rec), sizeof(T));
        if (file) records.push_back(rec);
    }
    return records;
}

vector<TransitRecord> readTransitLog() { return readAllRecords<TransitRecord>(LOG_FILE); }
vector<AuditRecord> readAuditLog() { return readAllRecords<AuditRecord>(AUDIT_FILE); }

template <class T>
void writeTempFile(const vector<T>& records, const char* tempName) {
    fstream temp(tempName, ios::out | ios::binary | ios::trunc);
    if (!temp) throw runtime_error(string("Couldn't open the temp file ") + tempName + ", so nothing was saved.");
    for (size_t i = 0; i < records.size(); i++) {
        temp.write(reinterpret_cast<const char*>(&records[i]), sizeof(T));
    }
    temp.close();
    if (temp.fail()) {
        std::remove(tempName);
        throw runtime_error(string("Writing ") + tempName + " failed halfway, the old file is still safe.");
    }
}

void swapIntoPlace(const char* tempName, const char* realName) {
    std::remove(realName);     // remove first so rename also works on Windows
    if (std::rename(tempName, realName) != 0) {
        throw runtime_error(string("Couldn't swap ") + realName + " into place.");
    }
}

// Atomic style update for both files together.
// Step 1 writes the new versions to temp files, the real files are not touched yet.
// Step 2 swaps them in. If step 1 fails, nothing real has changed.
void saveDispatchRecords(TransitRecord rec, vector<AuditRecord> items) {
    vector<TransitRecord> transitLog = readTransitLog();
    vector<AuditRecord> auditLog = readAuditLog();

    rec.recordNumber = static_cast<int>(transitLog.size()) + 1;
    for (size_t i = 0; i < items.size(); i++) {
        items[i].dispatchNumber = rec.recordNumber;
        auditLog.push_back(items[i]);
    }
    transitLog.push_back(rec);

    writeTempFile(transitLog, LOG_TEMP);
    try {
        writeTempFile(auditLog, AUDIT_TEMP);
    } catch (...) {
        std::remove(LOG_TEMP);
        throw;
    }
    swapIntoPlace(LOG_TEMP, LOG_FILE);
    swapIntoPlace(AUDIT_TEMP, AUDIT_FILE);
}

template <class T, class Compare = less<T> >
class PriorityDispatchQueue {
private:
    vector<T> items;
public:
    void push(const T& item) {
        items.push_back(item);
        sort(items.begin(), items.end(), Compare());
    }
    const T& peekTop() const {
        if (items.empty()) throw out_of_range("The dispatch queue is empty, nothing to serve.");
        return items.front();
    }
    void popTop() {
        if (items.empty()) throw out_of_range("The dispatch queue is empty, nothing to pop.");
        items.erase(items.begin());
    }
    bool isEmpty() const { return items.empty(); }
    int size() const { return static_cast<int>(items.size()); }
};

double dispatchVehicle(DistributionDepot& depot, ReliefVehicle* vehicle,
                       double destX, double destY, bool corridorDamaged) {
    TransportVehicle& hold = vehicle->getCargoHold();
    if (hold.getItemCount() == 0) {
        throw runtime_error("Nothing is loaded on " + vehicle->getName() + " yet, so there's nothing to send.");
    }

    // runtime polymorphism, the right evaluateRoute gets called through the base pointer
    double fuelNeeded = 0;
    double hours = vehicle->evaluateRoute(destX, destY, corridorDamaged, fuelNeeded);

    // take the cargo out of the depot, and roll back if any item fails
    DistributionDepot backup = depot;
    try {
        for (int i = 0; i < hold.getItemCount(); i++) depot -= hold.getItem(i);
    } catch (...) {
        depot = backup;
        throw;
    }

    double tons = hold.getTotalLoad();

    TransitRecord rec;
    memset(&rec, 0, sizeof(rec));
    strncpy(rec.vehicleName, vehicle->getName().c_str(), sizeof(rec.vehicleName) - 1);
    rec.destX = destX;
    rec.destY = destY;
    rec.cargoItems = hold.getItemCount();
    rec.tonsDelivered = tons;
    rec.hours = hours;

    vector<AuditRecord> auditItems;
    for (int i = 0; i < hold.getItemCount(); i++) {
        const ReliefCargo& c = hold.getItem(i);
        AuditRecord a;
        memset(&a, 0, sizeof(a));
        strncpy(a.vehicleName, vehicle->getName().c_str(), sizeof(a.vehicleName) - 1);
        a.cargoId = c.getId();
        a.supplyType = c.getType();
        a.weightTons = c.getWeight();
        a.priority = c.getPriority();
        strncpy(a.label, c.getLabel(), sizeof(a.label) - 1);
        auditItems.push_back(a);
    }

    // save to disk first, if that fails put the depot back so nothing is half done
    try {
        saveDispatchRecords(rec, auditItems);
    } catch (...) {
        depot = backup;
        throw;
    }

    // everything is saved, now it is safe to change the vehicle
    vehicle->fuelRef() -= fuelNeeded;
    vehicle->getGPS().moveTo(destX, destY);
    TransportVehicle::addDispatchedTons(tons);
    hold.clearManifest();
    return hours;
}

int restoreFromLog(double& totalTons) {
    vector<TransitRecord> records = readTransitLog();
    totalTons = 0;
    for (size_t i = 0; i < records.size(); i++) totalTons += records[i].tonsDelivered;
    TransportVehicle::addDispatchedTons(totalTons);
    return static_cast<int>(records.size());
}

}
}

using namespace ReliefOps;
using namespace ReliefOps::Inventory;
using namespace ReliefOps::Logistics;

// ================= console look and feel =================
// Colors only work if the console understands ANSI codes. If you see weird symbols,
// set USE_COLOR to false at the top of the file.

// Windows needs to be told to turn color codes on, other systems already do it
void enableColors() {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(console, &mode);
    SetConsoleMode(console, mode | 0x0004);
#endif
}

void showGood(const string& text) { cout << GREEN << text << RESET << "\n"; }
void showBad(const string& text) { cout << RED << text << RESET << "\n"; }

void menuTitle(const string& text) { cout << YELLOW << " " << text << RESET << "\n"; }
void menuItem(int number, const string& text) {
    cout << "   " << right << setw(2) << number << left << "  " << text << "\n";
}

void printMenu(const DistributionDepot& depot, int queueSize) {
    double totalStock = 0;
    for (int i = 0; i < TOTAL_SUPPLY_TYPES; i++) totalStock += depot.getStock(i);

    cout << "\n" << CYAN
         << "+============================================+\n"
         << "|     ERL HUB   Relief Logistics Control     |\n"
         << "+============================================+" << RESET << "\n";
    cout << " Stock " << totalStock << " t   Vehicles " << TransportVehicle::getTotalVehicles()
         << "   Sent " << TransportVehicle::getTotalDispatchedTons() << " t   Queue " << queueSize << "\n\n";

    menuTitle("DEPOT");
    menuItem(1, "Show stock levels");
    menuItem(2, "Restock depot");
    menuItem(6, "Allocate supplies");
    menuTitle("VEHICLES");
    menuItem(3, "Load cargo on a vehicle");
    menuItem(4, "Fleet overview and manifests");
    menuItem(5, "Dispatch a vehicle");
    menuItem(14, "Refuel a vehicle");
    menuItem(15, "Save or load a manifest file");
    menuItem(9, "Compare two manifests");
    menuTitle("TRIAGE");
    menuItem(7, "Add item to triage queue");
    menuItem(8, "Serve next item from triage queue");
    menuItem(11, "Incident commander override");
    menuTitle("OPERATIONS");
    menuItem(10, "Mark a corridor damaged or open");
    menuItem(12, "Show transit log and manifest audit from disk");
    menuItem(13, "Show network statistics");
    menuItem(0, "Exit");
    cout << "\nYour choice > ";
}

void clearBadInput() {
    cin.clear();
    cin.ignore(10000, '\n');
}

int askVehicleIndex(const vector<ReliefVehicle*>& fleet) {
    for (size_t i = 0; i < fleet.size(); i++) {
        cout << "   " << (i + 1) << "  " << left << setw(24) << fleet[i]->vehicleType()
             << fleet[i]->getName() << "\n";
    }
    cout << "Pick a vehicle number > ";
    int pick;
    if (!(cin >> pick)) {
        clearBadInput();
        return -1;
    }
    if (pick < 1 || pick > static_cast<int>(fleet.size())) return -1;
    return pick - 1;
}

const char* cargoFormatHelp =
    "Type the cargo as one line: id type weight priority label\n"
    "   type: 0 ration, 1 medicine, 2 water, 3 blanket\n"
    "   priority: 1 low to 5 critical, label: one word\n"
    "   example: 101 1 0.5 5 insulin\n> ";

int main() {
    enableColors();
    cout << fixed << setprecision(2);

    DistributionDepot depot("RohiniFieldHub");
    depot += ReliefCargo(1, RATION_KIT, 20.0, 2, "startRations");
    depot += ReliefCargo(2, TRIAGE_MEDICINE, 5.0, 4, "startMedicine");
    depot += ReliefCargo(3, WATER_TANK, 30.0, 3, "startWater");
    depot += ReliefCargo(4, THERMAL_BLANKET, 8.0, 2, "startBlankets");

    // base pointer vector, this is where runtime polymorphism happens
    vector<ReliefVehicle*> fleet;
    fleet.push_back(new AerialTransport("HeliAlpha", 3000, 3.0, 0, 0));
    fleet.push_back(new GroundConvoy("TruckBravo", 1500, 10.0, 0, 0));
    fleet.push_back(new AmphibiousCraft("BoatCharlie", 900, 4.0, 0, 0));
    fleet.push_back(new AmphibiousMedicalUnit("HybridDelta", 1200, 5.0, 0, 0));

    unordered_map<string, bool> corridors = {
        {"NorthHighway", false}, {"RiverRoute", false}, {"CityBypass", true}
    };

    PriorityDispatchQueue<ReliefCargo, UrgentMedicalFirst> triageQueue;

    double restoredTons = 0;
    int restoredRecords = restoreFromLog(restoredTons);
    if (restoredRecords > 0) {
        showGood("Restored from disk. Dispatch records " + to_string(restoredRecords)
                 + ", tons sent so far " + toText(restoredTons) + ".");
    }

    int choice = -1;
    do {
        printMenu(depot, triageQueue.size());

        if (!(cin >> choice)) {
            clearBadInput();
            showBad("That wasn't a number, try again.");
            choice = -1;
            continue;
        }
        cout << "\n";

        // every exception in the project comes from std::exception so one catch handles them
        try {
            switch (choice) {
            case 1:
                depot.showStock();
                break;

            case 2: {
                ReliefCargo newCargo;
                cout << cargoFormatHelp;
                if (!(cin >> newCargo)) {
                    clearBadInput();
                    showBad("That cargo line didn't look right, nothing was added.");
                    break;
                }
                depot += newCargo;
                showGood("Restocked: " + string(supplyName(newCargo.getType())) + ", "
                         + toText(newCargo.getWeight()) + " tons of " + newCargo.getLabel());
                break;
            }

            case 3: {
                int v = askVehicleIndex(fleet);
                if (v < 0) {
                    showBad("That vehicle number doesn't exist.");
                    break;
                }
                ReliefCargo newCargo;
                cout << cargoFormatHelp;
                if (!(cin >> newCargo)) {
                    clearBadInput();
                    showBad("That cargo line didn't look right, nothing was loaded.");
                    break;
                }
                fleet[v]->getCargoHold().loadCargo(newCargo);
                showGood("Loaded onto " + fleet[v]->getName() + ".");
                break;
            }

            case 4:
                cout << CYAN << left << setw(24) << "Type" << setw(13) << "Name" << right << setw(9) << "Fuel L"
                     << setw(16) << "Position" << setw(24) << "Load tons and pct" << "   Life" << RESET << "\n";
                for_each(fleet.begin(), fleet.end(), [](ReliefVehicle* v) {
                    TransportVehicle& hold = v->getCargoHold();
                    string position = toText(v->getGPS().getX()) + ", " + toText(v->getGPS().getY());
                    string load = toText(hold.getTotalLoad()) + "/" + toText(hold.getMaxLoad())
                                  + "  " + to_string(static_cast<int>(loadPercent(hold.getTotalLoad(), hold.getMaxLoad()))) + "%";
                    bool ready = v->getLifeSupport().isReady();
                    cout << left << setw(24) << v->vehicleType() << setw(13) << v->getName()
                         << right << setw(9) << v->getFuel() << setw(16) << position
                         << setw(24) << load << "   ";
                    cout << (ready ? GREEN : RED) << (ready ? "ready" : "DOWN") << RESET << "\n";
                    if (hold.getItemCount() > 0) printManifestTable(hold);
                });
                cout << left;
                break;

            case 5: {
                int v = askVehicleIndex(fleet);
                if (v < 0) {
                    showBad("That vehicle number doesn't exist.");
                    break;
                }
                double destX, destY;
                cout << "Destination x and y in km > ";
                if (!(cin >> destX >> destY)) {
                    clearBadInput();
                    showBad("Those coordinates didn't make sense.");
                    break;
                }
                cout << "Corridors:\n";
                for (const auto& entry : corridors) {
                    cout << "   " << left << setw(14) << entry.first;
                    if (entry.second) cout << RED << "DAMAGED" << RESET << "\n";
                    else cout << GREEN << "open" << RESET << "\n";
                }
                string corridorName;
                cout << "Type the corridor name > ";
                cin >> corridorName;
                auto found = corridors.find(corridorName);
                if (found == corridors.end()) {
                    showBad("Never heard of that corridor.");
                    break;
                }
                double hours = dispatchVehicle(depot, fleet[v], destX, destY, found->second);
                showGood(fleet[v]->getName() + " delivered the cargo in " + toText(hours) + " hours.");
                break;
            }

            case 6: {
                int mode;
                cout << "1 for volume based, 2 for triage based > ";
                cin >> mode;
                if (mode == 1) {
                    int supplyType;
                    double volumeTons;
                    cout << "Supply types: 0 ration, 1 medicine, 2 water, 3 blanket\nSupply type and tons > ";
                    if (!(cin >> supplyType >> volumeTons) || supplyType < 0 || supplyType > 3) {
                        clearBadInput();
                        showBad("Couldn't read that.");
                        break;
                    }
                    double given = allocateSupply(depot, supplyType, volumeTons);
                    showGood("Allocated " + toText(given) + " tons of " + supplyName(supplyType) + ".");
                } else if (mode == 2) {
                    int triageLevel;
                    cout << "Triage level 1 to 5 > ";
                    if (!(cin >> triageLevel) || triageLevel < 1 || triageLevel > 5) {
                        clearBadInput();
                        showBad("Triage level must be 1 to 5.");
                        break;
                    }
                    double given = allocateSupply(depot, triageLevel);
                    showGood("Triage allocation gave " + toText(given) + " tons of medicine.");
                } else {
                    showBad("Only 1 or 2 please.");
                }
                break;
            }

            case 7: {
                ReliefCargo newCargo;
                cout << cargoFormatHelp;
                if (!(cin >> newCargo)) {
                    clearBadInput();
                    showBad("That cargo line didn't look right.");
                    break;
                }
                triageQueue.push(newCargo);
                showGood("Added. Queue size is now " + to_string(triageQueue.size()) + ".");
                break;
            }

            case 8: {
                const ReliefCargo& top = triageQueue.peekTop();
                cout << "Serving: " << top << "\n";
                allocateSupply(depot, top.getType(), top.getWeight());
                triageQueue.popTop();
                showGood("Done. Items still waiting: " + to_string(triageQueue.size()) + ".");
                break;
            }

            case 9: {
                cout << "First vehicle.\n";
                int a = askVehicleIndex(fleet);
                cout << "Second vehicle.\n";
                int b = askVehicleIndex(fleet);
                if (a < 0 || b < 0) {
                    showBad("One of those vehicle numbers doesn't exist.");
                    break;
                }
                if (fleet[a]->getCargoHold() == fleet[b]->getCargoHold()) {
                    showGood("Same manifests.");
                } else {
                    cout << YELLOW << "Different manifests." << RESET << "\n";
                }
                break;
            }

            case 10: {
                string corridorName;
                int state;
                cout << "Corridor name > ";
                cin >> corridorName;
                cout << "1 for damaged, 0 for open > ";
                cin >> state;
                corridors[corridorName] = (state == 1);
                if (state == 1) showBad(corridorName + " is now damaged.");
                else showGood(corridorName + " is now open.");
                break;
            }

            case 11: {
                IncidentCommander commander;
                ReliefCargo urgentCargo;
                cout << cargoFormatHelp;
                if (!(cin >> urgentCargo)) {
                    clearBadInput();
                    showBad("That cargo line didn't look right.");
                    break;
                }
                commander.forcePriority(urgentCargo);
                commander.emergencyPull(depot, urgentCargo);
                triageQueue.push(urgentCargo);
                cout << YELLOW << "Commander override done: " << urgentCargo << RESET << "\n";
                commander.inspect(depot);
                break;
            }

            case 12: {
                vector<TransitRecord> records = readTransitLog();
                vector<AuditRecord> audit = readAuditLog();
                if (records.empty()) {
                    cout << "The log is empty so far.\n";
                    break;
                }
                cout << CYAN << left << setw(5) << "No" << setw(13) << "Vehicle" << setw(16) << "Destination"
                     << right << setw(7) << "Items" << setw(10) << "Tons" << setw(10) << "Hours" << RESET << "\n";
                for_each(records.begin(), records.end(), [](const TransitRecord& r) {
                    string place = toText(r.destX) + ", " + toText(r.destY);
                    cout << left << setw(5) << r.recordNumber << setw(13) << r.vehicleName
                         << setw(16) << place << right << setw(7) << r.cargoItems
                         << setw(10) << r.tonsDelivered << setw(10) << r.hours << "\n";
                });
                cout << "\n" << CYAN << left << setw(5) << "No" << setw(13) << "Vehicle" << setw(7) << "Cargo"
                     << setw(18) << "Supply" << right << setw(8) << "Tons" << setw(6) << "Prio"
                     << "  " << left << "Label" << RESET << "\n";
                for_each(audit.begin(), audit.end(), [](const AuditRecord& a) {
                    cout << left << setw(5) << a.dispatchNumber << setw(13) << a.vehicleName
                         << setw(7) << a.cargoId << setw(18) << supplyName(a.supplyType)
                         << right << setw(8) << a.weightTons << setw(6) << a.priority
                         << "  " << left << a.label << "\n";
                });
                cout << left;
                break;
            }

            case 13:
                cout << CYAN << "Network statistics" << RESET << "\n"
                     << "   Operational vehicles    " << TransportVehicle::getTotalVehicles() << "\n"
                     << "   Total dispatched        " << TransportVehicle::getTotalDispatchedTons() << " tons\n";
                break;

            case 14: {
                int v = askVehicleIndex(fleet);
                if (v < 0) {
                    showBad("That vehicle number doesn't exist.");
                    break;
                }
                double litres;
                cout << "Litres to add > ";
                if (!(cin >> litres) || litres <= 0) {
                    clearBadInput();
                    showBad("Litres must be a positive number.");
                    break;
                }
                addFuel(fleet[v]->fuelRef(), litres);
                showGood(fleet[v]->getName() + " now has " + toText(fleet[v]->getFuel()) + " litres.");
                break;
            }

            case 15: {
                int v = askVehicleIndex(fleet);
                if (v < 0) {
                    showBad("That vehicle number doesn't exist.");
                    break;
                }
                int mode;
                cout << "1 to save this manifest to a text file, 2 to load it back > ";
                cin >> mode;
                string fileName = "manifest_" + fleet[v]->getName() + ".txt";
                if (mode == 1) {
                    string tempName = fileName + ".tmp";
                    {
                        ofstream out(tempName);
                        if (!out) throw runtime_error("Couldn't open " + tempName + " for writing.");
                        out << fleet[v]->getCargoHold();
                    }
                    swapIntoPlace(tempName.c_str(), fileName.c_str());
                    showGood("Saved the manifest to " + fileName + ". Items "
                             + to_string(fleet[v]->getCargoHold().getItemCount()) + ".");
                } else if (mode == 2) {
                    ifstream in(fileName);
                    if (!in) throw runtime_error("There is no saved manifest called " + fileName + " yet.");
                    if (!(in >> fleet[v]->getCargoHold())) {
                        showBad("That file looks damaged, so the manifest was left alone.");
                        break;
                    }
                    showGood("Loaded the manifest from " + fileName + ". Items "
                             + to_string(fleet[v]->getCargoHold().getItemCount()) + ".");
                } else {
                    showBad("Only 1 or 2 please.");
                }
                break;
            }

            case 0:
                showGood("Shutting down the hub.");
                break;

            default:
                showBad("No such menu option.");
            }
        } catch (const exception& problem) {
            cout << RED << "Problem: " << problem.what() << RESET << "\n";
        }
    } while (choice != 0);

    // delete through base pointers, the virtual destructor makes sure each full object is cleaned up
    for (size_t i = 0; i < fleet.size(); i++) delete fleet[i];
    fleet.clear();

    return 0;
}