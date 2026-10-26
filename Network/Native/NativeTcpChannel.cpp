#include "Network/Native/NativeTcpChannel.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace MoongTangE {

NativeTcpChannel::NativeTcpChannel(NativeEventLoop& eventLoop,
                                   NativeEventLoop::ConnectionId connection)
    : m_eventLoop(eventLoop), m_connection(connection)
{
}

void NativeTcpChannel::Start(MessageHandler onMessage, CloseHandler onClose)
{
    if (m_started || m_closed || weak_from_this().expired() || !onMessage || !onClose) {
        throw std::logic_error("A channel starts once with both handlers installed");
    }

    m_onMessage = std::move(onMessage);
    m_onClose = std::move(onClose);
    m_started = true;
    ReadFrameHeader();
}

void NativeTcpChannel::Send(const wire::Bytes& payload)
{
    if (m_closed || m_draining) {
        return;
    }
    if (m_outboundFrames.size() >= MAX_QUEUED_FRAMES) {
        Close("Slow peer: send queue full");
        return;
    }

    auto bytes = std::make_shared<wire::Bytes>(wire::frame(payload));
    const bool idle = m_outboundFrames.empty();
    m_outboundFrames.push_back(std::move(bytes));
    if (idle) {
        WriteNextFrame();
    }
}

void NativeTcpChannel::SendAndClose(const wire::Bytes& payload, std::string reason)
{
    if (m_closed || m_draining) {
        return;
    }

    Send(payload);
    if (!m_closed) {
        m_draining = true;
        m_closeReason = std::move(reason);
    }
}

void NativeTcpChannel::Close(std::string reason)
{
    if (m_closed) {
        return;
    }

    m_closed = true;
    m_eventLoop.Cancel(m_readDeadline);
    m_eventLoop.Close(m_connection);
    m_outboundFrames.clear();

    if (m_onClose) {
        m_onClose(std::move(reason));
    }
}

void NativeTcpChannel::ReadFrameHeader()
{
    if (m_closed || m_draining) {
        return;
    }

    auto self = shared_from_this();
    m_eventLoop.Cancel(m_readDeadline);
    m_readDeadline = m_eventLoop.Schedule(READ_TIMEOUT, [self] {
        self->Close("Read timeout");
    });
    m_eventLoop.Read(m_connection, m_frameHeader, [self](std::string error) {
        if (!error.empty()) {
            self->Close(std::move(error));
            return;
        }

        try {
            self->m_frameBody.resize(wire::frame_size(self->m_frameHeader));
            self->ReadFrameBody();
        } catch (const std::exception& failure) {
            self->Close(failure.what());
        }
    });
}

void NativeTcpChannel::ReadFrameBody()
{
    auto self = shared_from_this();
    m_eventLoop.Read(m_connection, m_frameBody, [self](std::string error) {
        if (!error.empty()) {
            self->Close(std::move(error));
            return;
        }

        try {
            if (!self->m_closed && !self->m_draining) {
                self->m_onMessage(self->m_frameBody);
            }
        } catch (const std::exception& failure) {
            self->Close(failure.what());
        }

        self->ReadFrameHeader();
    });
}

void NativeTcpChannel::WriteNextFrame()
{
    auto self = shared_from_this();
    auto bytes = m_outboundFrames.front();
    m_eventLoop.Write(m_connection, *bytes, [self, bytes](std::string error) {
        if (!error.empty()) {
            self->Close(std::move(error));
            return;
        }
        if (self->m_closed) {
            return;
        }

        self->m_outboundFrames.pop_front();
        if (!self->m_outboundFrames.empty()) {
            self->WriteNextFrame();
        } else if (self->m_draining) {
            self->Close(self->m_closeReason);
        }
    });
}

} // namespace MoongTangE
