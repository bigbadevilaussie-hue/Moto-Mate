"""GUI model tests; no Qt, network or device required."""
import unittest
from motomate.qt_model import budget, validate, link_label, age_label, COLLECT


class QtModelTest(unittest.TestCase):
    def test_budget(self):
        avg, ah, pct = budget(1, 20, 300, 10, 10.5, 56)
        self.assertAlmostEqual(avg, 500 / 310)
        self.assertAlmostEqual(ah, avg * 24 * 56 / 1000)
        self.assertAlmostEqual(pct, ah / 10.5 * 100)

    def test_bad_budget(self):
        for args in ((1, 20, 0, 0, 10.5, 56), (1, 20, 300, 10, 0, 56), (float("nan"), 20, 300, 10, 10.5, 56)):
            with self.assertRaises(ValueError):
                budget(*args)

    def test_unavailable(self):
        self.assertEqual(link_label(None), "OSA unavailable")
        self.assertEqual(age_label(None), "No sample received")

    def test_disconnected_not_sleep(self):
        value = {"device":"Moto-Mate","osa_version":"1.43","connected":False,"alive":False,"age_s":None}
        self.assertEqual(link_label(validate("/moto/status", value)), "BLE disconnected")
        self.assertEqual(age_label(value), "No sample received")

    def test_wrong_device(self):
        with self.assertRaises(ValueError):
            validate("/moto/status", {"device":"Bike-Mate"})

    def test_collection_no_delete(self):
        for flag in (True, None, 0):
            with self.assertRaises(ValueError):
                validate(COLLECT, {"phase":"running","device_deletion":flag})
        self.assertEqual(validate(COLLECT, {"phase":"running","device_deletion":False})["phase"], "running")


if __name__ == "__main__":
    unittest.main()
