#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

// Collector class to gather CPU and memory usage metrics
class MetricsCollector {
public:
    MetricsCollector(std::chrono::milliseconds interval);
    ~MetricsCollector();

    void start();
    void stop();

    // render metrics in Prometheus format
    std::string renderPrometheus() const;

    // Getters for individual metrics
    double cpu() const;
    double mem() const;
    // Uptime in seconds
    long uptime() const;

private:
    // Main loop to periodically update metrics
    void loop();

    double readCpuPercent();
    double readMemPercent();

    std::atomic<bool> running{false};
    std::thread worker;
    mutable std::mutex mtx;

    double cpu_{0.0};
    double mem_{0.0};
    long uptime_{0};

    // for CPU usage calculation
    unsigned long long prevIdle_{0};
    unsigned long long prevTotal_{0};
    bool hasPrev_{false};

    // Health flags for last reads
    bool lastCpuReadOk_{false};
    bool lastMemReadOk_{false};

    // Interval between metric collections
    std::chrono::milliseconds interval_{1000};
};
