using System.Globalization;
using MoongTangE.MatchmakingApi.Models;

namespace MoongTangE.MatchmakingApi;

public sealed class MatchmakingStore
{
    private const int MaxServers = 1024;
    private const int MaxReservations = 4096;
    private const int MaxCommandsPerServer = 1024;
    private static readonly TimeSpan ServerTimeout = TimeSpan.FromSeconds(3);
    private static readonly TimeSpan RecordRetention = TimeSpan.FromSeconds(60);
    private readonly object sync = new();
    private readonly Dictionary<string, GameServerRecord> servers = [];
    private readonly Dictionary<string, ReservationRecord> reservations = [];
    private long nextCommandId = 1;

    internal IReadOnlyList<ControlCommand>? Heartbeat(
        string serverId,
        GameServerHeartbeat heartbeat,
        DateTimeOffset now)
    {
        lock (sync)
        {
            Prune(now);
            if (!servers.TryGetValue(serverId, out var server))
            {
                if (servers.Count >= MaxServers)
                {
                    return null;
                }
                server = new GameServerRecord
                {
                    Id = serverId,
                    Heartbeat = heartbeat,
                    LastSeen = now
                };
                servers.Add(serverId, server);
            }
            else
            {
                server.Heartbeat = heartbeat;
                server.LastSeen = now;
            }

            server.Commands.RemoveAll(command => server.CompletedCommands.Contains(command.Id));
            server.CompletedCommands.Clear();

            return server.Commands.OrderBy(command => command.Id).ToArray();
        }
    }

    internal object Status(DateTimeOffset now)
    {
        lock (sync)
        {
            Prune(now);
            var active = servers.Values
                .Where(server => now - server.LastSeen < ServerTimeout)
                .OrderBy(server => server.Id)
                .ToArray();
            return new
            {
                ready = active.Length > 0,
                servers = active.Select(server => new
                {
                    server_id = server.Id,
                    game_host = server.Heartbeat.Host,
                    game_port = server.Heartbeat.Port,
                    protocol_version = server.Heartbeat.ProtocolVersion,
                    capacity = server.Heartbeat.Capacity,
                    players = server.Heartbeat.Players,
                    reserved = server.Heartbeat.Reserved,
                    available = EffectiveAvailable(server)
                })
            };
        }
    }

    internal ReservationRecord? Reserve(string ticket, DateTimeOffset now)
    {
        lock (sync)
        {
            Prune(now);
            if (reservations.TryGetValue(ticket, out var existing))
            {
                return existing;
            }
            if (reservations.Count >= MaxReservations)
            {
                return null;
            }

            var server = servers.Values
                .Where(candidate => now - candidate.LastSeen < ServerTimeout)
                .Where(candidate => candidate.Commands.Count < MaxCommandsPerServer)
                .Where(candidate => EffectiveAvailable(candidate) > 0)
                .OrderByDescending(candidate => EffectiveAvailable(candidate))
                .ThenBy(candidate => candidate.Id)
                .FirstOrDefault();
            if (server is null)
            {
                return null;
            }

            var reservation = new ReservationRecord
            {
                Ticket = ticket,
                ServerId = server.Id,
                CreatedAt = now,
                ChangedAt = now
            };
            reservations.Add(ticket, reservation);
            server.Commands.Add(new ControlCommand(nextCommandId++, "reserve", ticket));
            return reservation;
        }
    }

    internal ReservationRecord? Lookup(string ticket, DateTimeOffset now)
    {
        lock (sync)
        {
            if (!reservations.TryGetValue(ticket, out var reservation))
            {
                return null;
            }
            if (!servers.TryGetValue(reservation.ServerId, out var server) ||
                now - server.LastSeen >= ServerTimeout)
            {
                reservation.Status = "unavailable";
                reservation.ChangedAt = now;
            }
            return reservation;
        }
    }

    internal ReservationRecord? Cancel(string ticket, DateTimeOffset now)
    {
        lock (sync)
        {
            if (!reservations.TryGetValue(ticket, out var reservation))
            {
                return null;
            }
            if (reservation.Status is "pending" or "reserved")
            {
                var server = servers[reservation.ServerId];
                if (server.Commands.Count >= MaxCommandsPerServer)
                {
                    reservation.Status = "unavailable";
                    reservation.ChangedAt = now;
                    return reservation;
                }
                reservation.Status = "cancelling";
                reservation.ChangedAt = now;
                server.Commands.Add(new ControlCommand(nextCommandId++, "cancel", ticket));
            }
            return reservation;
        }
    }

    internal bool ReportCommand(
        string serverId,
        long commandId,
        string status,
        ulong roomId,
        long expiresInMs,
        DateTimeOffset now)
    {
        lock (sync)
        {
            if (!servers.TryGetValue(serverId, out var server))
            {
                return false;
            }
            var command = server.Commands.FirstOrDefault(candidate => candidate.Id == commandId);
            if (command is null || !reservations.TryGetValue(command.Ticket, out var reservation))
            {
                return false;
            }

            server.CompletedCommands.Add(commandId);
            if (command.Action == "reserve" && reservation.Status == "cancelling")
            {
                return true;
            }
            UpdateReservation(reservation, status, roomId, expiresInMs, now);
            return true;
        }
    }

    internal bool ReportReservation(
        string serverId,
        string ticket,
        string status,
        ulong roomId,
        long expiresInMs,
        DateTimeOffset now)
    {
        lock (sync)
        {
            if (!reservations.TryGetValue(ticket, out var reservation) ||
                reservation.ServerId != serverId)
            {
                return false;
            }
            UpdateReservation(reservation, status, roomId, expiresInMs, now);
            return true;
        }
    }

    internal object PublicReservation(ReservationRecord reservation)
    {
        lock (sync)
        {
            var server = servers[reservation.ServerId];
            var publicStatus = reservation.Status == "cancelling" ? "pending" : reservation.Status;
            return new
            {
                status = publicStatus,
                server_id = reservation.ServerId,
                game_host = server.Heartbeat.Host,
                game_port = server.Heartbeat.Port,
                protocol_version = server.Heartbeat.ProtocolVersion,
                room_id = reservation.RoomId.ToString(CultureInfo.InvariantCulture),
                expires_in_ms = reservation.ExpiresInMs
            };
        }
    }

    private int EffectiveAvailable(GameServerRecord server)
    {
        var unreflected = reservations.Values.Count(reservation =>
            reservation.ServerId == server.Id &&
            reservation.ChangedAt >= server.LastSeen &&
            reservation.Status is "pending" or "reserved" or "cancelling");
        return Math.Max(0, server.Heartbeat.Available - unreflected);
    }

    private void Prune(DateTimeOffset now)
    {
        var expiredTickets = reservations.Values
            .Where(reservation => IsTerminal(reservation.Status) &&
                now - reservation.ChangedAt >= RecordRetention)
            .Select(reservation => reservation.Ticket)
            .ToArray();
        foreach (var ticket in expiredTickets)
        {
            reservations.Remove(ticket);
        }

        var staleServers = servers.Values
            .Where(server => now - server.LastSeen >= RecordRetention)
            .Where(server => reservations.Values.All(reservation => reservation.ServerId != server.Id))
            .Select(server => server.Id)
            .ToArray();
        foreach (var serverId in staleServers)
        {
            servers.Remove(serverId);
        }
    }

    private static bool IsTerminal(string status) =>
        status is "consumed" or "full" or "expired" or "cancelled" or "unavailable";

    private static void UpdateReservation(
        ReservationRecord reservation,
        string status,
        ulong roomId,
        long expiresInMs,
        DateTimeOffset now)
    {
        reservation.Status = status;
        reservation.RoomId = roomId;
        reservation.ExpiresInMs = expiresInMs;
        reservation.ChangedAt = now;
    }
}
