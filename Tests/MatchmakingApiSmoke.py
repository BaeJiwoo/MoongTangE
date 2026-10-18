"""Real HTTP -> reservation -> framed Protobuf TCP integration, including races."""
import concurrent.futures
import http.client
import json
import os
from pathlib import Path
import queue
import secrets
import socket
import struct
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Tools"))
from matchmaking_client import MatchmakingClient


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def wait_for(predicate, seconds=4):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.02)
    raise AssertionError("Timed out waiting for API state")


def receive_exact(peer, count):
    data = b""
    while len(data) < count:
        part = peer.recv(count - len(data))
        check(bool(part), "Unexpected EOF")
        data += part
    return data


def varint(data, offset):
    value = 0
    for shift in range(0, 70, 7):
        byte = data[offset]
        offset += 1
        value |= (byte & 127) << shift
        if byte < 128:
            return value, offset
    raise AssertionError("Bad varint")


def hello(game_port, ticket=""):
    peer = socket.create_connection(("127.0.0.1", game_port), timeout=3)
    try:
        payload = bytes([1, 8, 5])
        if ticket:
            payload += bytes([18, 64]) + ticket.encode("ascii")
        peer.sendall(struct.pack("!I", len(payload)) + payload)
        size = struct.unpack("!I", receive_exact(peer, 4))[0]
        check(size <= 8192, "Oversized frame")
        response = receive_exact(peer, size)
        check(response[0] == 2, "Expected server hello")
        fields = {}
        offset = 1
        while offset < len(response):
            tag, offset = varint(response, offset)
            check(tag & 7 == 0, "Unexpected hello field")
            fields[tag >> 3], offset = varint(response, offset)
        check(fields[1] == 5, "Wrong protocol version")
        return peer, fields
    except Exception:
        peer.close()
        raise


def main():
    api_key = secrets.token_hex(32)
    environment = dict(os.environ, MOONGTANGE_API_KEY=api_key)
    process = subprocess.Popen([sys.argv[1], "--port", "0", "--api-port", "0",
                                "--require-ticket", "--max-rooms", "2",
                                "--empty-room-seconds", "0", "--reservation-seconds", "3",
                                "--run-for", "18"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, env=environment)
    lines = queue.Queue()
    reader = threading.Thread(target=lambda: [lines.put(line) for line in process.stdout], daemon=True)
    reader.start()
    peers = []
    try:
        ports = {}
        deadline = time.monotonic() + 8
        while len(ports) != 2 and time.monotonic() < deadline:
            line = lines.get(timeout=8)
            if line.startswith("Listening "):
                ports["game"] = int(line.rsplit(":", 1)[1])
            if line.startswith("Matchmaking API "):
                ports["api"] = int(line.rsplit(":", 1)[1])
        check(len(ports) == 2, "Server did not publish both ports")
        api = MatchmakingClient("http://127.0.0.1:" + str(ports["api"]), api_key)
        def ready_state():
            code, result = api.request("GET", "/v1/status")
            return result if code == 200 else None
        state = wait_for(ready_state)
        instance = state["instance_id"]
        check(state["capacity"] == 6 and state["players_per_room"] == 3 and state["require_ticket"], "Wrong capacity")
        bad_api = MatchmakingClient("http://127.0.0.1:" + str(ports["api"]), "wrong")
        check(bad_api.request("GET", "/v1/status")[0] == 401, "Missing authentication")
        check(api.request("PUT", "/v1/reservations", "a" * 64, "old-instance")[0] == 409, "Old instance accepted")
        check(api.request("PUT", "/v1/reservations", "invalid", instance)[0] == 400, "Invalid ticket accepted")
        check(api.request("GET", "/v1/reservations", "f" * 64, instance)[0] == 404, "Unknown ticket accepted")
        peer, fields = hello(ports["game"])
        peer.close()
        check(fields[4] == 3, "Ticket-only mode bypassed")

        # Concurrent retries of one key must reserve exactly one seat.
        ticket = secrets.token_hex(32)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(lambda _: api.request("PUT", "/v1/reservations", ticket, instance), range(16)))
        wait_for(lambda: api.request("GET", "/v1/reservations", ticket, instance)[1]["status"] == "reserved")
        check(api.request("GET", "/v1/status")[1]["reserved"] == 1, "Duplicate reservation")
        peer, fields = hello(ports["game"], ticket)
        peers.append(peer)
        check(fields[4] == 1 and fields[3] == 1, "Reserved admission failed")
        replay, rejected = hello(ports["game"], ticket)
        replay.close()
        check(rejected[4] == 3, "Ticket replay accepted")
        check(api.request("PUT", "/v1/reservations", ticket, instance)[1]["status"] == "consumed", "Consumed result lost")

        # Exercise the actual adapter and fill both rooms; a seventh seat must fail.
        assignments = [api.reserve("127.0.0.1") for _ in range(5)]
        check([a["room_id"] for a in assignments] == ["1", "1", "2", "2", "2"], "First fit changed")
        full_ticket = secrets.token_hex(32)
        api.request("PUT", "/v1/reservations", full_ticket, instance)
        wait_for(lambda: api.request("GET", "/v1/reservations", full_ticket, instance)[1]["status"] == "full")
        state = api.request("GET", "/v1/status")[1]
        check(state["players"] == 1 and state["reserved"] == 5 and state["available"] == 0, "Bad occupancy")
        for assignment in assignments:
            connection, admitted = hello(ports["game"], assignment["ticket"])
            peers.append(connection)
            check(admitted[4] == 1 and str(admitted[3]) == assignment["room_id"], "Wrong room")
        peers[0].close()
        wait_for(lambda: api.request("GET", "/v1/status")[1]["players"] == 5)
        replacement = api.reserve("127.0.0.1")
        check(replacement["room_id"] == "1", "Vacancy was not reused")
        api.request("DELETE", "/v1/reservations", replacement["ticket"], instance)
        wait_for(lambda: api.request("GET", "/v1/reservations", replacement["ticket"], instance)[1]["status"] == "cancelled")
        check(api.request("PUT", "/v1/reservations", replacement["ticket"], instance)[0] == 410, "Cancelled ticket recreated")
        expiring = api.reserve("127.0.0.1")
        wait_for(lambda: api.request("GET", "/v1/reservations", expiring["ticket"], instance)[1]["status"] == "expired")
        expired, rejected = hello(ports["game"], expiring["ticket"])
        expired.close()
        check(rejected[4] == 3, "Expired ticket accepted")
        for connection in peers:
            connection.close()
        peers.clear()
        wait_for(lambda: api.request("GET", "/v1/status")[1]["available"] == 6)

        # Simultaneous independent allocations must never overbook either room.
        tickets = [secrets.token_hex(32) for _ in range(20)]
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(lambda key: api.request("PUT", "/v1/reservations", key, instance), tickets))
        def settled():
            results = [api.request("GET", "/v1/reservations", key, instance)[1]["status"] for key in tickets]
            return results if "pending" not in results else None
        results = wait_for(settled)
        check(results.count("reserved") == 6 and results.count("full") == 14, "Overbooked reservations")
        state = api.request("GET", "/v1/status")[1]
        check(all(room["players"] + room["reserved"] <= 3 for room in state["rooms"]), "Overbooked room")

        # Oversized bodies are rejected before handlers and never allocate a seat.
        connection = http.client.HTTPConnection("127.0.0.1", ports["api"], timeout=2)
        connection.request("PUT", "/v1/reservations", b"x" * 2048,
                           {"Authorization": "Bearer " + api_key})
        check(connection.getresponse().status == 413, "Unbounded HTTP body")
        connection.close()
        # Keep incomplete readers alive during shutdown; idle timeouts must drain workers.
        slow_peers = [socket.create_connection(("127.0.0.1", ports["api"]), timeout=2) for _ in range(4)]
        peers.extend(slow_peers)
        for connection in slow_peers:
            connection.sendall(b"GET /v1/status HTTP/1.1\r\nHost:")
        process.wait(timeout=22)
        check(process.returncode == 0, "Server failed during shutdown")
        print("HTTP authentication, idempotency, capacity races, ticket admission/replay, cancellation, expiry and shutdown passed")
    finally:
        for peer in peers:
            peer.close()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        reader.join(timeout=2)
        process.stdout.close()


if __name__ == "__main__":
    main()
