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
    explicit RunningApplication(MoongTangE::ServerApplication& application)
        : m_thread([this, &application](std::stop_token stop) {
              const std::stop_callback onStop(stop, [&application] {
                  application.requestStop();
              });
              try {
                  application.run();
                  m_finished.set_value();
              } catch (...) {
                  m_finished.set_exception(std::current_exception());
              }
          }) {}

    void finish() {
        auto completion = m_finished.get_future();
        MOONGTANGE_CHECK(completion.wait_for(3s) == std::future_status::ready);
        completion.get();
        m_thread.join();
    }

private:
    std::promise<void> m_finished;
    std::jthread m_thread;
};

void requestedStopDisconnectsClients() {
    MoongTangE::ServerApplication application({.m_port = 0});
    MOONGTANGE_CHECK(application.port() == 0);
    RunningApplication running(application);
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return application.port() != 0;
    }));
    MoongTangE::TcpClientTransport client("127.0.0.1", application.port());
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return client.connected();
    }));
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return client.receive().has_value();
    }));
    application.requestStop();
    application.requestStop();
    running.finish();
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return !client.connected();
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        application.run();
    }));
}

void deadlineStopsApplication() {
    MoongTangE::ServerApplication application({.m_port = 0, .m_runDuration = 1s});
    const auto started = std::chrono::steady_clock::now();
    RunningApplication running(application);
    running.finish();
    MOONGTANGE_CHECK(application.port() != 0);
    MOONGTANGE_CHECK(std::chrono::steady_clock::now() - started >= 1s);
}

void startupFailureAndPriorCancellation() {
    asio::io_context io;
    asio::ip::tcp::acceptor reserved(io, {asio::ip::make_address("127.0.0.1"), 0});
    MoongTangE::ServerApplication unavailable({.m_port = reserved.local_endpoint().port()});
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        unavailable.run();
    }));
    MOONGTANGE_CHECK(unavailable.port() == 0);
    MoongTangE::ServerApplication cancelled({.m_port = reserved.local_endpoint().port()});
    cancelled.requestStop();
    cancelled.run();
    MOONGTANGE_CHECK(cancelled.port() == 0);
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        MoongTangE::ServerApplication invalid({.m_runDuration = -1s});
    }));
}
} // namespace

int main() {
    return MoongTangE::test::run([] {
        requestedStopDisconnectsClients();
        deadlineStopsApplication();
        startupFailureAndPriorCancellation();
    });
}
