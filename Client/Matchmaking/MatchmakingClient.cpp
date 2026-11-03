#include "Client/Matchmaking/MatchmakingClient.h"

#include "Common/Utility/CommandLine.h"
#include "Network/Protocol/Codec.h"

#include <algorithm>
#include <condition_variable>
#include <google/protobuf/json/json.h>
#include <httplib.h>
#include <matchmaking.pb.h>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace MoongTangE {
namespace {

using Clock = std::chrono::steady_clock;
constexpr auto MATCH_TIMEOUT = std::chrono::seconds(15);
constexpr auto POLL_INTERVAL = std::chrono::milliseconds(250);
constexpr std::size_t MAX_RESPONSE_BYTES = 4096;
constexpr unsigned MAX_SESSION_ATTEMPTS = 64;
constexpr std::string_view RESERVATIONS = "/client/matchmaking/reservations";

bool is_hex(std::string_view value, std::size_t length)
{
    return value.size() == length && std::ranges::all_of(value, [](char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

bool valid_host(std::string_view host)
{
    return !host.empty() && host.size() <= 253 && std::ranges::all_of(host, [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') || character == '-' || character == '.' ||
               character == ':';
    });
}

std::string validated_origin(std::string origin)
{
    const bool secure = origin.starts_with("https://");
    if (!secure && !origin.starts_with("http://")) {
        throw std::invalid_argument("API URL must use https:// or local http://");
    }
    if (origin.ends_with('/')) {
        origin.pop_back();
    }
    auto authority = origin.substr(secure ? 8 : 7);
    if (authority.empty() || authority.find_first_of("/?#@\\ \t\r\n") != std::string::npos) {
        throw std::invalid_argument("API URL must contain only an origin");
    }
    if (authority.back() == ']' || authority.find(':') == std::string::npos) {
        authority += secure ? ":443" : ":80";
    }
    const auto endpoint = endpoint_option(authority);
    if (!valid_host(endpoint.host) || (!secure && endpoint.host != "127.0.0.1" &&
                                       endpoint.host != "::1" && endpoint.host != "localhost")) {
        throw std::invalid_argument("Remote API connections require HTTPS");
    }
    return origin;
}

std::string new_attempt_id()
{
    std::random_device random;
    constexpr std::string_view HEX = "0123456789abcdef";
    std::string result(32, '0');
    for (auto& character : result) {
        character = HEX[random() & 15U];
    }
    return result;
}

void check_running(std::stop_token stopToken, Clock::time_point deadline)
{
    if (stopToken.stop_requested()) {
        throw std::runtime_error("Cancelled");
    }
    if (Clock::now() >= deadline) {
        throw std::runtime_error("Matchmaking timed out; try again");
    }
}

void pause_request(std::stop_token stopToken, Clock::time_point deadline,
                   std::chrono::milliseconds duration)
{
    std::mutex mutex;
    std::unique_lock lock(mutex);
    std::condition_variable_any condition;
    condition.wait_until(lock, stopToken, std::min(deadline, Clock::now() + duration), [] {
        return false;
    });
    check_running(stopToken, deadline);
}

template <typename Message> Message parse_json(const httplib::Response& response)
{
    Message message;
    if (!response.get_header_value("Content-Type").starts_with("application/json") ||
        !google::protobuf::json::JsonStringToMessage(response.body, &message).ok()) {
        throw std::runtime_error("Invalid matchmaking API response");
    }
    return message;
}

void configure_http(httplib::Client& http)
{
    http.set_connection_timeout(2);
    http.set_read_timeout(2);
    http.set_write_timeout(2);
    http.set_max_timeout(2000);
    http.set_payload_max_length(MAX_RESPONSE_BYTES);
    http.set_follow_location(false);
    http.enable_server_certificate_verification(true);
    http.enable_server_hostname_verification(true);
}

} // namespace

class MatchmakingClient::GuestApiSession
{
  public:
    GuestApiSession(std::string origin, std::string caFile);
    GameAssignment Match(std::stop_token stopToken,
                         const std::function<void(ConnectionStage)>& progress);
    void CancelReservation() noexcept;
    void CompleteReservation();

  private:
    httplib::Response Request(const std::string& method, const std::string& path,
                              std::stop_token stopToken, Clock::time_point deadline);

    std::string m_origin;
    std::string m_caFile;
    httplib::Client m_http;
    std::string m_token;
    Clock::time_point m_sessionExpiresAt{};
    std::string m_attemptId;
    unsigned m_attemptCount{};
};

MatchmakingClient::GuestApiSession::GuestApiSession(std::string origin, std::string caFile)
    : m_origin(validated_origin(std::move(origin))), m_caFile(std::move(caFile)), m_http(m_origin)
{
    configure_http(m_http);
    if (!m_caFile.empty()) {
        m_http.set_ca_cert_path(m_caFile);
    }
}

httplib::Response MatchmakingClient::GuestApiSession::Request(const std::string& method,
                                                              const std::string& path,
                                                              std::stop_token stopToken,
                                                              Clock::time_point deadline)
{
    while (true) {
        check_running(stopToken, deadline);
        httplib::Request request;
        request.method = method;
        request.path = path;
        request.headers.emplace("Accept", "application/json");
        if (!m_token.empty()) {
            request.headers.emplace("Authorization", "Bearer " + m_token);
        }
        if (!m_attemptId.empty()) {
            request.headers.emplace("Idempotency-Key", m_attemptId);
        }
        auto response = m_http.send(request);
        check_running(stopToken, deadline);
        if (!response && response.error() != httplib::Error::Connection &&
            response.error() != httplib::Error::Read && response.error() != httplib::Error::Write &&
            response.error() != httplib::Error::ConnectionTimeout) {
            throw std::runtime_error(std::string("API: ") + httplib::to_string(response.error()));
        }
        if (response && response->status != 429) {
            return std::move(*response);
        }
        auto delay = POLL_INTERVAL;
        if (response) {
            try {
                const auto seconds = unsigned_option(response->get_header_value("Retry-After"), 60);
                delay = std::chrono::milliseconds(seconds * 1000);
                if (Clock::now() + delay >= deadline) {
                    throw std::runtime_error("Matchmaking is busy; try again later");
                }
            } catch (const std::invalid_argument&) {
                throw std::runtime_error("Invalid API retry delay");
            }
        }
        pause_request(stopToken, deadline, delay);
    }
}

GameAssignment
MatchmakingClient::GuestApiSession::Match(std::stop_token stopToken,
                                          const std::function<void(ConnectionStage)>& progress)
{
    const auto deadline = Clock::now() + MATCH_TIMEOUT;
    const std::stop_callback cancellation(stopToken, [this] {
        m_http.stop();
    });
    if (m_token.empty() || m_sessionExpiresAt <= deadline + std::chrono::seconds(5) ||
        m_attemptCount >= MAX_SESSION_ATTEMPTS) {
        m_token.clear();
        progress(ConnectionStage::SESSION);
        const auto startedAt = Clock::now();
        const auto response = Request("POST", "/client/sessions", stopToken, deadline);
        if (response.status != 201) {
            throw std::runtime_error("Could not create a guest session");
        }
        const auto session = parse_json<proto::GuestSession>(response);
        if (!is_hex(session.access_token(), 64) || !is_hex(session.guest_id(), 32) ||
            session.expires_in_ms() < 20000 || session.expires_in_ms() > 1800000) {
            throw std::runtime_error("Invalid guest session response");
        }
        m_token = session.access_token();
        m_attemptCount = 0;
        m_sessionExpiresAt = startedAt + std::chrono::milliseconds(session.expires_in_ms());
    }
    progress(ConnectionStage::MATCHING);
    m_attemptId = new_attempt_id();
    ++m_attemptCount;
    bool creating = true;
    while (true) {
        const auto startedAt = Clock::now();
        const auto response = Request(
            creating ? "POST" : "GET",
            std::string(RESERVATIONS) + (creating ? "" : "/" + m_attemptId), stopToken, deadline);
        creating = false;
        if (response.status == 401) {
            m_token.clear();
            throw std::runtime_error("Guest session expired; try again");
        }
        if (response.status == 503) {
            throw std::runtime_error("No game server is available");
        }
        if (response.status == 409) {
            throw std::runtime_error("Game server is full; try again");
        }
        if (response.status == 410) {
            throw std::runtime_error("Reservation expired; try again");
        }
        if (response.status != 200 && response.status != 202) {
            throw std::runtime_error("Matchmaking request was rejected");
        }
        const auto assignment = parse_json<proto::MatchAssignment>(response);
        if (response.status == 200 && assignment.status() == "reserved") {
            if (!valid_host(assignment.game_host()) || assignment.game_port() == 0 ||
                assignment.game_port() > 65535 ||
                assignment.protocol_version() != wire::PROTOCOL_VERSION ||
                assignment.room_id() == 0 || !is_hex(assignment.admission_ticket(), 64) ||
                assignment.expires_in_ms() <= 0 || assignment.expires_in_ms() > 300000) {
                throw std::runtime_error("Invalid or incompatible game server assignment");
            }
            const auto expiresAt =
                startedAt + std::chrono::milliseconds(assignment.expires_in_ms());
            if (expiresAt <= Clock::now()) {
                throw std::runtime_error("Reservation expired; try again");
            }
            return {assignment.game_host(), static_cast<std::uint16_t>(assignment.game_port()),
                    assignment.room_id(), assignment.admission_ticket(), expiresAt};
        }
        if (response.status != 202 || assignment.status() != "pending") {
            throw std::runtime_error("Unexpected matchmaking state");
        }
        pause_request(stopToken, deadline, POLL_INTERVAL);
    }
}

void MatchmakingClient::GuestApiSession::CancelReservation() noexcept
{
    if (m_attemptId.empty()) {
        return;
    }
    try {
        httplib::Client cleanup(m_origin);
        configure_http(cleanup);
        if (!m_caFile.empty()) {
            cleanup.set_ca_cert_path(m_caFile);
        }
        cleanup.set_connection_timeout(1);
        cleanup.set_max_timeout(1000);
        cleanup.Delete(std::string(RESERVATIONS) + "/" + m_attemptId,
                       httplib::Headers{{"Authorization", "Bearer " + m_token}});
    } catch (...) {
        // Best effort: server-side TTL remains authoritative after a lost cancellation.
    }
    m_attemptId.clear();
}

void MatchmakingClient::GuestApiSession::CompleteReservation()
{
    m_attemptId.clear();
}

MatchmakingClient::MatchmakingClient(std::string origin, std::string caFile)
    : m_session(std::make_unique<GuestApiSession>(std::move(origin), std::move(caFile)))
{
}

MatchmakingClient::~MatchmakingClient() = default;

GameAssignment MatchmakingClient::Match(std::stop_token stopToken,
                                        const std::function<void(ConnectionStage)>& progress)
{
    return m_session->Match(stopToken, progress);
}

void MatchmakingClient::CancelReservation() noexcept
{
    m_session->CancelReservation();
}

void MatchmakingClient::CompleteReservation()
{
    m_session->CompleteReservation();
}

} // namespace MoongTangE
