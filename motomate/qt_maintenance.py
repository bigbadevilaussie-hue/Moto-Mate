"""Explicit human-intervention tools; never contacted by routine GUI polling."""
from pathlib import Path
import json
import re
import sys
from PySide6.QtCore import QProcess, Qt
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel,
    QCheckBox, QLineEdit, QFileDialog, QMessageBox, QPlainTextEdit)

ROOT = Path(__file__).resolve().parents[1]


class Maintenance(QWidget):
    def __init__(self, owner):
        super().__init__()
        self.owner = owner
        self.busy = False
        self.session = False
        self.settings = None
        self.action = None
        self.buffer = ""
        self.process = QProcess(self)
        self.process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        self.process.readyReadStandardOutput.connect(self.output)
        self.process.finished.connect(self.finished)
        self.process.errorOccurred.connect(self.error)
        box = QVBoxLayout(self)
        box.setContentsMargins(14,14,14,14)
        def text(s):
            w = QLabel(s)
            w.setWordWrap(True)
            w.setTextFormat(Qt.TextFormat.PlainText)
            box.addWidget(w)
            return w
        self.state = text("No local maintenance session.")
        text("Wake Moto-Mate manually into BENCH Wi-Fi maintenance first. Remote activation/deactivation is not available yet. Ending this local session does not put the device to sleep.")
        self.connect_button = QPushButton("Connect to awake Moto-Mate")
        self.connect_button.clicked.connect(lambda: self.run("status"))
        box.addWidget(self.connect_button)
        self.disconnect_button = QPushButton("End local session")
        self.disconnect_button.clicked.connect(self.end)
        box.addWidget(self.disconnect_button)
        text("SETTINGS • BENCH ONLY (FIELD not enabled)")
        self.read_button = QPushButton("Read settings")
        self.read_button.clicked.connect(lambda: self.run("read"))
        box.addWidget(self.read_button)
        self.sleep = QCheckBox("Arm BENCH sleep on next reset")
        self.verbose = QCheckBox("Verbose BENCH diagnostics")
        box.addWidget(self.sleep)
        box.addWidget(self.verbose)
        self.apply = QPushButton("Apply and verify settings")
        self.apply.clicked.connect(self.save)
        box.addWidget(self.apply)
        text("OTA • Application binary only")
        row = QHBoxLayout()
        self.image = QLineEdit(str(ROOT / "build" / "motomate.ino.bin"))
        self.pick = QPushButton("Browse…")
        self.pick.clicked.connect(self.browse)
        row.addWidget(self.image,1)
        row.addWidget(self.pick)
        box.addLayout(row)
        self.version = QLineEdit()
        self.version.setPlaceholderText("Expected firmware version, e.g. 1.35")
        box.addWidget(self.version)
        self.upload = QPushButton("Upload and verify reboot")
        self.upload.clicked.connect(self.ota)
        box.addWidget(self.upload)
        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.document().setMaximumBlockCount(300)
        self.log.setMinimumHeight(140)
        box.addWidget(self.log,1)
        self.refresh_controls()

    def refresh_controls(self):
        job = self.owner.values.get("/moto/log/status")
        available = bool(job and job.get("phase") in ("idle","passed","failed")
                         and not self.owner.post_pending and not self.busy)
        self.connect_button.setEnabled(available and not self.session)
        self.disconnect_button.setEnabled(not self.busy and self.session)
        ready = available and self.session
        self.read_button.setEnabled(ready)
        writable = bool(ready and self.settings and self.settings.get("nvs_ok") is True
                        and self.settings.get("profile") == "BENCH" and self.settings.get("bench_sleep_log") is False)
        self.sleep.setEnabled(writable)
        self.verbose.setEnabled(writable)
        self.apply.setEnabled(writable)
        for widget in (self.image,self.version,self.pick,self.upload):
            widget.setEnabled(ready)

    def browse(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select application binary", self.image.text(), "Application binaries (*.bin)")
        if path:
            self.image.setText(path)

    def end(self):
        if not self.busy:
            self.session = False
            self.settings = None
            self.state.setText("Local session ended. Device state unchanged.")
            self.refresh_controls()

    def save(self):
        if not self.apply.isEnabled():
            return
        if QMessageBox.question(self, "Confirm settings", "Write BENCH settings and verify readback?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No) == QMessageBox.StandardButton.Yes:
            self.run("write", str(int(self.sleep.isChecked())), str(int(self.verbose.isChecked())),
                     str(int(self.settings["bench_sleep"])), str(int(self.settings["verbose"])))

    def ota(self):
        if not self.upload.isEnabled():
            return
        path = Path(self.image.text()).expanduser().resolve()
        version = self.version.text().strip()
        if not path.is_file() or not re.fullmatch(r"\d+\.\d+(?:\.\d+)?",version):
            self.state.setText("Select an existing application binary and enter its expected version.")
            return
        if QMessageBox.question(self, "Confirm OTA", f"Upload {path.name} and verify firmware {version}? Do not reset or disconnect power during the operation.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No) == QMessageBox.StandardButton.Yes:
            self.run("ota",str(path),version)

    def run(self, action, *args):
        job = self.owner.values.get("/moto/log/status")
        if self.busy or self.owner.post_pending or not job or job.get("phase") not in ("idle","passed","failed"):
            return
        if action != "status" and not self.session:
            return
        self.busy = True
        self.action = action
        self.buffer = ""
        self.log.clear()
        self.state.setText("Operation in progress: " + action)
        self.owner.render()
        self.process.setWorkingDirectory(str(ROOT))
        self.process.start(sys.executable, ["-u",str(ROOT/"tools"/"moto_gui_maintenance.py"),action,*args])

    def output(self):
        chunk = bytes(self.process.readAllStandardOutput()).decode("utf-8",errors="replace")
        self.buffer = (self.buffer + chunk)[-65536:]
        self.log.setPlainText(self.buffer)
        self.log.moveCursor(self.log.textCursor().MoveOperation.End)

    def error(self, error):
        if error == QProcess.ProcessError.FailedToStart:
            self.busy = False
            self.session = False
            self.settings = None
            self.state.setText("Maintenance helper failed to start.")
            self.owner.render()

    def finished(self, code, status):
        self.output()
        action = self.action
        self.busy = False
        if code != 0 or status != QProcess.ExitStatus.NormalExit:
            self.session = False
            self.settings = None
            self.state.setText("Operation not confirmed. Check device before retrying.")
        elif action == "status":
            self.session = True
            self.state.setText("Awake BENCH device verified. Explicit local session open.")
        elif action in ("read","write"):
            try:
                self.settings = json.loads(self.buffer)
                self.sleep.setChecked(self.settings["bench_sleep"])
                self.verbose.setChecked(self.settings["verbose"])
                self.state.setText("Settings read back." if action == "read" else "Settings saved and verified.")
            except (ValueError,KeyError,TypeError):
                self.settings = None
                self.state.setText("Settings output not understood; no write enabled.")
        elif action == "ota":
            self.session = False
            self.settings = None
            self.state.setText("OTA verified by new boot, alternate slot and expected version. Reconnect explicitly.")
        self.owner.render()
