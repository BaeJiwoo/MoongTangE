#include "Client/Transport/TcpClientTransport.h"
#include "Server/ServerApplication.h"
#include "TestSupport.h"
#include <asio.hpp>
#include <chrono>
#include <exception>
#include <future>
#include <stop_token>
#include <thread>

namespace {
using namespace std::chrono_literals;

// A failing assertion also stops and joins the application before it is destroyed.
class RunningApplication {
public:
    explicit RunningApplication(sandbox::ServerApplication& application)
        : thread_([this, &application](std::stop_token stop) {
              const std::stop_callback onStop(stop, [&application] {
                  application.requestStop();
              });
              try {
                  application.run();
                  finished_.set_value();
              } catch (...) {
                  finished_.set_exception(std::current_exception());
              }
          }) {}

    void finish() {
        auto completion = finished_.get_future();
        CHECK(completion.wait_for(3s) == std::future_status::ready);
        completion.get();
        thread_.join();
    }

private:
    std::promise<void> finished_;
    std::jthread thread_;
};

void requestedStopDisconnectsClients() {
    sandbox::ServerApplication application({.port = 0});
    CHECK(application.port() == 0);
    RunningApplication running(application);
    CHECK(test::waitFor([&] {
        return application.port() != 0;
    }));
    sandbox::TcpClientTransport client("127.0.0.1", application.port());
    CHECK(test::waitFor([&] {
        return client.connected();
    }));
    CHECK(test::waitFor([&] {
        return client.receive().has_value();
    }));
    application.requestStop();
    application.requestStop();
    running.finish();
    CHECK(test::waitFor([&] {
        return !client.connected();
    }));
    CHECK(test::throws([&] {
        application.run();
    }));
}

void deadlineStopsApplication() {
    sandbox::ServerApplication application({.port = 0, .duration = 1s});
    const auto started = std::chrono::steady_clock::now();
    RunningApplication running(application);
    running.finish();
    CHECK(application.port() != 0);
    CHECK(std::chrono::steady_clock::now() - started >= 1s);
}

void startupFailureAndPriorCancellation() {
    asio::io_context io;
    asio::ip::tcp::acceptor reserved(io, {asio::ip::make_address("127.0.0.1"), 0});
    sandbox::ServerApplication unavailable({.port = reserved.local_endpoint().port()});
    CHECK(test::throws([&] {
        unavailable.run();
    }));
    CHECK(unavailable.port() == 0);
    sandbox::ServerApplication cancelled({.port = reserved.local_endpoint().port()});
    cancelled.requestStop();
    cancelled.run();
    CHECK(cancelled.port() == 0);
    CHECK(test::throws([] {
        sandbox::ServerApplication invalid({.duration = -1s});
    }));
}
} // namespace

int main() {
    return test::run([] {
        requestedStopDisconnectsClients();
        deadlineStopsApplication();
        startupFailureAndPriorCancellation();
    });
}
