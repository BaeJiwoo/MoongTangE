#include "Client/Transport/TcpClientTransport.h"
#include "Server/ServerApplication.h"
#include "TcpTestSocket.h"
#include "TestSupport.h"

#include <chrono>
#include <exception>
#include <future>
#include <stop_token>
#include <thread>

namespace {

using namespace std::chrono_literals;

// A failing assertion also stops and joins the application before it is destroyed.
class RunningApplication
{
  public:
    explicit RunningApplication(MoongTangE::ServerApplication& application)
        : m_thread([this, &application](std::stop_token stop) {
              const std::stop_callback onStop(stop, [&application] {
                  application.RequestStop();
              });
              try {
                  application.Run();
                  m_finished.set_value();
              } catch (...) {
                  m_finished.set_exception(std::current_exception());
              }
          })
    {
    }

    void Finish()
    {
        auto completion = m_finished.get_future();
        MOONGTANGE_CHECK(completion.wait_for(3s) == std::future_status::ready);
        completion.get();
        m_thread.join();
    }

  private:
    std::promise<void> m_finished;
    std::jthread m_thread;
};

void requested_stop_disconnects_clients()
{
    MoongTangE::ServerApplication application({.m_port = 0});
    MOONGTANGE_CHECK(application.Port() == 0);
    RunningApplication running(application);
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return application.Port() != 0;
    }));

    MoongTangE::TcpClientTransport client("127.0.0.1", application.Port());
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return client.Connected();
    }));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return client.Receive().has_value();
    }));

    application.RequestStop();
    application.RequestStop();
    running.Finish();
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return !client.Connected();
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        application.Run();
    }));
}

void deadline_stops_application()
{
    MoongTangE::ServerApplication application({.m_port = 0, .m_runDuration = 1s});
    const auto started = std::chrono::steady_clock::now();
    RunningApplication running(application);
    running.Finish();
    MOONGTANGE_CHECK(application.Port() != 0);
    MOONGTANGE_CHECK(std::chrono::steady_clock::now() - started >= 1s);
}

void startup_failure_and_prior_cancellation()
{
    MoongTangE::test::TcpTestListener reserved;
    MoongTangE::ServerApplication unavailable({.m_port = reserved.Port()});
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        unavailable.Run();
    }));
    MOONGTANGE_CHECK(unavailable.Port() == 0);

    MoongTangE::ServerApplication cancelled({.m_port = reserved.Port()});
    cancelled.RequestStop();
    cancelled.Run();
    MOONGTANGE_CHECK(cancelled.Port() == 0);
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        MoongTangE::ServerApplication invalid({.m_runDuration = -1s});
    }));
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        requested_stop_disconnects_clients();
        deadline_stops_application();
        startup_failure_and_prior_cancellation();
    });
}
