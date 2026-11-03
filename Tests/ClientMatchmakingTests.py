"""Exercise the production C++ guest client against HTTP faults and separate services."""
import http.server
import json
import os
from pathlib import Path
import secrets
import ssl
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

from MatchmakingApiSmoke import check, terminate, unused_port, wait_for


def run_probe(probe, url, mode, environment=None, ca_file=None):
    arguments = [probe, url, mode] + ([str(ca_file)] if ca_file else [])
    result = subprocess.run(arguments, env=environment, capture_output=True,
                            text=True, timeout=26)
    check(result.returncode == 0, result.stdout + result.stderr)
    return result.stdout + result.stderr


def fault_tests(probe):
    for fault in ("no_server", "bad_json", "oversized", "bad_port", "bad_version",
                  "bad_ticket", "expired", "response_loss", "rate_limit", "cancel", "tcp_failure", "reuse"):
        attempts, cancellations, sessions = [], [], []

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def reply(self, code, body, headers=None):
                data = json.dumps(body).encode() if not isinstance(body, bytes) else body
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                for key, value in (headers or {}).items():
                    self.send_header(key, value)
                self.end_headers()
                self.wfile.write(data)

            def do_POST(self):
                if self.path == "/client/sessions":
                    sessions.append(True)
                    self.reply(201, {"guest_id": "a" * 32, "access_token": "b" * 64,
                                     "expires_in_ms": 1800000})
                    return
                check(self.headers.get("Authorization") == "Bearer " + "b" * 64, "Missing guest token")
                attempts.append(self.headers.get("Idempotency-Key"))
                if fault == "response_loss" and len(attempts) == 1:
                    self.close_connection = True
                    return
                if fault == "rate_limit" and len(attempts) == 1:
                    self.reply(429, {}, {"Retry-After": "1"})
                    return
                self.do_GET()

            def do_GET(self):
                if fault in ("no_server", "response_loss", "rate_limit", "reuse"):
                    self.reply(503, {"error": "no_game_server"})
                    return
                if fault == "bad_json":
                    self.reply(200, b"{invalid")
                    return
                if fault == "oversized":
                    self.reply(200, b" " * 5000)
                    return
                if fault == "cancel":
                    time.sleep(0.2)
                    try:
                        self.reply(202, {"status": "pending"})
                    except (ConnectionError, OSError):
                        pass
                    return
                assignment = {"status": "reserved", "game_host": "127.0.0.1",
                              "game_port": unused_port(), "protocol_version": 8,
                              "room_id": "1", "expires_in_ms": 29000, "admission_ticket": "c" * 64}
                overrides = {"bad_port": ("game_port", 65536), "bad_version": ("protocol_version", 999),
                             "bad_ticket": ("admission_ticket", "oops"), "expired": ("expires_in_ms", 0)}
                if fault in overrides:
                    field, value = overrides[fault]
                    assignment[field] = value
                self.reply(200, assignment)

            def do_DELETE(self):
                cancellations.append(self.path.rsplit("/", 1)[1])
                self.reply(410, {"status": "cancelled"})

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}"
            mode = "cancel" if fault == "cancel" else "fail_many" if fault == "reuse" else "fail"
            run_probe(probe, url, mode)
            if fault == "reuse":
                check(len(attempts) == 65 and len(set(attempts)) == 65, "Attempt IDs were reused")
                check(len(sessions) == 2, "Guest session reuse or attempt-cap renewal failed")
                check(cancellations == attempts, "Failed attempts leaked reservations")
            elif fault != "cancel":
                check(attempts and len(set(attempts)) == 1, "Retries changed attempt ID")
                check(cancellations == [attempts[0]], "Missing best-effort cancellation")
            if fault in ("response_loss", "rate_limit"):
                check(len(attempts) == 2, "Transient failure was not retried")
        finally:
            server.shutdown()
            server.server_close()
            worker.join()
    for invalid in ("http://game.example:80", "http://127.0.0.1:80/path", "https://user@host"):
        result = subprocess.run([probe, invalid, "fail"], capture_output=True, timeout=5)
        check(result.returncode != 0, "Invalid origin was accepted")
    print("Guest client validation, idempotent retry, cancellation and connection failures passed")


def tls_tests(probe):
    fixtures = Path(__file__).parent / "Fixtures"
    certificate = fixtures / "loopback-test-cert.pem"
    requests = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            requests.append(self.path)
            session = self.path == "/client/sessions"
            body = json.dumps({"guest_id": "a" * 32, "access_token": "b" * 64,
                               "expires_in_ms": 1800000} if session else {}).encode()
            self.send_response(201 if session else 503)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_DELETE(self):
            self.send_response(410)
            self.end_headers()

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, fixtures / "localhost-test-key.pem")
    server.socket = context.wrap_socket(server.socket, server_side=True)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        diagnostic = run_probe(probe, f"https://127.0.0.1:{server.server_port}", "fail", ca_file=certificate)
        check(requests == ["/client/sessions", "/client/matchmaking/reservations"],
              "Trusted HTTPS session did not reach the API: " + diagnostic + repr(requests))
        requests.clear()
        diagnostic = run_probe(probe, f"https://127.0.0.1:{server.server_port}", "fail")
        check(not requests and "verification" in diagnostic, "Untrusted issuer was not rejected: " + diagnostic)
        certificate = fixtures / "localhost-test-cert.pem"
        context.load_cert_chain(certificate, fixtures / "localhost-test-key.pem")
        diagnostic = run_probe(probe, f"https://127.0.0.1:{server.server_port}", "fail", ca_file=certificate)
        check(not requests and "hostname verification" in diagnostic,
              "Hostname mismatch was not rejected: " + diagnostic)
        print("HTTPS trusted certificate, untrusted issuer and hostname mismatch passed")
    finally:
        server.shutdown()
        server.server_close()
        worker.join()


def service_test(probe, server_executable, project, gui=None):
    url = f"http://127.0.0.1:{unused_port()}"
    control_key = secrets.token_hex(32)
    public_key = secrets.token_hex(32)
    environment = dict(os.environ, ASPNETCORE_URLS=url, MOONGTANGE_CONTROL_KEY=control_key,
                       MOONGTANGE_PUBLIC_API_KEY=public_key, Logging__LogLevel__Default="Warning")
    client_environment = dict(os.environ)
    client_environment.pop("MOONGTANGE_CONTROL_KEY", None)
    client_environment.pop("MOONGTANGE_PUBLIC_API_KEY", None)
    api_process = game_process = None
    with tempfile.TemporaryFile() as log:
        try:
            subprocess.run(["dotnet", "build", "-c", "Release", project], check=True, timeout=60,
                           stdout=log, stderr=subprocess.STDOUT)
            application = Path(project).resolve().parent / "bin/Release/net8.0/MoongTangE.MatchmakingApi.dll"
            api_process = subprocess.Popen(["dotnet", str(application)],
                                           env=environment, stdout=log, stderr=subprocess.STDOUT)
            wait_for(lambda: urllib.request.urlopen(url + "/health", timeout=1).status == 200, seconds=25)
            game_process = subprocess.Popen(
                [server_executable, "--port", "0", "--require-ticket", "--max-rooms", "2",
                 "--matchmaking-url", url, "--server-id", "guest-test", "--advertise-host", "127.0.0.1"],
                env=environment, stdout=log, stderr=subprocess.STDOUT)

            def ready():
                request = urllib.request.Request(url + "/matchmaking/status",
                                                 headers={"Authorization": "Bearer " + public_key})
                with urllib.request.urlopen(request, timeout=1) as response:
                    return json.load(response)["ready"]

            wait_for(ready)
            if gui:
                subprocess.run([gui, "--api-url", url, "--smoke"], env=client_environment,
                               check=True, timeout=35)
            else:
                run_probe(probe, url, "connect", client_environment)
            print("C++ guest session -> API reservation -> native TCP -> input/snapshot passed")
        except Exception:
            log.seek(0)
            print(log.read().decode(errors="replace"), file=sys.stderr)
            raise
        finally:
            terminate(game_process)
            terminate(api_process)


if __name__ == "__main__":
    fault_tests(sys.argv[1])
    tls_tests(sys.argv[1])
    service_test(*sys.argv[1:4])
