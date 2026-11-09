"""Build and run a local playtest, or run the small gameplay regression suite."""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
QUICK_TESTS = ("PhysicsTests", "WorldTests", "SurvivalTests", "RenderingTests", "CliTests", "LocalTests")
SECRET_KEYS = ("MOONGTANGE_PUBLIC_API_KEY", "MOONGTANGE_CONTROL_KEY")


class Processes:
    """Own only directly launched children; never discover/kill processes by name or stale PID."""

    def __init__(self, directory):
        self.directory = directory
        self.children = []

    def start(self, name, command, environment=None, cwd=ROOT):
        options = ({"creationflags": subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP}
                   if os.name == "nt" else {"start_new_session": True})
        with (self.directory / f"{name}.log").open("wb") as log:
            process = subprocess.Popen(list(map(str, command)), cwd=cwd, env=environment,
                                       stdin=subprocess.DEVNULL, stdout=log,
                                       stderr=subprocess.STDOUT, **options)
        self.children.append((name, process))
        return process

    def check_running(self, children=None):
        for name, process in self.children if children is None else children:
            code = process.poll()
            if code is not None:
                raise RuntimeError(f"{name} exited ({code}); see {self.directory / (name + '.log')}")

    def close(self):
        # Ignore a second Ctrl+C until every owned child has been reaped.
        previous = signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            for _, process in reversed(self.children):
                if process.poll() is None:
                    try:
                        process.terminate()
                    except ProcessLookupError:
                        pass
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
        finally:
            signal.signal(signal.SIGINT, previous)


def run(command, **kwargs):
    print("+ " + subprocess.list2cmdline(list(map(str, command))), flush=True)
    subprocess.run(list(map(str, command)), cwd=ROOT, check=True, **kwargs)


def clean_environment():
    environment = dict(os.environ)
    for name in SECRET_KEYS:
        environment.pop(name, None)
    environment.pop("MOONGTANGE_SMOKE_CAPTURE", None)
    return environment


def wait_for(predicate, processes, description, seconds=30):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        processes.check_running()
        try:
            if predicate():
                return
        except (urllib.error.URLError, TimeoutError, ConnectionError):
            pass
        time.sleep(0.1)
    raise RuntimeError(f"Timed out waiting for {description}; see {processes.directory}")


def unused_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def executable(build, name):
    suffix = ".exe" if os.name == "nt" else ""
    for directory in (build / "Release", build):
        path = directory / (name + suffix)
        if path.is_file():
            return path
    raise RuntimeError(f"Missing {name} in {build}; run again without --skip-build")


def dotnet_executable(build):
    cache = build / "CMakeCache.txt"
    if cache.is_file():
        for line in cache.read_text(encoding="utf-8").splitlines():
            if line.startswith("MOONGTANGE_DOTNET_EXECUTABLE:"):
                path = line.partition("=")[2]
                if Path(path).is_file():
                    return path
    found = shutil.which("dotnet")
    if not found:
        raise RuntimeError("Install .NET 8 SDK and add dotnet to PATH")
    return found


def build_project(mode, skip):
    preset = "windows-release" if os.name == "nt" else "linux-release"
    build = ROOT / "build" / ("playtest-core" if mode == "check" else preset)
    dotnet = dotnet_executable(build) if mode == "play" else None
    if not skip:
        if mode == "check":
            run(["cmake", "-S", ROOT, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
                 "-DBUILD_TESTING=ON", "-DMOONGTANGE_BUILD_CLIENT=OFF",
                 "-DMOONGTANGE_BUILD_NETWORK=OFF", f"-DPython3_EXECUTABLE={sys.executable}"])
            targets = list(QUICK_TESTS)
        else:
            # The full preset requires the .NET SDK when configuring its network tests.
            dotnet = dotnet or dotnet_executable(build)
            run(["cmake", "--preset", preset, f"-DPython3_EXECUTABLE={sys.executable}",
                 f"-DMOONGTANGE_DOTNET_EXECUTABLE={dotnet}"])
            targets = ["Client"] + (["DedicatedServer"] if mode == "play" else [])
        run(["cmake", "--build", build, "--config", "Release", "--parallel", "4",
             "--target", *targets])
        if mode == "play":
            run([dotnet, "build", "-c", "Release", ROOT / "MatchmakingApi/MoongTangE.MatchmakingApi.csproj"])
    return build, dotnet


def joined_players(directory, count):
    assignments = []
    for index in range(1, count + 1):
        log = directory / f"client-{index}.log"
        if not log.is_file():
            return False
        match = re.search(r"Playtest joined room=(\d+) player=(\d+) nickname=Playtest" + str(index) + r"\b",
                          log.read_text(encoding="utf-8", errors="replace"))
        if not match:
            return False
        assignments.append(tuple(map(int, match.groups())))
    if (any(room == 0 or player == 0 for room, player in assignments)
            or len({room for room, _ in assignments}) != 1
            or len({player for _, player in assignments}) != count):
        raise RuntimeError("Clients did not receive distinct players in the same room")
    return True


def play(options, build, dotnet):
    client = executable(build, "Client")
    parent = ROOT / "build/playtest-logs"
    parent.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix=time.strftime("%Y%m%d-%H%M%S-"), dir=parent))
    print(f"Seed: {options.seed}\nLogs: {directory}", flush=True)
    processes = Processes(directory)
    client_environment = clean_environment()
    count = 1 if options.mode == "single" else options.clients
    try:
        if options.mode == "play":
            server = executable(build, "DedicatedServer")
            api = ROOT / "MatchmakingApi/bin/Release/net8.0/MoongTangE.MatchmakingApi.dll"
            if not api.is_file():
                raise RuntimeError("Missing API build; run again without --skip-build")
            url = f"http://127.0.0.1:{unused_port()}"
            public_key, control_key = secrets.token_hex(32), secrets.token_hex(32)
            environment = dict(client_environment, ASPNETCORE_URLS=url,
                               MOONGTANGE_PUBLIC_API_KEY=public_key,
                               MOONGTANGE_CONTROL_KEY=control_key,
                               Logging__LogLevel__Default="Warning")
            # Ignore workstation proxy settings for these strictly loopback probes.
            http = urllib.request.build_opener(urllib.request.ProxyHandler({}))

            def status(path):
                request = urllib.request.Request(url + path,
                                                 headers={"Authorization": "Bearer " + public_key})
                with http.open(request, timeout=1) as response:
                    return json.load(response)

            processes.start("api", [dotnet, api], environment)
            wait_for(lambda: status("/health"), processes, "API health")
            processes.start("server", [server, "--port", "0", "--require-ticket", "--max-rooms", "1",
                                      "--world-seed", options.seed, "--matchmaking-url", url,
                                      "--server-id", "local-playtest", "--advertise-host", "127.0.0.1"],
                            environment)
            wait_for(lambda: status("/matchmaking/status")["ready"], processes, "server registration")
            connection = ["--api-url", url]
        else:
            connection = ["--single", "--world-seed", options.seed]

        services = list(processes.children)
        clients = []
        for index in range(1, count + 1):
            arguments = [client, *connection, "--playtest", f"Playtest{index}"]
            if options.smoke:
                arguments.append("--smoke")
            clients.append(processes.start(f"client-{index}", arguments, client_environment, client.parent))
        if options.smoke:
            # Smoke clients may finish before a slow peer joins. Inspect their exit codes/logs below.
            deadline = time.monotonic() + 40
        else:
            wait_for(lambda: joined_players(directory, count), processes, "client admission")
            print(f"{count} player(s) joined. WASD: move / F3: colliders. Close all game windows or Ctrl+C to stop.",
                  flush=True)
            deadline = None
        while any(process.poll() is None for process in clients):
            processes.check_running(services)
            if any(process.poll() not in (None, 0) for process in clients):
                raise RuntimeError(f"Client failed; see {directory}")
            if deadline is not None and time.monotonic() >= deadline:
                raise RuntimeError(f"Smoke timed out; see {directory}")
            time.sleep(0.1)
        if any(process.returncode != 0 for process in clients) or not joined_players(directory, count):
            raise RuntimeError(f"Clients did not complete successfully; see {directory}")
        print("Playtest completed.")
    finally:
        processes.close()
        print("Stopped all processes launched by this playtest.", flush=True)


def uint64(value):
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("seed must be a uint64 decimal integer") from error
    if not 0 <= parsed <= 2**64 - 1:
        raise argparse.ArgumentTypeError("seed must be between 0 and 18446744073709551615")
    return parsed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("play", "single", "check"), nargs="?", default="play")
    parser.add_argument("--clients", type=int, choices=(1, 2, 3), default=3)
    parser.add_argument("--seed", type=uint64, default=42, help="world layout seed (default: 42)")
    parser.add_argument("--skip-build", action="store_true", help="use existing binaries intentionally")
    parser.add_argument("--smoke", action="store_true", help="hidden automated movement check; requires OpenGL")
    options = parser.parse_args()
    if os.name != "nt" and not sys.platform.startswith("linux"):
        parser.error("Only Windows and Linux are supported")
    if options.mode == "check" and options.smoke:
        parser.error("--smoke requires play or single")
    try:
        revision = subprocess.run(["git", "describe", "--always", "--dirty"], cwd=ROOT,
                                  text=True, capture_output=True, check=False).stdout.strip()
        print(f"MoongTangE playtest | source {revision or 'unknown'}", flush=True)
        if options.skip_build:
            print("Build skipped: binaries may differ from this source revision.", flush=True)
        build, dotnet = build_project(options.mode, options.skip_build)
        if options.mode == "check":
            run(["ctest", "--test-dir", build, "-C", "Release", "--output-on-failure",
                 "--no-tests=error", "-R", "^(" + "|".join((*QUICK_TESTS, "PlaytestToolTests")) + ")$"])
        else:
            play(options, build, dotnet)
        return 0
    except KeyboardInterrupt:
        print("Playtest interrupted.")
        return 130
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Playtest failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
