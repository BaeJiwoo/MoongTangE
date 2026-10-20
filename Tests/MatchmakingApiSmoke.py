"""Separate ASP.NET API -> C++ control client -> framed TCP admission smoke test."""
import concurrent.futures
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
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Tools"))
from matchmaking_client import MatchmakingClient


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def wait_for(predicate, seconds=8):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            value = predicate()
            if value:
                return value
        except (OSError, ValueError):
            pass
        time.sleep(0.05)
    raise AssertionError("Timed out waiting for service state")


def unused_port():
    peer = socket.socket()
    peer.bind(("127.0.0.1", 0))
    port = peer.getsockname()[1]
    peer.close()
    return port


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


def hello(game_port, ticket="", nickname="SmokePlayer"):
    peer = socket.create_connection(("127.0.0.1", game_port), timeout=3)
    try:
        payload = bytes([1, 8, 7])
        if ticket:
            payload += bytes([18, 64]) + ticket.encode("ascii")
        payload += bytes([26, len(nickname)]) + nickname.encode("ascii")
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
        return peer, fields
    except Exception:
        peer.close()
        raise


def terminate(process):
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def main():
    server_executable = sys.argv[1]
    api_project = sys.argv[2]
    api_port = unused_port()
    public_key = secrets.token_hex(32)
    control_key = secrets.token_hex(32)
    api_url = f"http://127.0.0.1:{api_port}"
    api_environment = dict(os.environ,
                           ASPNETCORE_URLS=api_url,
                           MOONGTANGE_PUBLIC_API_KEY=public_key,
                           MOONGTANGE_CONTROL_KEY=control_key)
    api_process = subprocess.Popen(
        ["dotnet", "run", "--configuration", "Release", "--project", api_project],
        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, env=api_environment)
    game_process = None
    peers = []
    try:
        wait_for(lambda: urllib.request.urlopen(api_url + "/health", timeout=1).status == 200)
        api = MatchmakingClient(api_url, public_key)
        check(api.request("GET", "/matchmaking/status")[1]["ready"] is False,
              "API unexpectedly has an in-process game server")

        game_environment = dict(os.environ, MOONGTANGE_CONTROL_KEY=control_key)
        game_process = subprocess.Popen(
            [server_executable, "--port", "0", "--require-ticket", "--max-rooms", "2",
             "--reservation-seconds", "2", "--run-for", "18",
             "--matchmaking-url", api_url, "--server-id", "smoke-1",
             "--advertise-host", "127.0.0.1"],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=game_environment)
        game_output = queue.Queue()
        reader = threading.Thread(
            target=lambda: [game_output.put(line) for line in game_process.stdout], daemon=True)
        reader.start()
        game_port = None
        deadline = time.monotonic() + 8
        while game_port is None and time.monotonic() < deadline:
            line = game_output.get(timeout=8)
            if line.startswith("Listening "):
                game_port = int(line.rsplit(":", 1)[1])
        check(game_port is not None, "C++ server did not publish its TCP port")

        state = wait_for(lambda: (lambda result: result if result["ready"] else None)(
            api.request("GET", "/matchmaking/status")[1]))
        check(state["servers"][0]["server_id"] == "smoke-1", "Wrong registered server")
        check(state["servers"][0]["game_port"] == game_port, "Wrong advertised TCP port")
        check(MatchmakingClient(api_url, "wrong").request("GET", "/matchmaking/status")[0] == 401,
              "Public API accepted a bad key")
        check(api.request("PUT", "/matchmaking/reservations", "invalid")[0] == 400,
              "Invalid ticket accepted")

        peer, rejected = hello(game_port)
        peer.close()
        check(rejected[4] == 3, "Ticket-only mode bypassed")

        ticket = secrets.token_hex(32)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            responses = list(pool.map(
                lambda _: api.request("PUT", "/matchmaking/reservations", ticket), range(12)))
        check(all(code in (200, 202) for code, _ in responses), "Idempotent reserve failed")
        reserved = wait_for(lambda: (lambda result: result if result[0] == 200 else None)(
            api.request("GET", "/matchmaking/reservations", ticket)))
        check(reserved[1]["status"] == "reserved", "C++ tick did not confirm reservation")
        peer, admitted = hello(game_port, ticket)
        peers.append(peer)
        check(admitted[4] == 1 and admitted[3] == 1, "Reserved TCP admission failed")
        wait_for(lambda: api.request("GET", "/matchmaking/reservations", ticket)[1]["status"] == "consumed")
        replay, rejected = hello(game_port, ticket)
        replay.close()
        check(rejected[4] == 3, "Consumed ticket replay was accepted")

        cancelling = secrets.token_hex(32)
        api.request("PUT", "/matchmaking/reservations", cancelling)
        api.request("DELETE", "/matchmaking/reservations", cancelling)
        wait_for(lambda: api.request("GET", "/matchmaking/reservations", cancelling)[1]["status"] == "cancelled")

        expiring = api.reserve()
        check(expiring["host"] == "127.0.0.1" and expiring["port"] == game_port,
              "Public API returned the wrong game endpoint")
        wait_for(lambda: api.request("GET", "/matchmaking/reservations", expiring["ticket"])[1]["status"] == "expired",
                 seconds=6)

        terminate(game_process)
        wait_for(lambda: api.request("GET", "/matchmaking/status")[1]["ready"] is False, seconds=6)
        print("Separate ASP.NET API registration, reservation, TCP admission, cancellation and expiry passed")
    finally:
        for peer in peers:
            peer.close()
        terminate(game_process)
        terminate(api_process)
        if game_process is not None and game_process.stdout is not None:
            game_process.stdout.close()


if __name__ == "__main__":
    main()
