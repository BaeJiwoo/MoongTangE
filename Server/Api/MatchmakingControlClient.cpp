#include "Server/Api/MatchmakingControlClient.h"

#include "Common/Protocol/AdmissionTicket.h"
#include "Network/Protocol/Codec.h"
#include "Server/Core/ServerHost.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <httplib.h>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace MoongTangE {
namespace {

constexpr auto CONTROL_INTERVAL = std::chrono::seconds(1);
constexpr std::size_t MAX_CONTROL_BODY = 128 * 1024;

std::string_view reservation_status(ReservationStatus status)
{
    switch (status) {
    case ReservationStatus::RESERVED:
        return "reserved";
    case ReservationStatus::CONSUMED:
        return "consumed";
    case ReservationStatus::FULL:
        return "full";
    case ReservationStatus::EXPIRED:
        return "expired";
    case ReservationStatus::CANCELLED:
        return "cancelled";
    case ReservationStatus::UNAVAILABLE:
    case ReservationStatus::UNKNOWN:
    case ReservationStatus::PENDING:
        return "unavailable";
    }
    return "unavailable";
}

bool terminal(ReservationStatus status)
{
    return status != ReservationStatus::UNKNOWN && status != ReservationStatus::PENDING;
}

std::string report_body(ReservationResult result)
{
    return std::string(reservation_status(result.status)) + '\t' + std::to_string(result.roomId) +
           '\t' + std::to_string(std::max<std::int64_t>(0, result.expiresInMs));
}

} // namespace

MatchmakingControlClient::MatchmakingControlClient(std::shared_ptr<ServerHost> host,
                                                   MatchmakingControlOptions options)
    : m_host(std::move(host)), m_options(std::move(options)),
      m_httpClient(std::make_unique<httplib::Client>(m_options.m_url))
{
    if (!m_host || !m_host->Running() || !m_options.m_url.starts_with("http://") ||
        m_options.m_serverId.empty() || m_options.m_advertisedHost.empty() ||
        m_options.m_gamePort == 0 || m_options.m_controlKey.size() < 32 ||
        m_options.m_controlKey.size() > 128 ||
        !std::all_of(m_options.m_controlKey.begin(), m_options.m_controlKey.end(), [](char value) {
            return value >= '!' && value <= '~';
        })) {
        throw std::invalid_argument("Invalid matchmaking control configuration");
    }

    m_httpClient->set_connection_timeout(1, 0);
    m_httpClient->set_read_timeout(1, 0);
    m_httpClient->set_write_timeout(1, 0);
    m_controlThread = std::jthread([this](std::stop_token stopToken) {
        Run(stopToken);
    });
}

MatchmakingControlClient::~MatchmakingControlClient()
{
    if (m_controlThread.joinable()) {
        m_controlThread.request_stop();
        m_controlThread.join();
    }
}

void MatchmakingControlClient::Run(std::stop_token stopToken)
{
    while (!stopToken.stop_requested()) {
        try {
            Synchronize();
        } catch (...) {
            // The game loop remains available while the independent control plane is offline.
        }
        std::this_thread::sleep_for(CONTROL_INTERVAL);
    }
}

void MatchmakingControlClient::Synchronize()
{
    const auto commands = Heartbeat();
    if (!commands.empty()) {
        AcceptCommands(commands);
    }
    ReportCommands();
    ReportReservationChanges();
}

std::string MatchmakingControlClient::Heartbeat()
{
    const auto state = m_host->Matchmaking();
    if (!state.running || state.tick == 0) {
        return {};
    }

    std::size_t players = 0;
    std::size_t reserved = 0;
    for (const auto& room : state.rooms) {
        players += room.players;
        reserved += room.reserved;
    }
    const auto capacity = m_host->Capacity();
    const auto available = capacity - players - reserved;
    const httplib::Headers headers{
        {"Authorization", "Bearer " + m_options.m_controlKey},
        {"X-Game-Host", m_options.m_advertisedHost},
        {"X-Game-Port", std::to_string(m_options.m_gamePort)},
        {"X-Protocol-Version", std::to_string(wire::PROTOCOL_VERSION)},
        {"X-Capacity", std::to_string(capacity)},
        {"X-Players", std::to_string(players)},
        {"X-Reserved", std::to_string(reserved)},
        {"X-Available", std::to_string(available)},
        {"X-Reservation-Seconds", std::to_string(m_options.m_reservationLifetime.count())},
    };
    const auto response = m_httpClient->Put("/internal/v1/game-servers/" + m_options.m_serverId,
                                            headers, "", "text/plain");
    return response && response->status == 200 && response->body.size() <= MAX_CONTROL_BODY
               ? response->body
               : std::string{};
}

void MatchmakingControlClient::AcceptCommands(const std::string& body)
{
    std::istringstream lines(body);
    for (std::string line; std::getline(lines, line);) {
        const auto firstTab = line.find('\t');
        const auto secondTab =
            firstTab == std::string::npos ? std::string::npos : line.find('\t', firstTab + 1);
        if (firstTab == std::string::npos || secondTab == std::string::npos ||
            line.find('\t', secondTab + 1) != std::string::npos) {
            continue;
        }

        std::uint64_t commandId = 0;
        const auto id = std::string_view(line).substr(0, firstTab);
        const auto [end, error] = std::from_chars(id.data(), id.data() + id.size(), commandId);
        const auto action = line.substr(firstTab + 1, secondTab - firstTab - 1);
        const auto ticket = line.substr(secondTab + 1);
        if (error != std::errc{} || end != id.data() + id.size() || commandId == 0 ||
            (action != "reserve" && action != "cancel") || !valid_admission_ticket(ticket) ||
            m_commands.contains(commandId)) {
            continue;
        }

        m_commands.emplace(commandId, CommandState{commandId, action, ticket});
        if (action == "reserve") {
            m_host->Reserve(ticket);
        } else {
            m_host->CancelReservation(ticket);
        }
    }
}

void MatchmakingControlClient::ReportCommands()
{
    for (auto entry = m_commands.begin(); entry != m_commands.end();) {
        auto result = m_host->Reservation(entry->second.m_ticket);
        if (entry->second.m_action == "cancel" && result.status == ReservationStatus::UNKNOWN) {
            result.status = ReservationStatus::UNAVAILABLE;
        }
        if (!terminal(result.status)) {
            ++entry;
            continue;
        }

        const auto path = "/internal/v1/game-servers/" + m_options.m_serverId + "/commands/" +
                          std::to_string(entry->second.m_id);
        if (!Report(path, report_body(result))) {
            ++entry;
            continue;
        }
        if (result.status == ReservationStatus::RESERVED) {
            m_reportedStatuses[entry->second.m_ticket] = static_cast<int>(result.status);
        } else {
            m_reportedStatuses.erase(entry->second.m_ticket);
        }
        entry = m_commands.erase(entry);
    }
}

void MatchmakingControlClient::ReportReservationChanges()
{
    for (auto entry = m_reportedStatuses.begin(); entry != m_reportedStatuses.end();) {
        const auto result = m_host->Reservation(entry->first);
        if (!terminal(result.status) || static_cast<int>(result.status) == entry->second) {
            ++entry;
            continue;
        }

        const auto path =
            "/internal/v1/game-servers/" + m_options.m_serverId + "/reservations/" + entry->first;
        if (!Report(path, report_body(result))) {
            ++entry;
            continue;
        }
        entry->second = static_cast<int>(result.status);
        if (result.status != ReservationStatus::RESERVED) {
            entry = m_reportedStatuses.erase(entry);
        } else {
            ++entry;
        }
    }
}

bool MatchmakingControlClient::Report(const std::string& path, const std::string& body)
{
    const httplib::Headers headers{{"Authorization", "Bearer " + m_options.m_controlKey}};
    const auto response = m_httpClient->Put(path, headers, body, "text/plain");
    return response && response->status == 204;
}

} // namespace MoongTangE
