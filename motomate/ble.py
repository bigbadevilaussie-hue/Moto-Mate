"""Read-only OSA commissioning status. GUI never owns a BLE connection."""
import json
import threading
import urllib.request
from .config import OSA_URL

class BLEWorker(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.running = True
        self.status_text = "OSA commissioning · connecting"

    def run(self):
        while self.running:
            try:
                with urllib.request.urlopen(OSA_URL + "/moto/status", timeout=3) as response:
                    value = json.load(response)
                if value.get("device") != "Moto-Mate":
                    raise ValueError("wrong device")
                self.status_text = "OSA online · telemetry pending"
            except (OSError, ValueError, TypeError, AttributeError):
                self.status_text = "OSA unavailable · no device data"
            for _ in range(20):
                if not self.running:
                    return
                threading.Event().wait(0.25)
