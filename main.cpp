#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>

#include <http_server.h>
#include <metrics_collector.h>

// HELPER FUNCTIONS that read from environment variables.
// Read an unsigned short from env
static unsigned short env_port(const char* name, unsigned short fallback) {
    if (const char* v = std::getenv(name)) {
        try {
            long val = std::stol(v);
            if (val > 0 && val <= 65535) return static_cast<unsigned short>(val);
        } catch (...) {}
    }
    return fallback;
}

// Read interval (ms) from env, with sane bounds (100ms to 60000ms)
static std::chrono::milliseconds env_interval_ms(const char* name, int fallback_ms) {
    if (const char* v = std::getenv(name)) {
        try {
            long val = std::stol(v);
            if (val >= 100 && val <= 60'000) return std::chrono::milliseconds(val);
        } catch (...) {}
    }
    return std::chrono::milliseconds(fallback_ms);
}


/* -------------------- Simple synchronous HTTP server -------------------- */
static std::atomic<bool> stopFlag{false};
void handleSig(int){ stopFlag = true; }



int main() {
    std::signal(SIGINT,  handleSig);
    std::signal(SIGTERM, handleSig);

    // Read configuration from environment variables
    const unsigned short port = env_port("PORT", 8080);
    const auto interval = env_interval_ms("INTERVAL_MS", 1000);
    std::cout << "Usando puerto " << port << " e intervalo " << interval.count() << " ms\n";

    MetricsCollector mc(interval);
    mc.start();

    // we must wait a bit to have some metrics collected
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    run_http_server(port, mc, stopFlag);

    mc.stop();
    std::cout << "Apagando...\n";
    return 0;
}
