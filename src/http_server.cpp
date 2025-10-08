
#include <http_server.h>
#include <metrics_collector.h>

#include <iostream>
#include <chrono>

// Boost.Beast/Asio
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp = net::ip::tcp;

void run_http_server(unsigned short port, MetricsCollector& mc, std::atomic<bool>& stopFlag) {
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
