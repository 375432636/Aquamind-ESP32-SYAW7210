import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]


class WifiProvisioningTest(unittest.TestCase):
    def test_station_to_provisioning_lifecycle(self):
        source = (REPO_ROOT / "main/boards/common/wifi_board.cc").read_text()
        # Compile the actual caller against a minimal model of ESP-IDF's
        # default-handler ownership and esp-wifi-connect 2.6.2 Stop().
        start = source.index("void WifiBoard::StartNetwork() {")
        end = source.index("\nNetworkInterface* WifiBoard::GetNetwork()", start)
        caller = source[start:end]
        harness = '#include "wifi_provisioning_stubs.h"\n' + caller + """
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    WifiBoard board;
    station_connects = scenario == "connected";
    has_saved_network = scenario != "no_saved_network";
    board.wifi_config_mode_ = scenario == "forced_provisioning";
    board.StartNetwork();
    assert(provisioning_starts == (station_connects ? 0 : 1));
    assert(stops == (scenario == "timeout" ? 1 : 0));
    if (scenario == "connected") {
        assert(station_netif.alive && station_netif.default_handler_attached);
    }
}
"""
        with tempfile.TemporaryDirectory() as directory:
            test_source = Path(directory) / "wifi_provisioning.cc"
            binary = Path(directory) / "wifi_provisioning"
            test_source.write_text(harness)
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Wno-unused-lambda-capture",
                    "-Itests",
                    str(test_source),
                    "-o",
                    str(binary),
                ],
                cwd=REPO_ROOT,
                check=True,
            )
            for case in (
                "timeout",
                "connected",
                "no_saved_network",
                "forced_provisioning",
            ):
                with self.subTest(case=case):
                    subprocess.run([str(binary), case], check=True)


if __name__ == "__main__":
    unittest.main()
