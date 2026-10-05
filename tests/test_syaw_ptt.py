import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]


class SyawPttTest(unittest.TestCase):
    def build_and_run(self, test_source, sources=(), cases=("default",)):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "syaw_ptt_test"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-include",
                    "memory",
                    "-DCONFIG_BOARD_TYPE_SYAW_7210=1",
                    "-Itests/syaw_stubs",
                    "-Imain",
                    "-Imain/protocols",
                    test_source,
                    *sources,
                    "-o",
                    str(binary),
                ],
                cwd=REPO_ROOT,
                check=True,
            )
            for case in cases:
                with self.subTest(case=case):
                    subprocess.run([str(binary), case], check=True)

    def test_ptt_messages_and_release_races(self):
        self.build_and_run(
            "tests/syaw_ptt_flow_test.cc",
            (
                "main/boards/syaw-7210/syaw_ptt.cc",
                "main/protocols/protocol.cc",
            ),
            (
                "idle",
                "auto_listening",
                "realtime_listening",
                "auto_speaking",
                "realtime_speaking",
                "listening_without_processor",
                "quick_release",
                "release_while_connecting",
                "release_and_repress_while_connecting",
                "duplicate_edges",
                "connection_failure",
                "unsupported_states",
                "missing_protocol",
                "disconnect_before_release",
            ),
        )

    def test_boot_bindings(self):
        self.build_and_run("tests/syaw_button_test.cc")

    def test_release_lists_distinct_syaw_board(self):
        result = subprocess.run(
            [sys.executable, "scripts/release.py", "--list-boards", "--json"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        variants = json.loads(result.stdout)
        self.assertIn({"board": "syaw-7210", "name": "syaw-7210"}, variants)
        self.assertIn({"board": "lichuang-dev", "name": "lichuang-dev"}, variants)


if __name__ == "__main__":
    unittest.main()
