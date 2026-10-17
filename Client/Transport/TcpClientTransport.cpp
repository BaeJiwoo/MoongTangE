#include "Client/Transport/TcpClientTransport.h"
#include "Network/TcpChannel.h"
#include <mutex>
#include <thread>
namespace sandbox {
struct TcpClientTransport::Impl {
    asio::io_context io;
    asio::executor_work_guard<asio::io_context::executor_type> work{io.get_executor()};
    asio::ip::tcp::resolver resolver{io};
    asio::ip::tcp::socket socket{io};
    asio::steady_timer deadline{io};
    std::shared_ptr<TcpChannel> channel;
    mutable std::mutex mutex;
    std::uint32_t id{};
    std::size_t queued{};
    std::optional<WorldSnapshot> latest;
    std::string failure;
    bool stopped{};
    bool terminal{}; // Only accessed on the io_context thread.
    std::thread thread;
    Impl(const std::string& host,std::uint16_t port) {
        deadline.expires_after(std::chrono::seconds(5));
        deadline.async_wait([this](asio::error_code error) { if (!error) fail("Connect/handshake timeout"); });
        resolver.async_resolve(host,std::to_string(port),[this](asio::error_code error,asio::ip::tcp::resolver::results_type endpoints) {
            if (terminal) return;
            if (error) { fail(error.message()); return; }
            asio::async_connect(socket,endpoints,[this](asio::error_code error,const asio::ip::tcp::endpoint&) {
                if (terminal) return;
                if (error) { fail(error.message()); return; }
                channel=std::make_shared<TcpChannel>(std::move(socket));
                channel->onClose=[this](std::string reason) { std::lock_guard lock(mutex); id=0; if (failure.empty()) failure=std::move(reason); };
                channel->onMessage=[this](const wire::Bytes& data) {
                    std::lock_guard lock(mutex);
                    if (!id) { id=wire::readHello(data,true); deadline.cancel(); }
                    else { auto state=wire::readSnapshot(data); if (!latest || state.tick>latest->tick) latest=std::move(state); }
                };
                channel->start(); channel->send(wire::hello());
            });
        });
        thread=std::thread([this] { io.run(); });
    }
    void fail(std::string reason) {
        terminal=true;
        { std::lock_guard lock(mutex); if (failure.empty()) failure=std::move(reason); id=0; }
        resolver.cancel(); deadline.cancel();
        asio::error_code ignored; socket.close(ignored);
        if (channel) channel->close("Connection failed");
    }
    void stop() {
        { std::lock_guard lock(mutex); if (stopped) return; stopped=true; id=0; }
        asio::post(io,[this] { fail("Client closed"); work.reset(); });
        if (thread.joinable()) thread.join();
    }
    ~Impl() { stop(); }
};
TcpClientTransport::TcpClientTransport(const std::string& host,std::uint16_t port) : impl_(std::make_unique<Impl>(host,port)) {}
TcpClientTransport::~TcpClientTransport()=default;
bool TcpClientTransport::connected() const { std::lock_guard lock(impl_->mutex); return impl_->id && !impl_->stopped; }
std::uint32_t TcpClientTransport::playerId() const { std::lock_guard lock(impl_->mutex); return impl_->id; }
bool TcpClientTransport::send(InputCommand input) {
    {
        std::lock_guard lock(impl_->mutex);
        if (!input.valid() || impl_->stopped || !impl_->id || input.playerId!=impl_->id || impl_->queued>=64) return false;
        ++impl_->queued;
    }
    asio::post(impl_->io,[p=impl_.get(),data=wire::input(input)] {
        { std::lock_guard lock(p->mutex); --p->queued; }
        if (p->channel) p->channel->send(data);
    });
    return true;
}
std::optional<WorldSnapshot> TcpClientTransport::receive() { std::lock_guard lock(impl_->mutex); auto state=std::move(impl_->latest); impl_->latest.reset(); return state; }
std::string TcpClientTransport::error() const { std::lock_guard lock(impl_->mutex); return impl_->failure; }
void TcpClientTransport::close() { impl_->stop(); }
}
