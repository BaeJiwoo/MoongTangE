"""Exercise ownership and failure cleanup using real child processes, without graphics."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("playtest", Path(__file__).resolve().parents[1] / "Tools/playtest.py")
playtest = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(playtest)
SLEEPER = [sys.executable, "-c", "import time; time.sleep(60)"]


class PlaytestToolTests(unittest.TestCase):
    def test_cleanup_preserves_unrelated_process_and_reaps_owned_children(self):
        outsider = subprocess.Popen(SLEEPER)
        try:
            with tempfile.TemporaryDirectory() as directory:
                processes = playtest.Processes(Path(directory))
                try:
                    children = [processes.start(str(index), SLEEPER) for index in range(3)]
                    processes.check_running()
                finally:
                    processes.close()
                self.assertTrue(all(child.poll() is not None for child in children))
                self.assertIsNone(outsider.poll())
                processes.close()  # Closing a completed session is harmless.
        finally:
            outsider.terminate()
            outsider.wait(timeout=5)

    def test_failed_spawn_still_cleans_up_previous_child(self):
        with tempfile.TemporaryDirectory() as directory:
            processes = playtest.Processes(Path(directory))
            try:
                child = processes.start("first", SLEEPER)
                with self.assertRaises(OSError):
                    processes.start("missing", [str(Path(directory) / "not-an-executable")])
            finally:
                processes.close()
            self.assertIsNotNone(child.poll())

    def test_early_exit_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            processes = playtest.Processes(Path(directory))
            try:
                child = processes.start("failed", [sys.executable, "-c", "raise SystemExit(7)"])
                child.wait(timeout=5)
                with self.assertRaisesRegex(RuntimeError, r"failed exited \(7\)"):
                    playtest.wait_for(lambda: False, processes, "readiness", seconds=1)
            finally:
                processes.close()

    def test_startup_timeout_and_interrupt_leave_no_api_process(self):
        for failure in (RuntimeError("readiness failed"), KeyboardInterrupt()):
            with self.subTest(failure=type(failure).__name__), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                api = root / "MatchmakingApi/bin/Release/net8.0/MoongTangE.MatchmakingApi.dll"
                api.parent.mkdir(parents=True)
                api.touch()
                owned = []
                start = playtest.Processes.start

                def start_sleeper(processes, name, command, environment=None, cwd=None):
                    child = start(processes, name, SLEEPER, environment, root)
                    owned.append(child)
                    return child

                options = SimpleNamespace(mode="play", clients=3, seed=42, smoke=False)
                with patch.object(playtest, "ROOT", root), \
                     patch.object(playtest, "executable", return_value=Path(sys.executable)), \
                     patch.object(playtest.Processes, "start", start_sleeper), \
                     patch.object(playtest, "wait_for", side_effect=failure):
                    with self.assertRaises(type(failure)):
                        playtest.play(options, root, sys.executable)
                self.assertEqual(len(owned), 1)
                self.assertIsNotNone(owned[0].poll())

    def test_readiness_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            processes = playtest.Processes(Path(directory))
            with self.assertRaisesRegex(RuntimeError, "Timed out waiting for admission"):
                playtest.wait_for(lambda: False, processes, "admission", seconds=0.01)

    def test_client_environment_excludes_server_secrets(self):
        with patch.dict(playtest.os.environ, {name: "private" for name in playtest.SECRET_KEYS}):
            environment = playtest.clean_environment()
            self.assertTrue(all(name not in environment for name in playtest.SECRET_KEYS))
            self.assertTrue(all(playtest.os.environ[name] == "private" for name in playtest.SECRET_KEYS))

    def test_admission_requires_same_room_distinct_players_and_expected_names(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertFalse(playtest.joined_players(root, 3))
            for index in range(1, 4):
                (root / f"client-{index}.log").write_text(
                    f"Playtest joined room=1 player={index} nickname=Playtest{index}\n", encoding="utf-8")
            self.assertTrue(playtest.joined_players(root, 3))
            for invalid in ("room=2 player=3", "room=1 player=2", "room=0 player=3"):
                (root / "client-3.log").write_text(
                    f"Playtest joined {invalid} nickname=Playtest3\n", encoding="utf-8")
                with self.assertRaises(RuntimeError):
                    playtest.joined_players(root, 3)
            (root / "client-3.log").write_text(
                "Playtest joined room=1 player=3 nickname=WrongName\n", encoding="utf-8")
            self.assertFalse(playtest.joined_players(root, 3))


if __name__ == "__main__":
    unittest.main()
