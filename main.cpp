#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>


// Boost.Beast/Asio
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp = net::ip::tcp;

// Collector class to gather CPU and memory usage metrics
class MetricsCollector {
public:
    MetricsCollector() = default;
    ~MetricsCollector() { stop(); }

    void start() {
        if (running.exchange(true)) return;
        worker = std::thread(&MetricsCollector::loop, this);
    }
    void stop() {
        if (!running.exchange(false)) return;
        if (worker.joinable()) worker.join();
    }

    // render metrics in Prometheus format
    std::string renderPrometheus() const {
        std::lock_guard<std::mutex> lock(mtx);
        std::ostringstream oss;
        oss << "# HELP cpu_usage Porcentaje de uso de CPU\n"
            << "# TYPE cpu_usage gauge\n"
            << "cpu_usage " << cpu_ << "\n"
            << "# HELP mem_usage Porcentaje de uso de memoria\n"
            << "# TYPE mem_usage gauge\n"
            << "mem_usage " << mem_ << "\n"
            << "# HELP uptime_seconds Tiempo activo del servicio\n"
            << "# TYPE uptime_seconds counter\n"
            << "uptime_seconds " << uptime_ << "\n";
        return oss.str();
    }

    // Getters for individual metrics
    double cpu() const     { std::lock_guard<std::mutex> l(mtx); return cpu_; }
    double mem() const     { std::lock_guard<std::mutex> l(mtx); return mem_; }
    // Uptime in seconds
    long   uptime() const  { std::lock_guard<std::mutex> l(mtx); return uptime_; }

private:
    // Main loop to periodically update metrics
    void loop() {
        // To use chrono literals like 1s
        using namespace std::chrono_literals;
        while (running) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                cpu_ = readCpuPercent();
                mem_ = readMemPercent();
                ++uptime_;
            }
            std::this_thread::sleep_for(1s);
        }
    }

    double readCpuPercent() {
        std::ifstream stat("/proc/stat");
        if (!stat.good()) return 0.0;

        std::string cpuLabel;
        // /proc/stat file line format: cpu  3357 0 4313 1362393 ... which corresponds to:
        // user, nice, system, idle, iowait, irq, softirq, steal
        unsigned long long user=0, nice=0, system=0, idle=0, iowait=0, irq=0, softirq=0, steal=0;
        stat >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        unsigned long long idleAll  = idle + iowait;
        unsigned long long nonIdle  = user + nice + system + irq + softirq + steal;
        unsigned long long total    = idleAll + nonIdle;

        // If there is no previous measurement done, we cannot calculate usage, so we set it up and return 0.0
        if (!hasPrev_) {
            prevIdle_ = idleAll;
            prevTotal_ = total;
            hasPrev_ = true;
            return 0.0;
        }

        // If there is previous measurement done,e can calculate the CPU usage
        unsigned long long idleDelta  = idleAll - prevIdle_;
        unsigned long long totalDelta = total   - prevTotal_;
        prevIdle_  = idleAll;
        prevTotal_ = total;

        //avoid division by zero
        if (totalDelta == 0) return 0.0;
        double usage = 100.0 * (1.0 - (double)idleDelta / (double)totalDelta);
        if (usage < 0.0)   usage = 0.0;
        if (usage > 100.0) usage = 100.0;
        return usage;
    }

    double readMemPercent() {
        std::ifstream meminfo("/proc/meminfo");
        if (!meminfo.good()) return 0.0;

        // /proc/meminfo file lines of interest:
        // MemTotal:       16384256 kB
        // MemAvailable:   12345678 kB
        // SO lines info is stored in the three variables below
        std::string key, units;
        unsigned long long valueKb = 0;
        unsigned long long memTotal = 0, memAvail = 0;

        while (meminfo >> key >> valueKb >> units) {
            if (key == "MemTotal:")        memTotal = valueKb;
            else if (key == "MemAvailable:") memAvail = valueKb;
            // IF we already have both key values, we can stop reading
            if (memTotal && memAvail) break;
        }

        if (memTotal == 0) return 0.0;
        double used = (double)(memTotal - memAvail);
        // Calculate memory usage percentage
        return 100.0 * used / (double)memTotal;
    }

    std::atomic<bool> running{false};
    std::thread worker;
    mutable std::mutex mtx;

    double cpu_{0.0};
    double mem_{0.0};
    long   uptime_{0};

    // for CPU usage calculation
    unsigned long long prevIdle_{0};
    unsigned long long prevTotal_{0};
    bool hasPrev_{false};
};

/* -------------------- Simple synchronous HTTP server -------------------- */
static std::atomic<bool> stopFlag{false};
void handleSig(int){ stopFlag = true; }

void run_http_server(unsigned short port, MetricsCollector& mc) {
    net::io_context ioc{1};
    beast::error_code ec;

    tcp::acceptor acceptor{ioc, {tcp::v4(), port}, true};
    acceptor.non_blocking(true, ec); // set non-blocking acceptor to allow shutdown with ctrl+c

    std::cout << "HTTP escuchando en http://localhost:" << port
              << "  (endpoints: /metrics, /healthz)\n";

    while (!stopFlag.load()) {
        tcp::socket socket{ioc};

        acceptor.accept(socket, ec);
        // If error on accept WHICH MEANS no connection is present, just continue
        if (ec) {
            if (ec == net::error::would_block) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            // Other errors in accept: log and continue
            std::cerr << "accept error: " << ec.message() << "\n";
            continue;
        }

        // Read HTTP request
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);
        if (ec) { socket.close(); continue; }

        // Prepare HTTP response
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.keep_alive(false);

        // If called endpoint /metrics, return metrics in Prometheus format
        if (req.method() == http::verb::get && req.target() == "/metrics") {
            res.set(http::field::content_type, "text/plain; version=0.0.4");
            res.body() = mc.renderPrometheus();
        
        // If called endpoint /healthz, return simple OK response
        } else if (req.method() == http::verb::get && req.target() == "/healthz") {
            res.set(http::field::content_type, "text/plain");
            res.body() = "ok\n";
        // For other endpoints, return 404 Not Found
        } else {
            res.result(http::status::not_found);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Not found\n";
        }

        res.prepare_payload();
        http::write(socket, res, ec);
        socket.shutdown(tcp::socket::shutdown_send, ec);
        socket.close(ec);
    }
}    


int main() {
    std::signal(SIGINT,  handleSig);
    std::signal(SIGTERM, handleSig);

    MetricsCollector mc;
    mc.start();

    // we must wait a bit to have some metrics collected
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    run_http_server(8080, mc);

    mc.stop();
    std::cout << "Apagando...\n";
    return 0;
}
