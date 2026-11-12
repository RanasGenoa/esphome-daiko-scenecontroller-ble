import ast
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

IGNORED_DIRS = {
    ".git",
    ".local",
    ".esphome",
    ".venv",
    "venv",
    "__pycache__",
    ".cache",
    ".pytest_cache",
    ".ruff_cache",
    "tmp",
}
IGNORED_FILES = {
    "secrets.yaml",
    ".DS_Store",
}


def walk_workspace_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for entry in sorted(root.iterdir()):
        if entry.name in IGNORED_DIRS:
            continue
        if entry.is_dir():
            files.extend(walk_workspace_files(entry))
        elif entry.name not in IGNORED_FILES:
            files.append(entry)
    return files


class TestContributorGuardrails(unittest.TestCase):
    """Automated quality, architectural, security, and legal compliance guardrails (AGENTS.md)."""

    def test_python_component_syntax(self) -> None:
        py_files = list((REPO_ROOT / "components").rglob("*.py")) + list(
            (REPO_ROOT / "tests").rglob("*.py")
        )
        self.assertGreaterEqual(len(py_files), 6)
        for py_file in py_files:
            source = py_file.read_text(encoding="utf-8")
            ast.parse(source, filename=str(py_file))

    def test_auto_discovery_and_unified_light_entity_invariants(self) -> None:
        init_py = (
            REPO_ROOT / "components/daiko_scenecontroller_ble/__init__.py"
        ).read_text(encoding="utf-8")
        light_h = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble_light.h"
        ).read_text(encoding="utf-8")
        light_cpp = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble_light.cpp"
        ).read_text(encoding="utf-8")
        header_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.h"
        ).read_text(encoding="utf-8")
        cpp_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.cpp"
        ).read_text(encoding="utf-8")
        yaml_src = (REPO_ROOT / "daiko_controller.yaml").read_text(encoding="utf-8")

        # Auto-load and auto-create entities must be enabled by default
        self.assertIn(
            'AUTO_LOAD = ["light", "button", "sensor", "binary_sensor", "text_sensor"]',
            init_py,
        )
        self.assertIn("CONF_AUTO_CREATE_ENTITIES", init_py)
        self.assertIn("_default_auto_lights", init_py)

        # Hardware circuit type auto-discovery & NVS persistence (L1..L6 all 6 load types)
        self.assertIn("DAIKO_CIRCUIT_TYPES_MAGIC", header_src)
        self.assertIn("PersistedCircuitTypes", header_src)
        self.assertIn("daiko_ctype_v1", cpp_src)
        for load_enum in (
            "L1_DIMMABLE_LED = 0x01",
            "L2_DIMMABLE_INCANDESCENT = 0x02",
            "L3_ON_OFF = 0x03",
            "L4_DIMMABLE_PWM = 0x04",
            "L5_TUNABLE_PWM = 0x05",
            "L6_TUNABLE_PHASE = 0x06",
        ):
            self.assertIn(load_enum, header_src)
            self.assertIn(load_enum.split(" = ")[0], light_cpp)

        # Unified light entity types (AUTO for circuits 1..6, MASTER for master power, ON_OFF for non-dimmable)
        self.assertIn("AUTO = 0", light_h)
        self.assertIn("MASTER = 4", light_h)
        self.assertFalse(
            (REPO_ROOT / "components/daiko_scenecontroller_ble/switch").exists(),
            "Lighting circuits and master power must be exposed as light entities, not switch",
        )
        self.assertNotIn("\nswitch:", yaml_src)

    def test_ble_mesh_iv_index_and_recovery_invariants(self) -> None:
        yaml_src = (REPO_ROOT / "daiko_controller.yaml").read_text(encoding="utf-8")
        header_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.h"
        ).read_text(encoding="utf-8")
        cpp_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.cpp"
        ).read_text(encoding="utf-8")

        # Mandatory Kconfig for Secure Network Beacon IV recovery
        self.assertIn("CONFIG_BLE_MESH_IVU_RECOVERY_IVI: y", yaml_src)

        # PersistedIvState & underflow guard invariants
        self.assertIn("DAIKO_IV_STATE_MAGIC", header_src)
        self.assertIn("MAX_VALID_IV_INDEX", header_src)
        self.assertIn("daiko_iv_state_v1", cpp_src)
        self.assertIn("if (cur_iv == 0 && cur_ivu)", cpp_src)

        # Forbidden anti-pattern: never hardcode bt_mesh.iv_index = 0
        code_only = re.sub(r"//.*$", "", cpp_src, flags=re.MULTILINE)
        self.assertNotRegex(code_only, r"bt_mesh\.iv_index\s*=\s*0\s*;")

    def test_loopback_filtering_and_bounded_rotation_invariants(self) -> None:
        header_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.h"
        ).read_text(encoding="utf-8")
        cpp_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn("MAX_AUTO_ROTATION_STREAK = 2", header_src)
        self.assertIn("if (src == this->self_addr_)", cpp_src)
        self.assertIn("if (src != this->controller_addr_)", cpp_src)
        self.assertIn("daiko_addr_v2_m", cpp_src)
        self.assertIn("daiko_wrap_epoch_v1", cpp_src)

    def test_light_throttling_and_hardware_dual_state_invariants(self) -> None:
        light_h = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble_light.h"
        ).read_text(encoding="utf-8")
        header_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.h"
        ).read_text(encoding="utf-8")

        self.assertIn("THROTTLE_INTERVAL_MS = 330", light_h)
        self.assertIn("USER_COOLDOWN_MS = 3500", light_h)
        self.assertIn("CIRCUIT_ONOFF = 0x35", header_src)
        self.assertIn("CHANNEL_DIMMING = 0x51", header_src)

    def test_no_private_keys_local_paths_private_ips_or_binaries(self) -> None:
        self_path = Path(__file__).resolve()
        private_key_re = re.compile(r"-----BEGIN [A-Z0-9 ]*PRIVATE KEY-----")
        local_home_re = re.compile(
            r"/(Users|home)/(?!ubuntu|runner|user\b)[a-zA-Z0-9_-]+/"
        )
        private_ip_re = re.compile(r"\b192\.168\.\d{1,3}\.\d{1,3}\b")
        forbidden_exts = {".apk", ".so", ".ab", ".dex", ".arsc", ".bin", ".pdf"}

        for file_path in walk_workspace_files(REPO_ROOT):
            rel = file_path.relative_to(REPO_ROOT)
            self.assertNotIn(
                file_path.suffix.lower(),
                forbidden_exts,
                f"Forbidden binary/resource file detected in workspace: {rel}",
            )
            if file_path.resolve() == self_path:
                continue

            content = file_path.read_text(encoding="utf-8")
            self.assertNotRegex(
                content, private_key_re, f"Private key detected in {rel}"
            )
            self.assertNotRegex(
                content, local_home_re, f"Hardcoded host path detected in {rel}"
            )
            self.assertNotRegex(
                content, private_ip_re, f"Private LAN IP detected in {rel}"
            )

    def test_no_reverse_engineering_or_proprietary_internal_terms(self) -> None:
        self_path = Path(__file__).resolve()
        # Construct forbidden terms dynamically to avoid self-matching
        forbidden_patterns = [
            re.compile(r"逆アセンブル"),
            re.compile(r"リバースエンジニアリング"),
            re.compile(r"バイナリ解析"),
            re.compile(r"\blibapp\.so\b", re.IGNORECASE),
            re.compile(r"\bdirect_tab_screen2\b", re.IGNORECASE),
            re.compile(r"\bSigVendorPacketMaker\b"),
            re.compile(r"\bSceneControllerGetParametersService\b"),
            re.compile(r"\bSC_CIRCUIT_TYPE\b"),
            re.compile(r"\bapkpure\b", re.IGNORECASE),
            re.compile(r"\bdisasm\b", re.IGNORECASE),
            re.compile(r"\bdecompile\b", re.IGNORECASE),
        ]

        for file_path in walk_workspace_files(REPO_ROOT):
            if file_path.resolve() == self_path:
                continue
            # Skip workflow prompt instructions that list forbidden review criteria
            if file_path.name == "ai-review.yml" or file_path.name in {
                "AGENTS.md",
                "CONTRIBUTING.md",
                "PULL_REQUEST_TEMPLATE.md",
            }:
                # Still enforce no proprietary symbol names in governance docs
                symbol_patterns = forbidden_patterns[3:]
                content = file_path.read_text(encoding="utf-8")
                for pat in symbol_patterns:
                    self.assertNotRegex(
                        content,
                        pat,
                        f"Proprietary symbol reference '{pat.pattern}' in {file_path.relative_to(REPO_ROOT)}",
                    )
                continue

            content = file_path.read_text(encoding="utf-8")
            rel = file_path.relative_to(REPO_ROOT)
            for pat in forbidden_patterns:
                self.assertNotRegex(
                    content,
                    pat,
                    f"Forbidden reverse-engineering or internal symbol term '{pat.pattern}' in {rel}",
                )

    def test_no_emojis_in_repository_files(self) -> None:
        self_path = Path(__file__).resolve()
        emoji_re = re.compile(
            "[\U0001f300-\U0001f9ff\U0001fa00-\U0001faff\u2600-\u26ff\u2700-\u27bf]"
        )
        for file_path in walk_workspace_files(REPO_ROOT):
            if file_path.resolve() == self_path:
                continue
            content = file_path.read_text(encoding="utf-8")
            rel = file_path.relative_to(REPO_ROOT)
            self.assertNotRegex(
                content,
                emoji_re,
                f"Emoji character detected in {rel} (no-emoji policy)",
            )

    def test_standalone_pairing_invariants(self) -> None:
        header_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.h"
        ).read_text(encoding="utf-8")
        cpp_src = (
            REPO_ROOT
            / "components/daiko_scenecontroller_ble/daiko_scenecontroller_ble.cpp"
        ).read_text(encoding="utf-8")
        init_py = (
            REPO_ROOT / "components/daiko_scenecontroller_ble/__init__.py"
        ).read_text(encoding="utf-8")

        # Documentation invariants: pairing guide must exist, key extraction guide must be deleted
        self.assertTrue(
            (REPO_ROOT / "PAIRING_GUIDE.md").exists(),
            "PAIRING_GUIDE.md must exist as the primary setup guide",
        )
        self.assertFalse(
            (REPO_ROOT / "KEY_EXTRACTION_GUIDE.md").exists(),
            "KEY_EXTRACTION_GUIDE.md must be deleted to eliminate key extraction risks",
        )

        # C++ state machine and API invariants
        self.assertIn("enum class PairingState : uint8_t", header_src)
        self.assertIn("start_pairing", header_src)
        self.assertIn("cancel_pairing", header_src)
        self.assertIn("ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD", cpp_src)
        self.assertIn("ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND", cpp_src)
        self.assertIn("ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD", cpp_src)

        # Auto-created entities must include pairing controls
        self.assertIn("start_pairing", init_py)
        self.assertIn("cancel_pairing", init_py)
        self.assertIn("pairing_state", init_py)

    def test_ai_review_workflow_concurrency_isolation(self) -> None:
        workflow = (REPO_ROOT / ".github/workflows/ai-review.yml").read_text(
            encoding="utf-8"
        )
        self.assertRegex(
            workflow,
            r"group:\s*\$\{\{\s*github\.workflow\s*\}\}-\$\{\{\s*github\.event_name\s*\}\}-\$\{\{\s*github\.event\.pull_request\.number\s*\|\|\s*github\.ref\s*\}\}",
        )


if __name__ == "__main__":
    unittest.main()
