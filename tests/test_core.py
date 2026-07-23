import os
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COMPILER = "g++"
BUILD_DIR = ROOT / ".test-build"
TEST_BIN = BUILD_DIR / "test_core"
BOARD_PROFILE_TEST_BIN = BUILD_DIR / "test_board_profile"
WIDGET_CONFIG_TEST_BIN = BUILD_DIR / "test_widget_config_core"
WIDGET_CORE_TEST_BIN = BUILD_DIR / "test_widget_core"
GTFS_RT_FILTER_TEST_BIN = BUILD_DIR / "test_gtfs_realtime_trip_filter"
DISPLAY_TEXT_CORE_TEST_BIN = BUILD_DIR / "test_display_text_core"
WIDGET_SCHEDULER_TEST_BIN = BUILD_DIR / "test_widget_scheduler"
WIDGET_PROVIDER_ROUTER_TEST_BIN = BUILD_DIR / "test_widget_provider_router"
NATIVE_CONFIG = ROOT / "platformio.native.ini"


class CoreBehaviorTests(unittest.TestCase):
    def test_selected_board_profile_cpp_contract(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            COMPILER,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DTRANSITINK_BOARD_ZECTRIX_NOTE4=1",
            "-Iinclude",
            "test_host/test_board_profile.cpp",
            "-o",
            str(BOARD_PROFILE_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(BOARD_PROFILE_TEST_BIN)], cwd=ROOT, check=True)

    def test_app_config_native_behaviors(self):
        env = os.environ.copy()
        env["PLATFORMIO_CORE_DIR"] = str(ROOT / ".platformio")
        cmd = [
            str(ROOT / ".venv/bin/platformio"),
            "test",
            "-c",
            str(NATIVE_CONFIG),
            "-e",
            "native_app_config",
            "-f",
            "test_app_config",
        ]
        subprocess.run(cmd, cwd=ROOT, env=env, check=True)

    def test_core_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            COMPILER,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/BusEtaCore.cpp",
            "src/core/BatteryStatus.cpp",
            "test_host/test_core.cpp",
            "-o",
            str(TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(TEST_BIN)], cwd=ROOT, check=True)

    def test_widget_config_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            COMPILER,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/WidgetConfigCore.cpp",
            "test_host/test_widget_config_core.cpp",
            "-o",
            str(WIDGET_CONFIG_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(WIDGET_CONFIG_TEST_BIN)], cwd=ROOT, check=True)

    def test_widget_core_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            COMPILER,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/WidgetCore.cpp",
            "src/core/WidgetConfigCore.cpp",
            "test_host/test_widget_core.cpp",
            "-o",
            str(WIDGET_CORE_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(WIDGET_CORE_TEST_BIN)], cwd=ROOT, check=True)

    def test_gtfs_realtime_trip_filter_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            COMPILER,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/GtfsRealtimeTripFilter.cpp",
            "test_host/test_gtfs_realtime_trip_filter.cpp",
            "-o",
            str(GTFS_RT_FILTER_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(GTFS_RT_FILTER_TEST_BIN)], cwd=ROOT, check=True)

    def test_display_text_core_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            COMPILER,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/DisplayTextCore.cpp",
            "test_host/test_display_text_core.cpp",
            "-o",
            str(DISPLAY_TEXT_CORE_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(DISPLAY_TEXT_CORE_TEST_BIN)], cwd=ROOT, check=True)

    def test_widget_scheduler_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            COMPILER,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/WidgetCore.cpp",
            "src/core/WidgetScheduler.cpp",
            "src/core/WidgetConfigCore.cpp",
            "test_host/test_widget_scheduler.cpp",
            "-o",
            str(WIDGET_SCHEDULER_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(WIDGET_SCHEDULER_TEST_BIN)], cwd=ROOT, check=True)

    def test_widget_provider_router_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            COMPILER,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Itest_host/provider_router_shims",
            "-Itest_native/shims",
            "-Iinclude",
            "src/core/WidgetCore.cpp",
            "src/core/WidgetScheduler.cpp",
            "src/core/WidgetConfigCore.cpp",
            "src/providers/WidgetProviderRouter.cpp",
            "test_host/test_widget_provider_router.cpp",
            "-o",
            str(WIDGET_PROVIDER_ROUTER_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(WIDGET_PROVIDER_ROUTER_TEST_BIN)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
