#include "metrics_collector.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

// Collector class to gather CPU and memory usage metrics
MetricsCollector::MetricsCollector(std::chrono::milliseconds interval)
    : interval_(interval) {}

MetricsCollector::~MetricsCollector() { stop(); }

void MetricsCollector::start() {
    if (running.exchange(true)) return;
    worker = std::thread(&MetricsCollector::loop, this);
}

void MetricsCollector::stop() {
    if (!running.exchange(false)) return;
    if (worker.joinable()) worker.join();
}

// render metrics in Prometheus format
std::string MetricsCollector::renderPrometheus() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::ostringstream oss;
    oss.setf(std::ios::fixed);

    oss << "# HELP cpu_usage Porcentaje de uso de CPU\n"
        << "# TYPE cpu_usage gauge\n"
        << "cpu_usage " << std::setprecision(2) << cpu_ << "\n"
        << "# HELP mem_usage Porcentaje de uso de memoria\n"
        << "# TYPE mem_usage gauge\n"
        << "mem_usage " << std::setprecision(2) << mem_ << "\n"
        << "# HELP uptime_seconds Tiempo activo del servicio\n"
        << "# TYPE uptime_seconds counter\n"
        << "uptime_seconds " << uptime_ << "\n"
        << "# HELP collector_up 1 si la última lectura fue correcta\n"
        << "# TYPE collector_up gauge\n"
        << "collector_up " << (lastCpuReadOk_ && lastMemReadOk_ ? 1 : 0) << "\n";
    return oss.str();
}

// Getters for individual metrics
double MetricsCollector::cpu() const {
    std::lock_guard<std::mutex> l(mtx);
    return cpu_;
}

double MetricsCollector::mem() const {
    std::lock_guard<std::mutex> l(mtx);
    return mem_;
}

// Uptime in seconds
long MetricsCollector::uptime() const {
    std::lock_guard<std::mutex> l(mtx);
    return uptime_;
}

// Main loop to periodically update metrics
void MetricsCollector::loop() {
    using namespace std::chrono_literals; // To use chrono literals like 1s
    while (running) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            cpu_ = readCpuPercent();
            mem_ = readMemPercent();
            ++uptime_;
        }
        std::this_thread::sleep_for(interval_);
    }
}

double MetricsCollector::readCpuPercent() {
    std::ifstream stat("/proc/stat");
    if (!stat.good()) {
        lastCpuReadOk_ = false;
        return 0.0;
    }

    std::string cpuLabel;
    // /proc/stat file line format: cpu  3357 0 4313 1362393 ... which corresponds to:
    // user, nice, system, idle, iowait, irq, softirq, steal
    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    stat >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    unsigned long long idleAll = idle + iowait;
    unsigned long long nonIdle = user + nice + system + irq + softirq + steal;
    unsigned long long total = idleAll + nonIdle;

    // If there is no previous measurement done, we cannot calculate usage, so we set it up and return 0.0
    if (!hasPrev_) {
        prevIdle_ = idleAll;
        prevTotal_ = total;
        hasPrev_ = true;
        lastCpuReadOk_ = true; // first read is always OK
        return 0.0;
    }

    // If there is previous measurement done, we can calculate the CPU usage
    unsigned long long idleDelta = idleAll - prevIdle_;
    unsigned long long totalDelta = total - prevTotal_;
    prevIdle_ = idleAll;
    prevTotal_ = total;

    lastCpuReadOk_ = (totalDelta != 0);
    if (!lastCpuReadOk_) return 0.0;

    // avoid division by zero
    if (totalDelta == 0) return 0.0;
    double usage = 100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta));
    if (usage < 0.0) usage = 0.0;
    if (usage > 100.0) usage = 100.0;
    return usage;
}

double MetricsCollector::readMemPercent() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.good()) {
        lastMemReadOk_ = false;
        return 0.0;
    }

    // /proc/meminfo file lines of interest:
    // MemTotal:       16384256 kB
    // MemAvailable:   12345678 kB
    // SO lines info is stored in the three variables below
    std::string key, units;
    unsigned long long valueKb = 0;
    unsigned long long memTotal = 0, memAvail = 0;

    while (meminfo >> key >> valueKb >> units) {
        if (key == "MemTotal:") memTotal = valueKb;
        else if (key == "MemAvailable:") memAvail = valueKb;
        // IF we already have both key values, we can stop reading
        if (memTotal && memAvail) break;
    }

    lastMemReadOk_ = (memTotal > 0);
    if (!lastMemReadOk_) return 0.0;

    double used = static_cast<double>(memTotal - memAvail);
    return 100.0 * used / static_cast<double>(memTotal);
}
