"""Pure host-side presentation rules; never communicates with the motorcycle."""
import math

GUI_VERSION = "2.07"
ROUTES = ("/moto/status", "/moto/log/status", "/moto/power/status", "/moto/bench/status", "/moto/maintenance/status")
COLLECT = "/moto/log/start"
MAINT = "/moto/maintenance/"
MAINT_STATUS = MAINT + "status"
COMMANDS = (COLLECT, *(MAINT + action for action in ("connect","end","read","write","ota")))


def validate(path, value):
    if not isinstance(value, dict):
        raise ValueError("not an object")
    if path == "/moto/status":
        if value.get("device") != "Moto-Mate" or not isinstance(value.get("osa_version"), str):
            raise ValueError("wrong device or version")
        if type(value.get("alive")) is not bool or type(value.get("connected")) is not bool:
            raise ValueError("missing connection flags")
    elif path in (*ROUTES[1:], *COMMANDS):
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

def live_labels(status):
    data = status.get("data") if status and status.get("alive") is True else None
    def number(key, low, high):
        value = data.get(key) if isinstance(data, dict) else None
        if type(value) in (float, int) and math.isfinite(value) and low <= value <= high:
            return value
        return None
    simulated = isinstance(data, dict) and (data.get("voltage_source") == "synthetic_bench" or data.get("firmware_version") == "1.48")
    rail = number("synthetic_voltage_mv", 0, 20000) if simulated else number("protected_rail_estimate_mv", 0, 65535)
    # Compatibility with older OSA: V1.48 always emits the simulated source in BLE.
    if simulated and rail is None:
        rail = number("protected_rail_estimate_mv", 0, 20000)
    temp = number("temperature_c", -40, 125)
    rh = number("humidity_pct", 0, 100)
    note = "* FAKE ADC • BENCH" if simulated else "* Protected rail estimate • uncalibrated"
    return (f"{rail/1000:.2f} V*" if rail is not None else "—.— V",
            note if rail is not None else "No fresh qualified ADC sample",
            f"{temp:.1f} °C\nTemperature" if temp is not None else "— °C\nTemperature",
            f"{rh:.0f} %\nHumidity" if rh is not None else "— %\nHumidity")

def ota_console(state):
    if not state:
        return "[OSA] Maintenance status unavailable."
    r=state.get("ota_receipt")
    lines=["[OSA] "+str(state.get("stage") or state.get("phase","idle"))]
    remaining=state.get("maintenance_remaining_ms")
    if state.get("session") is True and type(remaining) is int:
        lines.append(f"[MAINT] Remaining {remaining/1000:.0f}s (OSA estimate)")
    if not isinstance(r,dict):
        lines.append("[OTA] No upload receipt yet.")
        return "\n".join(lines)
    lines.append(f"[OTA] {r.get('before_version','—')} -> {r.get('target_version','—')}")
    lines.append(f"[OTA] Original boot {r.get('before_boot','—')} slot {r.get('before_slot','—')}")
    lines.append(f"[OTA] Image {r.get('bytes','—')} bytes")
    lines.append(f"[OTA] MD5 {r.get('md5','—')}")
    lines.append(f"[OTA] SHA256 {r.get('sha256','—')}")
    lines.append("[OTA] Upload acceptance "+("CONFIRMED" if r.get("accepted") is True else "NOT CONFIRMED"))
    if r.get("verified") is True:
        lines.append(f"[BOOT] MOTO-MATE V{r.get('after_version')} boot={r.get('after_boot')} slot={r.get('after_slot')}")
        lines.append("[OTA] PASS — new boot, alternate slot and revision verified")
    else:
        lines.append("[OTA] Reboot not verified; do not retry blindly")
    if r.get("error"):
        lines.append("[OTA] "+str(r["error"]))
    return "\n".join(lines)
