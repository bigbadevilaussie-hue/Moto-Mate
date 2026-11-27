"""Offline packaged-runtime smoke: no OSA requests and no hardware access."""
import json
from pathlib import Path
import traceback
from PySide6.QtWidgets import QApplication
from .qt_icon import app_icon
from .qt_theme import apply_arctic_theme
from .qt_model import GUI_VERSION, MAINT_STATUS


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def run_smoke(report_path):
    report=Path(report_path)
    result={"passed":False,"gui_version":GUI_VERSION,"network_requests":0}
    try:
        app=QApplication(["Moto-Mate smoke"])
        app.setApplicationName("Moto-Mate")
        icon=app_icon()
        require(not icon.isNull() and not icon.pixmap(24,24).isNull(),"bundled approved icon missing")
        app.setWindowIcon(icon)
        apply_arctic_theme(app)
        from .qt_instrument import Instrument
        window=Instrument(polling=False)
        require(not window.timer.isActive(),"offline smoke timer running")
        def forbidden(*args,**kwargs):
            result["network_requests"]+=1
            raise RuntimeError("network request in offline smoke")
        window.client.request=forbidden
        window.show()
        app.processEvents()
        require(window.isVisible(),"main instrument not visible")
        require(window.windowTitle().endswith(GUI_VERSION),"incorrect GUI version")
        require(not window.maintenance.upload.isEnabled(),"OTA enabled without OSA")
        window.values.update({
            "/moto/status":{"device":"Moto-Mate","osa_version":"SMOKE","mode":"bench",
                            "ble_state":"idle","connected":False,"alive":False,"age_s":None,"data":None},
            "/moto/log/status":{"phase":"idle","device_deletion":False},
            MAINT_STATUS:{"phase":"idle","session":False,"settings":None}})
        window.render()
        require("SMOKE" in window.host.text(),"cache display not updated")
        require(window.maintenance.connect_button.isEnabled(),"connect control not available")
        require(not window.maintenance.upload.isEnabled(),"OTA enabled without session")
        for name, tool in window.tools.items():
            window.open_tool(name)
            app.processEvents()
            require(tool.isVisible(),"tool not visible: "+name)
            tool.hide()
        require(result["network_requests"]==0 and not window.client.pending,"unexpected request")
        result.update(passed=True,tools=sorted(window.tools),icon=True)
        window.close()
        app.processEvents()
    except Exception:
        result["error"]=traceback.format_exc()
    report.write_text(json.dumps(result,indent=2)+"\n")
    return 0 if result["passed"] else 1
