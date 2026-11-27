"""Dumb Qt terminal: all maintenance actions are owned by OSA."""
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QPushButton, QLabel,
                               QCheckBox, QMessageBox, QPlainTextEdit)
from .qt_model import MAINT, MAINT_STATUS


class Maintenance(QWidget):
    def __init__(self, owner):
        super().__init__()
        self.owner = owner
        self.pending = False
        self.settings = None
        self.last_job = None
        box = QVBoxLayout(self)
        box.setContentsMargins(14,14,14,14)
        def text(s):
            w=QLabel(s)
            w.setWordWrap(True)
            w.setTextFormat(Qt.TextFormat.PlainText)
            box.addWidget(w)
            return w
        self.state = text("Awaiting OSA maintenance status.")
        text("Wake Moto-Mate manually into BENCH maintenance. Remote wake and device logout are not implemented yet.")
        self.connect_button=QPushButton("Connect via OSA")
        self.connect_button.clicked.connect(lambda: self.run("connect"))
        box.addWidget(self.connect_button)
        self.disconnect_button=QPushButton("End local session")
        self.disconnect_button.clicked.connect(lambda: self.run("end"))
        box.addWidget(self.disconnect_button)
        text("SETTINGS • BENCH ONLY")
        self.read_button=QPushButton("Read settings")
        self.read_button.clicked.connect(lambda: self.run("read"))
        box.addWidget(self.read_button)
        self.sleep=QCheckBox("Arm BENCH sleep on next reset")
        self.verbose=QCheckBox("Verbose BENCH diagnostics")
        box.addWidget(self.sleep)
        box.addWidget(self.verbose)
        self.apply=QPushButton("Apply and verify settings")
        self.apply.clicked.connect(self.save)
        box.addWidget(self.apply)
        text("OTA • automatic fixed-build acquisition")
        self.versions=text("ESP32 —  /  OSA build —")
        self.upload=QPushButton("Update firmware")
        self.upload.setObjectName("nextActionButton")
        self.upload.clicked.connect(self.ota)
        box.addWidget(self.upload)
        self.request_note=text("No automatic update or device contact.")
        self.log=QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMinimumHeight(110)
        box.addWidget(self.log,1)
        self.refresh_controls()

    @property
    def busy(self):
        state=self.owner.values.get(MAINT_STATUS) or {}
        return self.pending or state.get("phase")=="running"

    def refresh_controls(self):
        state=self.owner.values.get(MAINT_STATUS)
        log=self.owner.values.get("/moto/log/status")
        available=bool(state and log and log.get("phase") in ("idle","passed","failed")
                       and not self.owner.post_pending and not self.busy)
        session=bool(state and state.get("session") is True)
        self.connect_button.setEnabled(available and not session)
        self.disconnect_button.setEnabled(available and session)
        self.read_button.setEnabled(available and session)
        self.upload.setEnabled(available and session)
        current=state.get("settings") if state else None
        job=state.get("job_id") if state else None
        if current is None:
            self.settings=None
        elif job!=self.last_job:
            self.settings=current
            self.sleep.setChecked(current["bench_sleep"])
            self.verbose.setChecked(current["verbose"])
        self.last_job=job
        writable=bool(available and session and self.settings and self.settings.get("nvs_ok") is True
                      and self.settings.get("profile")=="BENCH" and self.settings.get("bench_sleep_log") is False)
        for w in (self.sleep,self.verbose,self.apply):
            w.setEnabled(writable)
        if state:
            self.state.setText(str(state.get("stage") or state.get("phase","—"))+
                               (" • local session open" if session else ""))
            self.versions.setText(f"ESP32 {state.get('device_version') or '—'}  /  OSA build {state.get('build_version') or '—'}")
            self.log.setPlainText("\n".join(f"{k}: {state.get(k)}" for k in
                ("phase","action","job_id","md5","sha256","verified","error")))
        else:
            self.state.setText("OSA maintenance unavailable. OSA V1.44 required.")
            self.versions.setText("ESP32 —  /  OSA build —")
            self.log.clear()

    def run(self,action,payload=None):
        if self.busy:
            return
        self.pending=True
        self.request_note.setText("Request sent to OSA; awaiting acceptance.")
        self.owner.render()
        self.owner.client.request(MAINT+action,explicit=True,payload=payload)

    def save(self):
        if not self.apply.isEnabled():
            return
        if QMessageBox.question(self,"Confirm settings","Ask OSA to save and verify BENCH settings?",
                QMessageBox.StandardButton.Yes|QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No)==QMessageBox.StandardButton.Yes:
            self.run("write",{"bench_sleep":self.sleep.isChecked(),"verbose":self.verbose.isChecked(),
                              "old_bench_sleep":self.settings["bench_sleep"],"old_verbose":self.settings["verbose"]})

    def ota(self):
        if not self.upload.isEnabled():
            return
        if QMessageBox.question(self,"Confirm OTA","Ask OSA to acquire the fixed build, check revision and update if newer? Do not disconnect power during update.",
                QMessageBox.StandardButton.Yes|QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No)==QMessageBox.StandardButton.Yes:
            self.run("ota")
