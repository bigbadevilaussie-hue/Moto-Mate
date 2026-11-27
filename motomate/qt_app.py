"""Qt6 Arctic dashboard. Asynchronous loopback OSA calls only."""
import json
import sys
from PySide6.QtCore import Qt, QTimer, QUrl, Signal, QObject, QByteArray
from PySide6.QtNetwork import QNetworkAccessManager, QNetworkRequest, QNetworkReply, QNetworkProxy
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QFrame, QLabel,
    QPushButton, QHBoxLayout, QVBoxLayout, QGridLayout, QStackedWidget,
    QScrollArea, QDoubleSpinBox, QMessageBox, QPlainTextEdit, QSizeGrip)
from .qt_theme import apply_arctic_theme
from .qt_model import GUI_VERSION, ROUTES, COLLECT, COMMANDS, MAINT, MAINT_STATUS, validate, link_label, age_label, budget

BASE = "http://127.0.0.1:9999"
LIMIT = 65536


class OsaClient(QObject):
    received = Signal(str, object)
    failed = Signal(str, str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.manager = QNetworkAccessManager(self)
        self.manager.setProxy(QNetworkProxy(QNetworkProxy.ProxyType.NoProxy))
        self.pending = {}

    def request(self, path, explicit=False, payload=None):
        if path not in ROUTES and not (path in COMMANDS and explicit):
            raise ValueError("route not allowed")
        if path in self.pending:
            return
        req = QNetworkRequest(QUrl(BASE + path))
        req.setTransferTimeout(3000)
        req.setAttribute(QNetworkRequest.Attribute.RedirectPolicyAttribute,
                         QNetworkRequest.RedirectPolicy.ManualRedirectPolicy)
        if path in COMMANDS:
            if payload is not None:
                req.setHeader(QNetworkRequest.KnownHeaders.ContentTypeHeader, "application/json")
            body = QByteArray(json.dumps(payload).encode("utf-8")) if payload is not None else QByteArray()
            reply = self.manager.post(req, body)
        else:
            reply = self.manager.get(req)
        self.pending[path] = reply
        # Bound accumulation even if the server returns a malformed response.
        reply.setReadBufferSize(LIMIT + 1)
        reply.readyRead.connect(lambda: self.check_size(reply))
        reply.finished.connect(lambda: self.finish(path, reply))

    @staticmethod
    def check_size(reply):
        if reply.bytesAvailable() > LIMIT:
            reply.abort()

    def finish(self, path, reply):
        self.pending.pop(path, None)
        try:
            code = reply.attribute(QNetworkRequest.Attribute.HttpStatusCodeAttribute)
            if reply.error() != QNetworkReply.NetworkError.NoError or code not in (200, 202):
                raise ValueError(f"OSA request failed ({code or reply.errorString()})")
            raw = bytes(reply.readAll())
            if len(raw) > LIMIT:
                raise ValueError("response too large")
            value = validate(path, json.loads(raw))
            self.received.emit(path, value)
        except (ValueError, TypeError, UnicodeError) as exc:
            self.failed.emit(path, str(exc))
        finally:
            reply.deleteLater()

    def poll(self):
        for path in ROUTES:
            self.request(path)


def label(text="", name=None):
    widget = QLabel(text)
    widget.setTextFormat(Qt.TextFormat.PlainText)
    if name:
        widget.setObjectName(name)
    widget.setWordWrap(True)
    return widget


def card(title, detail=""):
    frame = QFrame()
    frame.setObjectName("card")
    layout = QVBoxLayout(frame)
    layout.setContentsMargins(14, 10, 14, 10)
    layout.setSpacing(7)
    layout.addWidget(label(title, "section"))
    if detail:
        layout.addWidget(label(detail, "muted"))
    return frame, layout


def metric(title, initial, detail):
    frame, layout = card(title)
    value = label(initial, "metric")
    layout.addWidget(value)
    note = label(detail, "muted")
    layout.addWidget(note)
    return frame, value, note


class Dashboard(QMainWindow):
    def page(self, title, sub):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(10)
        layout.addWidget(label(title, "pageTitle"))
        layout.addWidget(label(sub, "muted"))
        return page, layout

    def overview(self):
        page, layout = self.page("Motorcycle overview", "TRIUMPH SPRINT ST 1050  •  PARKED MONITORING")
        frame, hero = card("Motorcycle connection")
        self.connection = label("Awaiting OSA", "metric")
        self.age = label("No sample received", "muted")
        hero.addWidget(self.connection)
        hero.addWidget(self.age)
        self.mode = label("Mode —  /  BLE —", "muted")
        hero.addWidget(self.mode)
        layout.addWidget(frame)
        grid = QGridLayout()
        grid.setSpacing(12)
        for i, (title, detail) in enumerate((
            ("Battery voltage", "No qualified ADC sample"),
            ("Ambient temperature", "No qualified SHT40 sample"),
            ("Relative humidity", "No qualified SHT40 sample"),
            ("Cranking reserve", "No qualified crank evidence"))):
            frame, _, _ = metric(title, "—", detail)
            grid.addWidget(frame, i // 2, i % 2)
        layout.addLayout(grid)
        frame, info = card("Power policy", "Design targets, not live device measurements.")
        info.addWidget(label("300 s sleep  •  ≤10 s necessary awake  •  BLE first"))
        info.addWidget(label("Wi-Fi: explicit maintenance, OTA and critical paths."))
        layout.addWidget(frame)
        layout.addStretch()
        return page

    def collection(self):
        page, layout = self.page("Log collection", "Explicit BENCH maintenance only • no device deletion")
        frame, self.job, self.job_note = metric("OSA collection job", "—", "Awaiting host status")
        layout.addWidget(frame)
        frame, body = card("Manual collection", "Moto-Mate must already be awake in Wi-Fi maintenance. This does not wake a sleeping motorcycle.")
        self.collect_button = QPushButton("Collect and verify host copies")
        self.collect_button.setObjectName("nextActionButton")
        self.collect_button.setEnabled(False)
        self.collect_button.clicked.connect(self.collect)
        body.addWidget(self.collect_button)
        self.collect_note = label("No collection automatically triggered.", "muted")
        body.addWidget(self.collect_note)
        layout.addWidget(frame)
        layout.addStretch()
        return page

    def dyna_ride(self):
        page, layout = self.page("Dyna Ride", "Ride-specific diagnostic evidence")
        grid = QGridLayout()
        for i, title in enumerate(("Crank sag", "Sag recovery", "Charging voltage", "Charge stability", "Undercharging", "Overcharging")):
            frame, _, _ = metric(title, "IDLE", "No qualified ride data")
            grid.addWidget(frame, i // 2, i % 2)
        layout.addLayout(grid)
        layout.addStretch()
        return page

    def dyna_tune(self):
        page, layout = self.page("Dyna Tune", "Optimise protection using evidence, not assumptions")
        for title, detail in (("Rested voltage trend", "Requires comparable parked voltage and temperature samples."),
                              ("Day / night thermal cycles", "Requires timestamped SHT40 observations."),
                              ("Humidity & condensation", "Requires ambient temperature and relative humidity history."),
                              ("Wake energy & reliability", "Requires measured whole-system current and elapsed timing.")):
            frame, body = card(title, detail)
            body.addWidget(label("IDLE • insufficient evidence", "muted"))
            layout.addWidget(frame)
        layout.addStretch()
        return page

    def energy(self):
        page, layout = self.page("Battery budget", "Scenario calculator • modelled additional load, not measurements")
        frame, body = card("Parked duty cycle")
        grid = QGridLayout()
        self.inputs = []
        for i, (name, default, minimum, maximum, decimals, suffix) in enumerate((
            ("Sleep current", 1, 0, 100, 2, " mA"),
            ("Awake current", 20, 0, 1000, 2, " mA"),
            ("Sleep duration", 300, 1, 86400, 0, " s"),
            ("Awake duration", 10, 0, 3600, 1, " s"),
            ("Nominal capacity", 10.5, 0.1, 100, 2, " Ah"),
            ("Parked duration", 56, 1, 365, 0, " days"))):
            spin = QDoubleSpinBox()
            spin.setDecimals(decimals)
            spin.setRange(minimum, maximum)
            spin.setSuffix(suffix)
            spin.setValue(default)
            spin.valueChanged.connect(self.recalculate)
            self.inputs.append(spin)
            grid.addWidget(label(name), i, 0)
            grid.addWidget(spin, i, 1)
        body.addLayout(grid)
        layout.addWidget(frame)
        frame, self.energy_avg, self.energy_note = metric("Projected average current", "—", "—")
        layout.addWidget(frame)
        layout.addWidget(label("Excludes motorcycle parasitic load, self-discharge, temperature and ageing. Nominal Ah is not a cranking reserve estimate.", "muted"))
        self.recalculate()
        layout.addStretch()
        return page

    def recalculate(self, *_):
        if len(self.inputs) != 6 or not hasattr(self, "energy_avg"):
            return
        avg, used, pct = budget(*(spin.value() for spin in self.inputs))
        self.energy_avg.setText(f"{avg:.2f} mA")
        self.energy_note.setText(f"{used:.2f} Ah additional consumption • {pct:.1f}% of nominal capacity")

    def engineering(self):
        page, layout = self.page("Engineering", "Live OSA responses • cache-only polling every five seconds")
        refresh = QPushButton("Refresh host cache")
        refresh.clicked.connect(self.client.poll)
        layout.addWidget(refresh)
        self.raw = QPlainTextEdit()
        self.raw.setReadOnly(True)
        self.raw.setPlainText("Awaiting OSA…")
        layout.addWidget(self.raw, 1)
        return page

    def collect(self):
        if self.post_pending or not self.collect_button.isEnabled():
            return
        if QMessageBox.question(self, "BENCH collection",
                "Moto-Mate must already be awake in Wi-Fi maintenance. Start collection without deleting device files?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No) != QMessageBox.StandardButton.Yes:
            return
        self.post_pending = True
        self.collect_button.setEnabled(False)
        self.collect_note.setText("Request submitted; awaiting OSA acceptance.")
        self.client.request(COLLECT, explicit=True)

    def update_value(self, path, value):
        if path in COMMANDS and path.startswith(MAINT):
            self.maintenance.pending = False
            self.maintenance.request_note.setText("OSA accepted request.")
            path = MAINT_STATUS
        if path == COLLECT:
            self.post_pending = False
            self.collect_note.setText("OSA accepted collection request.")
            path = "/moto/log/status"
        self.values[path] = value
        self.errors.pop(path, None)
        self.render()

    def update_error(self, path, error):
        if path in COMMANDS and path.startswith(MAINT):
            self.maintenance.pending = False
            self.maintenance.request_note.setText("Request not confirmed: " + error)
            self.client.request(MAINT_STATUS)
        elif path == COLLECT:
            self.post_pending = False
            self.collect_note.setText("Request not confirmed: " + error)
            self.client.request("/moto/log/status")
        else:
            self.values.pop(path, None)
            self.errors[path] = error
        self.render()

    def render(self):
        status = self.values.get("/moto/status")
        self.host.setText("OSA " + status["osa_version"] + " • online" if status else "OSA unavailable")
        self.connection.setText(link_label(status))
        self.age.setText(age_label(status))
        self.mode.setText(f"Mode {status.get('mode', '—')}  /  BLE {status.get('ble_state', '—')}" if status else "Mode —  /  BLE —")
        job = self.values.get("/moto/log/status")
        self.job.setText(job["phase"].upper() if job else "UNAVAILABLE")
        if job:
            count = job.get("prepared")
            note = f"Prepared: {count if count is not None else '—'}  •  Device deletion: disabled"
            if job.get("error"):
                note += "  •  " + str(job["error"])
            self.job_note.setText(note)
        else:
            self.job_note.setText("No current collection status")
        self.collect_button.setEnabled(bool(status and status.get("mode") == "bench" and job
            and job.get("phase") in ("idle", "passed", "failed") and not self.post_pending))
        self.raw.setPlainText(json.dumps({path: self.values.get(path, {"error": self.errors.get(path, "awaiting response")})
                                           for path in ROUTES}, indent=2, ensure_ascii=False))


def main():
    smoke = len(sys.argv) == 3 and sys.argv[1] == "--smoke-test"
    if smoke:
        from .qt_smoke import run_smoke
        return run_smoke(sys.argv[2])
    app = QApplication(sys.argv)
    app.setApplicationName("Moto-Mate")
    from .qt_icon import app_icon
    icon = app_icon()
    if not icon.isNull():
        app.setWindowIcon(icon)
    apply_arctic_theme(app)
    from .qt_instrument import Instrument
    window = Instrument()
    screen = window.screen().availableGeometry()
    window.resize(min(window.width(), screen.width()), min(window.height(), screen.height()))
    window.move(screen.center() - window.rect().center())
    window.show()
    return app.exec()
