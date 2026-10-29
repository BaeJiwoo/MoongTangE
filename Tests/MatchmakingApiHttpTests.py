"""Exercise MVC routes and the existing HTTP contract over a real loopback server."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

from MatchmakingApiSmoke import check, terminate, unused_port, wait_for


def main():
    project = Path(sys.argv[1]).resolve()
    subprocess.run(["dotnet", "build", str(project), "-c", "Release", "--nologo"],
                   check=True, timeout=60)
    application = project.parent / "bin/Release/net8.0/MoongTangE.MatchmakingApi.dll"
    api_url = f"http://127.0.0.1:{unused_port()}"
    public_key, control_key = secrets.token_hex(32), secrets.token_hex(32)
    environment = dict(os.environ, ASPNETCORE_URLS=api_url,
                       MOONGTANGE_PUBLIC_API_KEY=public_key,
                       MOONGTANGE_CONTROL_KEY=control_key,
                       Logging__LogLevel__Default="Warning")

    def request(method, path, key=None, ticket=None, body=None, headers=None):
        fields = dict(headers or {})
        if key is not None:
            fields["Authorization"] = "Bearer " + key
        if ticket is not None:
            fields["X-Admission-Ticket"] = ticket
        data = body.encode("utf-8") if body is not None else None
        message = urllib.request.Request(api_url + path, data=data, headers=fields, method=method)
        try:
            response = urllib.request.urlopen(message, timeout=5)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            return response.status, response.headers, response.read()

    def expect(code, method, path, key=None, ticket=None, body=None, headers=None,
               payload=None, empty=False, content_type=None):
        status, response_headers, data = request(method, path, key, ticket, body, headers)
        check(status == code, f"{method} {path}: expected {code}, got {status}: {data!r}")
        # Kestrel clears application headers when it rejects an oversized body.
        if code != 413:
            check(response_headers.get("Cache-Control") == "no-store", "Missing no-store policy")
        if payload is not None:
            check(json.loads(data) == payload, f"Unexpected JSON: {data!r}")
            check(response_headers.get_content_type() == "application/json", "Expected JSON")
        if empty:
            check(data == b"", f"Expected empty response: {data!r}")
        if content_type:
            check(response_headers.get_content_type() == content_type, "Wrong content type")
        return data

    with tempfile.TemporaryFile(mode="w+b") as log:
        process = subprocess.Popen(["dotnet", str(application)], env=environment,
                                   stdout=log, stderr=subprocess.STDOUT)
        try:
            wait_for(lambda: request("GET", "/health")[0] == 200, seconds=20)
            expect(200, "GET", "/health", payload={"status": "ok"})
            expect(405, "POST", "/health", empty=True)
            expect(404, "GET", "/missing", empty=True)

            ticket = "a" * 64
            protected = [("GET", "/matchmaking/status", public_key),
                         *[(verb, "/matchmaking/reservations", public_key)
                           for verb in ("GET", "PUT", "DELETE")],
                         ("PUT", "/game-servers/test/heartbeat", control_key),
                         ("PUT", "/game-servers/test/commands/1", control_key),
                         ("PUT", f"/game-servers/test/reservations/{ticket}", control_key)]
            for method, path, correct_key in protected:
                other_key = control_key if correct_key == public_key else public_key
                for wrong_key in (None, "wrong", other_key):
                    expect(401, method, path, wrong_key, payload={"error": "unauthorized"})

            expect(200, "GET", "/matchmaking/status", public_key,
                   payload={"ready": False, "servers": []})
            expect(405, "POST", "/matchmaking/reservations", public_key, empty=True)
            for method in ("GET", "PUT", "DELETE"):
                for invalid in (None, "", "a" * 63, "A" * 64, "g" * 64):
                    expect(400, method, "/matchmaking/reservations", public_key, invalid,
                           payload={"error": "invalid_ticket"})
            expect(503, "PUT", "/matchmaking/reservations", public_key, ticket,
                   payload={"error": "no_game_server"})
            for method in ("GET", "DELETE"):
                expect(404, method, "/matchmaking/reservations", public_key, ticket,
                       payload={"error": "unknown_ticket"})

            heartbeat = {"X-Game-Host": "127.0.0.1", "X-Game-Port": "27015",
                         "X-Protocol-Version": "8", "X-Capacity": "3", "X-Players": "0",
                         "X-Reserved": "0", "X-Available": "3", "X-Reservation-Seconds": "30"}
            heartbeat_path = "/game-servers/test/heartbeat"
            expect(400, "PUT", heartbeat_path, control_key, payload="invalid heartbeat")
            expect(400, "PUT", "/game-servers/invalid.id/heartbeat", control_key,
                   headers=heartbeat, payload="invalid heartbeat")
            for header, value in (("X-Game-Port", "0"), ("X-Players", "-1"),
                                  ("X-Available", "2"), ("X-Reservation-Seconds", "301")):
                expect(400, "PUT", heartbeat_path, control_key,
                       headers=dict(heartbeat, **{header: value}), payload="invalid heartbeat")
            expect(200, "PUT", heartbeat_path, control_key, headers=heartbeat,
                   empty=True, content_type="text/plain")
            expect(404, "PUT", "/game-servers/test/commands/not-a-number", control_key,
                   empty=True)
            command_path = "/game-servers/test/commands/1"
            for report in ("", "reserved", "invalid\t1\t0", "reserved\t-1\t0",
                           "reserved\t1\t-1", "reserved\t1\t0\textra"):
                expect(400, "PUT", command_path, control_key, body=report, empty=True)
            expect(400, "PUT", command_path, control_key, body="reserved\t1\t0", empty=True)
            expect(413, "PUT", command_path, control_key, body="x" * 1025)

            pending = json.loads(expect(202, "PUT", "/matchmaking/reservations",
                                        public_key, ticket))
            check(pending["status"] == "pending", "Reservation was not pending")
            expect(202, "GET", "/matchmaking/reservations", public_key, ticket)
            expect(202, "PUT", "/matchmaking/reservations", public_key, ticket)
            commands = expect(200, "PUT", heartbeat_path, control_key, headers=heartbeat,
                              content_type="text/plain").decode().splitlines()
            check(len(commands) == 1, "Duplicate reservation command")
            command_id, action, command_ticket = commands[0].split("\t")
            check((action, command_ticket) == ("reserve", ticket), "Wrong command format")
            expect(204, "PUT", f"/game-servers/test/commands/{command_id}", control_key,
                   body="reserved\t1\t29000", empty=True)
            reserved = json.loads(expect(200, "GET", "/matchmaking/reservations", public_key, ticket))
            check(reserved["status"] == "reserved" and reserved["room_id"] == "1" and
                  reserved["game_port"] == 27015, "Wrong reservation response")
            expect(202, "DELETE", "/matchmaking/reservations", public_key, ticket)
            commands = expect(200, "PUT", heartbeat_path, control_key, headers=heartbeat).decode()
            cancel_id, action, _ = commands.split("\t")
            check(action == "cancel", "Missing cancel command")
            expect(204, "PUT", f"/game-servers/test/commands/{cancel_id}", control_key,
                   body="cancelled\t1\t0", empty=True)
            expect(410, "GET", "/matchmaking/reservations", public_key, ticket)

            report_path = f"/game-servers/test/reservations/{ticket}"
            expect(400, "PUT", "/game-servers/test/reservations/invalid", control_key,
                   body="reserved\t1\t0", empty=True)
            expect(400, "PUT", report_path.replace("/test/", "/other/"), control_key,
                   body="reserved\t1\t0", empty=True)
            for state, code in (("reserved", 200), ("consumed", 200), ("full", 409),
                                ("expired", 410), ("cancelled", 410), ("unavailable", 503)):
                expect(204, "PUT", report_path, control_key, body=f"{state}\t1\t0", empty=True)
                result = json.loads(expect(code, "GET", "/matchmaking/reservations", public_key, ticket))
                check(result["status"] == state, "Wrong public reservation state")
            print("MVC routes, authorization, validation, response contracts and reservation workflow passed")
        except Exception:
            log.seek(0)
            print(log.read().decode("utf-8", errors="replace"), file=sys.stderr)
            raise
        finally:
            terminate(process)


if __name__ == "__main__":
    main()
