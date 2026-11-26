"""Host-only regression checks. No Tk window or hardware/network access."""
import ast
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class CommissioningGUI(unittest.TestCase):
    def test_python_syntax(self):
        for path in [ROOT / "motomate.py", ROOT / "setup.py", *(ROOT / "motomate").glob("*.py")]:
            with self.subTest(path=path.name):
                ast.parse(path.read_text(), filename=str(path))

    def test_dyna_missing_evidence(self):
        # Execute the actual pure status method without constructing a Tk window.
        source = ast.parse((ROOT / "motomate" / "dynatune_window.py").read_text())
        cls = next(n for n in source.body if isinstance(n, ast.ClassDef) and n.name == "DynaTuneWindow")
        method = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == "_overall_status")
        namespace = {}
        exec(compile(ast.Module(body=[method], type_ignores=[]), "<status>", "exec"), namespace)
        status = namespace["_overall_status"]
        from types import SimpleNamespace
        cases = [({}, "IDLE"), ({"ride": {"a": {"status": "IDLE"}}}, "IDLE"),
                 ({"ride": {"a": {"status": "PASS"}, "b": {"status": "IDLE"}}}, "IDLE"),
                 ({"ride": {"a": {"status": "PASS"}}}, "PASS"),
                 ({"ride": {"a": {"status": "WARN"}}}, "WARN"),
                 ({"ride": {"a": {"status": "FAIL"}}}, "FAIL")]
        for analysis, expected in cases:
            with self.subTest(analysis=analysis):
                self.assertEqual(status(SimpleNamespace(analysis=analysis, meta={}))["level"], expected)

    def test_active_app_has_no_direct_network_calls(self):
        app = (ROOT / "motomate" / "app.py").read_text()
        for forbidden in ("urlopen", "requests.", "http:", "webbrowser", "send_maint_on"):
            self.assertNotIn(forbidden, app)
        worker = (ROOT / "motomate" / "ble.py").read_text()
        self.assertIn('"/moto/status"', worker)
        self.assertNotIn('method="POST"', worker)

if __name__ == "__main__":
    unittest.main()
