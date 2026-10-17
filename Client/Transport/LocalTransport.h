#pragma once
#include "Common/Transport/IClientTransport.h"
#include "Server/Core/ServerHost.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace sandbox {
class LocalTransport final : public IClientTransport {
public:
    explicit LocalTransport(std::shared_ptr<ServerHost> host) : host_(std::move(host)) {
        if (!host_) {
            throw std::invalid_argument("Local transport requires a server");
        }
        id_ = host_->connect();
        if (id_ == 0) {
            throw std::runtime_error("Local server unavailable or full");
        }
    }

    ~LocalTransport() override {
        close();
    }

    LocalTransport(const LocalTransport&) = delete;
    LocalTransport& operator=(const LocalTransport&) = delete;
    LocalTransport(LocalTransport&&) = delete;
    LocalTransport& operator=(LocalTransport&&) = delete;

    bool connected() const override {
        return id_ && host_->hasClient(id_);
    }

    std::uint32_t playerId() const override {
        return id_;
    }

    bool send(InputCommand input) override {
        return connected() && input.playerId == id_ && host_->submit(input);
    }

    std::optional<WorldSnapshot> receive() override {
        if (!connected()) {
            return {};
        }
        auto state = host_->snapshot();
        if (!state.tick || state.tick == lastTick_) {
            return {};
        }
        lastTick_ = state.tick;
        return state;
    }

    std::string error() const override {
        return connected() ? "" : "Local server disconnected";
    }

    void close() override {
        if (id_) {
            host_->disconnect(id_);
            id_ = 0;
        }
    }

private:
    std::shared_ptr<ServerHost> host_;
    std::uint32_t id_{};
    std::uint64_t lastTick_{};
};
} // namespace sandbox
