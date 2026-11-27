"""Build receipt checks; no compiler or device required."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from moto_build import receipt

class BuildReceipt(unittest.TestCase):
    def test_s3_receipt(self):
        data=bytearray(24)
        data[0]=0xe9
        data[12]=9
        meta=receipt(bytes(data),"1.35")
        self.assertEqual(meta["version"],"1.35")
        self.assertEqual(meta["bytes"],24)
        self.assertEqual(len(meta["md5"]),32)
        self.assertEqual(len(meta["sha256"]),64)

    def test_bad_image(self):
        with self.assertRaises(ValueError):
            receipt(bytes(24),"1.35")
