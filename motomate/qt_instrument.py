"""Compact Arctic instrument and independent tool windows."""
import json
from PySide6.QtCore import Qt, QTimer, QPropertyAnimation, QEasingCurve, QPoint
from PySide6.QtGui import QPainter, QColor, QPen, QFont
from PySide6.QtWidgets import (QMainWindow, QWidget, QFrame, QLabel, QPushButton,
    QHBoxLayout, QVBoxLayout, QGridLayout, QScrollArea, QSizeGrip, QDialog, QMessageBox)
from .qt_app import Dashboard, OsaClient, card, label
from .qt_model import GUI_VERSION, link_label, age_label
from .qt_maintenance import Maintenance


class EmptyTrend(QWidget):
    """Honest empty graph until an actual historical data contract exists."""
    def __init__(self, unit, parent=None):
        super().__init__(parent)
        self.unit = unit
        self.setMinimumHeight(74)

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setPen(QPen(QColor("#dceaf5"), 1))
        for i in range(4):
            y = 7 + i * (self.height() - 14) // 3
            p.drawLine(5, y, self.width()-5, y)
        p.setPen(QColor("#355670"))
        p.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter,
                   "Waiting for qualified " + self.unit + " history")
        p.end()


class Instrument(Dashboard):
    def __init__(self):
        # Reuse tested page builders and cache handling, not the wide layout.
        QMainWindow.__init__(self)
        self.setWindowTitle(f"Moto-Mate • Arctic {GUI_VERSION}")
        self.setWindowFlag(Qt.WindowType.FramelessWindowHint)
        self.setMinimumSize(365, 550)
        self.resize(420, 690)
        self.values, self.errors = {}, {}
        self.post_pending = False
        self.client = OsaClient(self)
        self.client.received.connect(self.update_value)
        self.client.failed.connect(self.update_error)
        root = QWidget()
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)
        layout.setContentsMargins(12, 8, 12, 5)
        layout.setSpacing(8)
        head = QHBoxLayout()
        menu = QPushButton("☰")
        menu.setFixedWidth(44)
        menu.setToolTip("Open tools")
        menu.clicked.connect(self.toggle_drawer)
        head.addWidget(menu)
        head.addWidget(label("MOTO·MATE", "brand"), 1)
        layout.addLayout(head)
        layout.addWidget(label("SPRINT ST 1050  •  BATTERY FIRST", "muted"))
        frame, body = card("Connection")
        self.connection = label("Awaiting OSA", "section")
        self.age = label("No sample received", "muted")
        self.mode = label("Mode — / BLE —", "muted")
        for w in (self.connection, self.age, self.mode):
            body.addWidget(w)
        layout.addWidget(frame)
        frame, body = card("Battery voltage")
        voltage = label("—.— V", "metric")
        voltage.setStyleSheet("font-size: 44px; color: #176aab; font-weight: 750;")
        voltage.setAlignment(Qt.AlignmentFlag.AlignCenter)
        body.addWidget(voltage)
        body.addWidget(label("No qualified ADC sample", "muted"))
        layout.addWidget(frame)
        frame, body = card("Environment")
        readings = QHBoxLayout()
        for text in ("— °C\nTemperature", "— %\nHumidity", "—\nStorage"):
            item = label(text)
            item.setAlignment(Qt.AlignmentFlag.AlignCenter)
            readings.addWidget(item, 1)
        body.addLayout(readings)
        layout.addWidget(frame)
        for title, unit in (("Voltage trend", "voltage"), ("Temperature / humidity", "environment")):
            frame, body = card(title)
            body.addWidget(EmptyTrend(unit))
            layout.addWidget(frame, 1)
        foot = QHBoxLayout()
        self.host = label("Connecting to OSA…", "muted")
        foot.addWidget(self.host, 1)
        grip = QSizeGrip(root)
        grip.setFixedSize(16,16)
        foot.addWidget(grip)
        layout.addLayout(foot)

        self.tools = {}
        for title, make in (("Log collection", self.collection), ("Dyna Ride", self.dyna_ride),
                            ("Dyna Tune", self.dyna_tune), ("Battery budget", self.energy),
                            ("Engineering", self.engineering)):
            self.add_tool(title, make(), 610, 550)
        self.maintenance = Maintenance(self)
        self.add_tool("Human intervention", self.maintenance, 610, 640)
        for title, text in (("Settings", "Settings are available in Human intervention after an explicit connection."),
                            ("OTA firmware", "OTA is available in Human intervention after an explicit connection."),
                            ("Serial console", "Firmware serial-over-Wi-Fi is not implemented. Use the existing USB serial terminal."),
                            ("Reports / Ride finder", "Historical ride processing is not yet connected to qualified Moto-Mate data.")):
            if title in ("Settings", "OTA firmware"):
                continue
            page, box = self.page(title, text)
            box.addStretch()
            self.add_tool(title, page, 480, 310)
        self.drawer = QFrame(root)
        self.drawer.setObjectName("sidebar")
        self.drawer.setFixedWidth(235)
        nav = QVBoxLayout(self.drawer)
        nav.setContentsMargins(12, 12, 12, 12)
        nav.setSpacing(5)
        close = QPushButton("‹  Close menu")
        close.clicked.connect(self.toggle_drawer)
        nav.addWidget(close)
        for name in ("Human intervention", "Settings", "OTA firmware", "Log collection",
                     "Serial console", "Reports / Ride finder", "Dyna Ride", "Dyna Tune",
                     "Battery budget", "Engineering"):
            button = QPushButton(name)
            button.setObjectName("nav")
            button.clicked.connect(lambda checked=False, n=name: self.open_tool(n))
            nav.addWidget(button)
        nav.addStretch()
        self.drawer.hide()
        self.drawer_open = False
        self.animation = QPropertyAnimation(self.drawer, b"pos", self)
        self.animation.setDuration(160)
        self.animation.setEasingCurve(QEasingCurve.Type.OutCubic)
        self.animation.finished.connect(self.drawer_finished)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.client.poll)
        self.timer.start(5000)
        QTimer.singleShot(0, self.client.poll)

    def add_tool(self, title, page, width, height):
        window = QMainWindow(self, Qt.WindowType.Window)
        window.setWindowTitle(title + " • Moto-Mate")
        window.setWindowFlag(Qt.WindowType.FramelessWindowHint)
        window.setMinimumSize(350, 250)
        window.resize(width, height)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(page)
        window.setCentralWidget(scroll)
        self.tools[title] = window

    def toggle_drawer(self):
        self.animation.stop()
        self.drawer_open = not self.drawer_open
        self.drawer.resize(235, self.centralWidget().height())
        if not self.drawer.isVisible():
            self.drawer.move(-235, 0)
            self.drawer.show()
        self.drawer.raise_()
        self.animation.setStartValue(self.drawer.pos())
        self.animation.setEndValue(QPoint(0 if self.drawer_open else -235, 0))
        self.animation.start()

    def drawer_finished(self):
        if not self.drawer_open:
            self.drawer.hide()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if hasattr(self, "drawer"):
            self.drawer.resize(235, self.centralWidget().height())

    def open_tool(self, name):
        if self.drawer_open:
            self.toggle_drawer()
        if name in ("Settings", "OTA firmware"):
            name = "Human intervention"
        window = self.tools[name]
        if not window.isVisible():
            screen = self.screen().availableGeometry()
            window.move(screen.center() - window.rect().center())
        window.show()
        window.raise_()
        window.activateWindow()

    def collect(self):
        if self.maintenance.busy:
            return
        super().collect()

    def render(self):
        super().render()
        if self.maintenance.busy:
            self.collect_button.setEnabled(False)
        self.maintenance.refresh_controls()

    def closeEvent(self, event):
        if self.maintenance.busy:
            QMessageBox.information(self, "Operation active", "Wait for the active maintenance operation to finish before quitting.")
            event.ignore()
            return
        if self.post_pending:
            event.ignore()
            return
        super().closeEvent(event)
