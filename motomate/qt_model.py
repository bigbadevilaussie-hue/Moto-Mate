"""Pure host-side presentation rules; never communicates with the motorcycle."""
import math

GUI_VERSION = "2.01"
ROUTES = ("/moto/status", "/moto/log/status", "/moto/power/status", "/moto/bench/status")
COLLECT = "/moto/log/start"


def validate(path, value):
    if not isinstance(value, dict):
        raise ValueError("not an object")
    if path == "/moto/status":
        if value.get("device") != "Moto-Mate" or not isinstance(value.get("osa_version"), str):
            raise ValueError("wrong device or version")
        if type(value.get("alive")) is not bool or type(value.get("connected")) is not bool:
            raise ValueError("missing connection flags")
    elif path in (*ROUTES[1:], COLLECT):
        if not isinstance(value.get("phase"), str):
            raise ValueError("missing phase")
        if path in ("/moto/log/status", COLLECT) and value.get("device_deletion") is not False:
            raise ValueError("unexpected deletion policy")
    else:
        raise ValueError("unknown route")
    return value


def link_label(value):
    if value is None:
        return "OSA unavailable"
    if value.get("connected"):
        return "BLE connected"
    return "BLE disconnected"


def age_label(value):
    age = value.get("age_s") if value else None
    if isinstance(age, (int, float)) and not isinstance(age, bool) and math.isfinite(age) and age >= 0:
        return f"{age:.1f} s since last sample"
    return "No sample received"


def budget(sleep_ma, awake_ma, sleep_s, awake_s, capacity_ah, days):
    args = (sleep_ma, awake_ma, sleep_s, awake_s, capacity_ah, days)
    if any(not math.isfinite(x) or x < 0 for x in args) or sleep_s + awake_s <= 0 or capacity_ah <= 0:
        raise ValueError("invalid budget")
    avg = (sleep_ma * sleep_s + awake_ma * awake_s) / (sleep_s + awake_s)
    used = avg * 24 * days / 1000
    return avg, used, 100 * used / capacity_ah
