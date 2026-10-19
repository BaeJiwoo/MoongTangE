"""Matching-service adapter/example; Python standard library only.

Keep MOONGTANGE_API_KEY on the matching backend, never in the game client.
The caller authenticates the user and owns the user-to-ticket association.
"""
import argparse
import http.client
import json
import os
import secrets
import subprocess
import time
from urllib.parse import urlsplit


class MatchmakingClient:
    def __init__(self, api_url, api_key):
        self.endpoint = urlsplit(api_url)
        if (self.endpoint.scheme not in ("http", "https") or not self.endpoint.hostname
                or self.endpoint.path not in ("", "/") or self.endpoint.query
                or self.endpoint.fragment or self.endpoint.username or self.endpoint.password):
            raise ValueError("API URL must be an http(s) origin without credentials or a path")
        self.api_key = api_key

    def request(self, method, path, ticket=None, instance=None):
        connection_type = (http.client.HTTPSConnection if self.endpoint.scheme == "https"
                           else http.client.HTTPConnection)
        connection = connection_type(self.endpoint.hostname, self.endpoint.port, timeout=2)
        headers = {"Authorization": "Bearer " + self.api_key}
        if ticket is not None:
            headers["X-Admission-Ticket"] = ticket
        if instance is not None:
            headers["X-Server-Instance"] = instance
        try:
            connection.request(method, path, body=b"", headers=headers)
            response = connection.getresponse()
            body = response.read(65537)
            if len(body) > 65536:
                raise ValueError("API response too large")
            return response.status, json.loads(body)
        finally:
            connection.close()

    def reserve(self, game_host):
        code, state = self.request("GET", "/v1/status")
        if code != 200 or not state.get("ready"):
            raise RuntimeError("Game server is not ready")
        # 256-bit bearer credential doubles as the idempotency key. Never recycle it.
        ticket = secrets.token_hex(32)
        instance = state["instance_id"]
        deadline = time.monotonic() + 5
        try:
            while time.monotonic() < deadline:
                try:
                    code, result = self.request("PUT", "/v1/reservations", ticket, instance)
                except (OSError, http.client.HTTPException):
                    # A lost response may follow a successful allocation; reuse the same key.
                    time.sleep(0.05)
                    continue
                if code == 200 and result.get("status") == "reserved":
                    if result["expires_in_ms"] <= 0:
                        raise TimeoutError("Reservation expired before assignment delivery")
                    return {"host": game_host, "port": state["game_port"],
                            "protocol_version": state["protocol_version"],
                            "instance_id": instance, "room_id": result["room_id"],
                            "expires_in_ms": result["expires_in_ms"],
                            "ticket": ticket}
                if code not in (202, 503):
                    raise RuntimeError("Reservation rejected: " + result.get("status", result.get("error", str(code))))
                time.sleep(0.05)
            raise TimeoutError("Reservation did not finish within 5 seconds")
        except Exception:
            # Cancellation itself can fail; the bounded reservation TTL is the final cleanup.
            try:
                self.request("DELETE", "/v1/reservations", ticket, instance)
            except (OSError, ValueError, http.client.HTTPException):
                pass
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--api", default="http://127.0.0.1:27016")
    parser.add_argument("--game-host", default="127.0.0.1")
    parser.add_argument("--client", help="Launch this Client executable after reservation")
    options = parser.parse_args()
    assignment = MatchmakingClient(options.api, os.environ["MOONGTANGE_API_KEY"]).reserve(options.game_host)
    if options.client:
        host = assignment["host"]
        endpoint = ("[" + host + "]" if ":" in host else host) + ":" + str(assignment["port"])
        client_environment = os.environ.copy()
        client_environment.pop("MOONGTANGE_API_KEY", None)
        return subprocess.call([options.client, "--connect", endpoint, "--ticket", assignment["ticket"]],
                               env=client_environment)
    # Demo output is an assignment delivered to the requesting user, not a server log.
    print(json.dumps(assignment))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
