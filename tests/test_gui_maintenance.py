"""Pure maintenance guard tests; no device/network access."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from moto_gui_maintenance import verify_status, desired_settings

class MaintenanceGuard(unittest.TestCase):
    def test_status(self):
        base={"device":"Moto-Mate","mode":"ota_bench","ota_busy":False,"settings":{"profile":"BENCH"}}
        verify_status(base)
        for change in ({"device":"Bike-Mate"},{"ota_busy":True},{"mode":"field"},{"settings":{"profile":"FIELD"}}):
            with self.assertRaises(ValueError):
                verify_status({**base,**change})

    def test_write(self):
        base={"nvs_ok":True,"profile":"BENCH","bench_sleep":False,"verbose":False,"bench_sleep_log":False}
        self.assertEqual(desired_settings(base,1,0,0,0),{"profile":"BENCH","bench_sleep":"1","verbose":"0"})
        for change in ({"bench_sleep_log":True},{"verbose":True},{"nvs_ok":False},{"profile":"FIELD"}):
            with self.assertRaises(ValueError):
                desired_settings({**base,**change},1,0,0,0)
