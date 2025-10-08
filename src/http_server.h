#pragma once

#include <atomic>

class MetricsCollector;

/* Simple synchronous HTTP server using BOost.Beast */
void run_http_server(unsigned short port, MetricsCollector& mc, std::atomic<bool>& stopFlag);
