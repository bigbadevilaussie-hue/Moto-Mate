# === Moto-Mate GUI: app ===
# auto-extracted, edit here ===

import tkinter as tk
import tkinter.ttk
from tkinter import messagebox
import customtkinter as ctk
import time, sys
from datetime import datetime
from .config import *
from . import state
from .helpers import *
from .widgets import *
from .ble import *
from .reports import *


class App:
    def __init__(self, root):
        self.root = root
        self.theme = THEME
        root.title("Moto-Mate")
        root.configure(bg=self.theme["bg"])
        root.resizable(False, False)
        ctk.set_appearance_mode("light")  # V4.49: day-only
        ctk.set_default_color_theme("blue")
        self.worker = BLEWorker()
        # V5.47: menus are inline in the sidebar, no tk.Menu trees.
        # _update_maint_menu() has no widget to touch anymore; it
        # becomes a no-op, kept for callers.

        # V5.47: slide sidebar. Adapted from slide_menu.py.
        self._sidebar_w = 190
        self._sidebar_x = -self._sidebar_w
        self._sidebar_job = None
        self._sidebar_open = False
        self._maint_buttons = {}   # populated below, walked on each tick

        # body fills the whole window, underneath the sidebar
        body = ctk.CTkFrame(root, fg_color="transparent")
        body.pack(fill="both", expand=True)
        self._body = body

        # burger button at the top-left of the body
        hdr = ctk.CTkFrame(body, fg_color="transparent")
        hdr.pack(fill="x", padx=10, pady=(10, 0))
        burger = ctk.CTkButton(hdr, text="☰", width=44, height=36,
                               fg_color=("#ffffff", "#232334"),
                               text_color=("#1e1e2e", "#cdd6f4"),
                               hover_color=("#d8dde8", "#313145"),
                               font=ctk.CTkFont(size=18),
                               command=self._sidebar_toggle)
        burger.pack(side="left")

        # sidebar placed off-screen to the left
        sidebar = ctk.CTkFrame(root, width=self._sidebar_w,
                               corner_radius=0,
                               fg_color="#f5f6fa")
        sidebar.place(x=self._sidebar_x, y=0, relheight=1)
        sidebar.pack_propagate(False)
        self._sidebar = sidebar

        SB_BG = "#f5f6fa"
        SB_FG = "#1e1e2e"
        SB_HOVER = "#e6e9f0"
        SB_SUB_FG = "#5a5f70"

        def _wrap(cmd):
            # V4.47: run the command, then close the sidebar.
            def _run():
                try:
                    cmd()
                finally:
                    self._sidebar_close()
            return _run

        def _sb_action(parent, text, command, fg=None, hover=None):
            return ctk.CTkButton(parent, text=text,
                                 command=command,
                                 anchor="w", height=40,
                                 corner_radius=8,
                                 fg_color=SB_BG,
                                 hover_color=hover or SB_HOVER,
                                 text_color=fg or SB_FG,
                                 font=ctk.CTkFont(size=14),
                                 cursor="hand2")

        def _sb_group(parent, text, items, capture_to=None):
            sub = ctk.CTkFrame(parent, fg_color="transparent")
            btn = _sb_action(parent, text + "   ▸", None)
            st = {"open": False}

            def toggle():
                if st["open"]:
                    sub.pack_forget()
                    btn.configure(text=text + "   ▸")
                    st["open"] = False
                else:
                    sub.pack(after=btn, fill="x")
                    btn.configure(text=text + "   ▾")
                    st["open"] = True

            btn.configure(command=toggle)
            for label, cmd in items:
                b = ctk.CTkButton(
                    sub, text=label, command=_wrap(cmd), anchor="w", height=32,
                    fg_color=SB_BG, text_color=SB_SUB_FG,
                    hover_color=SB_HOVER, corner_radius=8,
                    font=ctk.CTkFont(size=13), cursor="hand2",
                )
                b.pack(fill="x", padx=(28, 10), pady=1)
                if capture_to is not None:
                    capture_to[label] = b
            return btn

        ctk.CTkFrame(sidebar, height=12, fg_color="transparent").pack(fill="x")

        _sb_group(sidebar, "🔧  Maintenance", [
            ("Activate",        self.activate_maintenance),
            ("Deactivate",      self.deactivate_maintenance),
            ("Sync Moto-Mate",  self.force_upload),
            ("Update Firmware", self.menu_ota),
            ("Settings",        self.open_settings),
            ("Open Serial Page", self._open_serial_page),
        ], capture_to=self._maint_buttons).pack(fill="x", padx=10, pady=2)

        _sb_group(sidebar, "📊  Reports", [
            ("Last Day of Rides", lambda: self.open_ride_picker()),
            ("Daily Report",      lambda: self.open_report("day")),
            ("Weekly Report",     lambda: self.open_report("week")),
        ]).pack(fill="x", padx=10, pady=2)

        _sb_action(sidebar, "🗺️  Ride Finder", _wrap(self.open_ride_finder)).pack(fill="x", padx=10, pady=2)
        _sb_action(sidebar, "🏍️  DynaRide",    _wrap(self.open_dynaride)).pack(fill="x", padx=10, pady=2)
        _sb_action(sidebar, "🔋  DynaBike",    _wrap(self.open_dynabike)).pack(fill="x", padx=10, pady=2)

        ctk.CTkFrame(sidebar, height=1, fg_color="#d8dde8").pack(fill="x", padx=16, pady=18)

        _sb_action(sidebar, "✕  Quit",       _wrap(self.on_quit),
                   fg=("#d20f39", "#e64553"),
                   hover=("#f0d0d4", "#3a1a20")).pack(fill="x", padx=10, pady=2)

        # click anywhere outside the sidebar closes it; Escape too
        root.bind("<Button-1>", self._sidebar_outside_click, add="+")
        root.bind("<Escape>",   lambda e: self._sidebar_close())

        ctk.CTkLabel(body, text="🏍️  Moto-Mate",
                     font=ctk.CTkFont(size=22, weight="bold"),
                     text_color=("#1e66f5", "#89b4fa")).pack(pady=(8, 2))
        self.weather_label = ctk.CTkLabel(body, text="OSA commissioning · no live data",
                                          font=ctk.CTkFont(size=18, weight="bold"))
        self.weather_label.pack(pady=(2, 10))
        bc = tk.Frame(body, bg=CARD); bc.pack(fill="x", padx=20, pady=6)
        br = tk.Frame(bc, bg=CARD); br.pack(fill="x", pady=10)
        self.acc_lbl = self._col(br, "ACC", "--")
        self.eng_lbl = self._col(br, "Engine", "--")
        self.warn_lbl = self._col(br, "Warn", "--")
        sf = tk.Frame(body, bg=CARD); sf.pack(fill="x", padx=20, pady=6)
        self.state_lbl = tk.Label(sf, text="NO DATA", bg=CARD, fg=BLUE,
                                  font=("Helvetica", 26, "bold"))
        self.state_lbl.pack(pady=(14, 0))
        # V5.43: wake mode indicator (BENCH/OFFPEAK/PEAK/DAY)
        self.wake_lbl = tk.Label(sf, text="", bg=CARD, fg=MUTED,
                                 font=("Helvetica", 11, "bold"))
        self.wake_lbl.pack(pady=(0, 8))
        self.maint_lbl = tk.Label(sf, text="", bg=CARD, fg=YELLOW,
                                  font=("Helvetica", 13, "bold"),
                                  padx=12, pady=4)
        self.maint_lbl.pack(pady=(0, 8))
        vc = tk.Frame(body, bg=CARD); vc.pack(fill="x", padx=20, pady=6)
        self.volt_lbl = tk.Label(vc, text="--.-- V", bg=CARD, fg=GREEN,
                                 font=("Helvetica", 46, "bold"))
        self.volt_lbl.pack(pady=14)
        rc = tk.Frame(body, bg=CARD); rc.pack(fill="x", padx=20, pady=6)
        ri = tk.Frame(rc, bg=CARD); ri.pack(fill="x", pady=6)
        self.temp_lbl = self._col(ri, "🏍️ Temp", "--.-C")
        self.stor_lbl = self._col(ri, "💾 Stor", "--%")
        self.time_lbl = self._col(ri, "Time", "--:--")
        vgc = tk.Frame(body, bg=CARD); vgc.pack(fill="x", padx=20, pady=6)
        self.volt_graph = Graph(vgc, self, color_key="green", y_min=11.5, y_max=15.0)
        self.volt_graph.pack(padx=8, pady=8)
        tgc = tk.Frame(body, bg=CARD); tgc.pack(fill="x", padx=20, pady=6)
        self.temp_graph = Graph(tgc, self, color_key="orange", y_min=15.0, y_max=40.0)
        self.temp_graph.pack(padx=8, pady=8)
        self.footer_lbl = tk.Label(root, text=f"GUI v{GUI_VERSION}",
                                   bg=BG, fg=MUTED,
                                   font=("Helvetica", 9))
        self.footer_lbl.pack(side="bottom", pady=6)
        root.protocol("WM_DELETE_WINDOW", self.on_quit)
        self.worker.start()
        self.apply_theme()
        self.tick()

    # ---- V5.47: slide sidebar animation ----
    _SIDEBAR_STEP_MS = 12
    _SIDEBAR_EASE = 0.28

    def _sidebar_outside_click(self, event):
        # Any click that lands outside the sidebar closes it.
        if not self._sidebar_open:
            return
        sx = self._sidebar.winfo_rootx()
        sw = self._sidebar.winfo_width()
        if event.x_root < sx or event.x_root > sx + sw:
            self._sidebar_close()

    def _sidebar_toggle(self):
        if self._sidebar_open:
            self._sidebar_close()
        else:
            self._sidebar_open_now()

    def _sidebar_open_now(self):
        self._sidebar_open = True
        self._sidebar.lift()
        self._sidebar_animate(0)

    def _sidebar_close(self):
        if self._sidebar_open:
            self._sidebar_open = False
            self._sidebar_animate(-self._sidebar_w)

    def _sidebar_animate(self, target):
        if self._sidebar_job:
            self.root.after_cancel(self._sidebar_job)
            self._sidebar_job = None
        self._sidebar_step(target)

    def _sidebar_step(self, target):
        dist = target - self._sidebar_x
        if abs(dist) < 1:
            self._sidebar_x = target
            self._sidebar.place(x=int(self._sidebar_x), y=0, relheight=1)
            self._sidebar_job = None
            return
        self._sidebar_x += dist * self._SIDEBAR_EASE
        self._sidebar.place(x=int(self._sidebar_x), y=0, relheight=1)
        self._sidebar_job = self.root.after(
            self._SIDEBAR_STEP_MS, lambda: self._sidebar_step(target))

    def _col(self, parent, label, value):
        c = tk.Frame(parent, bg=CARD); c.pack(side="left", expand=True, fill="x")
        tk.Label(c, text=label, bg=CARD, fg=MUTED,
                 font=("Helvetica", 11)).pack(pady=(4, 2))
        v = tk.Label(c, text=value, bg=CARD, fg=FG,
                     font=("Helvetica", 16, "bold"))
        v.pack(pady=(0, 4))
        return v

    def apply_theme(self):
        t = self.theme
        # V4.34: legacy colour aliases (BG, FG, GREEN, ...) are
        # snapshotted into every module via `from .config import *`.
        # Rebinding them in config.py alone is not enough - we have to
        # re-set each name in each module's namespace or widgets keep
        # drawing with the night palette after the theme flips.
        _alias = {
            "BG": t["bg"], "CARD": t["card"], "GRID": t["grid"],
            "FG": t["fg"], "BLUE": t["blue"], "GREEN": t["green"],
            "YELLOW": t["yellow"], "ORANGE": t["orange"],
            "MUTED": t["muted"], "RED": t["red"],
            "FILL_GREEN": t["fill"], "FILL_BLUE": t["fill"],
        }
        for _modname in ("motomate.app", "motomate.widgets", "motomate.reports",
                         "motomate.helpers", "motomate.ble", "motomate.ota",
                         "motomate.drive", "motomate.weather"):
            _mod = sys.modules.get(_modname)
            if _mod is None:
                continue
            for _k, _v in _alias.items():
                if hasattr(_mod, _k):
                    setattr(_mod, _k, _v)
        self.root.configure(bg=t["bg"])

        # Widgets whose bg/fg are driven by special logic, not just theme
        special_fg = {
            self.acc_lbl:    ("green" if state.latest_data.get("a") else "muted"),
            self.eng_lbl:    ("green" if state.latest_data.get("e") else "muted"),
            self.warn_lbl:   ("red"   if state.latest_data.get("w") else "muted"),
            self.volt_lbl:   None,  # set by tick()
            self.state_lbl:  None,  # set by tick()
            self.temp_lbl:   None,  # set by tick()
        }

        def walk(w):
            try:
                cls = w.winfo_class()
            except Exception:
                return
            try:
                if cls == "Frame":
                    w.configure(bg=t["card"] if w is not self.root else t["bg"])
                elif cls == "Label":
                    # Which frame is this label in? card or bg?
                    parent_bg = t["bg"]
                    try:
                        if w.master.winfo_class() == "Frame" and w.master is not self.root:
                            parent_bg = t["card"]
                    except Exception:
                        pass
                    w.configure(bg=parent_bg)
                    # Don't override fg for special labels
                    if w in special_fg:
                        key = special_fg[w]
                        if key is not None:
                            w.configure(fg=t[key])
                    else:
                        # heuristic: muted for the small column headers,
                        # fg for values. Check font size.
                        try:
                            font = w.cget("font")
                            fs = int(str(font).split()[-1].rstrip(")")) if "bold" not in str(font) else 0
                        except Exception:
                            fs = 0
                        w.configure(fg=t["fg"] if fs == 0 or fs >= 12 else t["muted"])
                elif cls == "Button":
                    w.configure(bg=t["card"], fg=t["fg"],
                                activebackground=t["accent"],
                                activeforeground=t["fg"])
                elif cls == "Entry":
                    w.configure(bg=t["card"], fg=t["fg"],
                                insertbackground=t["fg"])
                elif cls == "Menu":
                    pass  # menu colours are macOS-native
            except Exception:
                pass
            for c in w.winfo_children():
                walk(c)

        walk(self.root)

        # Graphs need explicit apply_theme
        try:
            self.volt_graph.apply_theme()
        except Exception:
            pass
        try:
            self.temp_graph.apply_theme()
        except Exception:
            pass

    def open_report(self, *args, **kwargs):
        self._unavailable("Reports")

    def do_not_press(self, *args, **kwargs):
        self._unavailable("Collection")

    def on_quit(self):
        self.worker.running = False
        self.root.destroy()

    def open_drive(self, *args, **kwargs):
        self._unavailable("Archive")

    def open_local_drive(self, *args, **kwargs):
        self._unavailable("Archive")

    def open_reports(self, *args, **kwargs):
        self._unavailable("Reports")

    def menu_sync(self, *args, **kwargs):
        self._unavailable("Archive sync")

    def _auto_close_dialog(self, title, message, seconds=2):
        dlg = tk.Toplevel(self.root)
        dlg.title(title)
        dlg.configure(bg=self.theme["bg"])
        dlg.resizable(False, False)
        dlg.transient(self.root)
        tk.Label(dlg, text=message, bg=self.theme["bg"], fg=self.theme["fg"],
                 font=("Helvetica", 11), padx=20, pady=20,
                 justify="left").pack()
        dlg.update_idletasks()
        w = dlg.winfo_width()
        h = dlg.winfo_height()
        x = self.root.winfo_rootx() + (self.root.winfo_width() - w) // 2
        y = self.root.winfo_rooty() + (self.root.winfo_height() - h) // 2
        dlg.geometry(f"+{x}+{y}")
        dlg.after(seconds * 1000, dlg.destroy)

    def menu_ota(self, *args, **kwargs):
        self._unavailable("Firmware update")

    def _ota_worker(self, *args, **kwargs):
        self._unavailable("Firmware update")

    def _update_maint_menu(self):
        for btn in self._maint_buttons.values():
            btn.configure(state="normal")

    def _unavailable(self, feature):
        messagebox.showinfo(feature, "Not available in this commissioning GUI. Moto-Mate integration is pending.", parent=self.root)

    def activate_maintenance(self, *args, **kwargs):
        self._unavailable("Maintenance")

    def deactivate_maintenance(self, *args, **kwargs):
        self._unavailable("Maintenance")

    def _start_maint_off_poll(self, *args, **kwargs):
        self._unavailable("Maintenance")

    def _send_maint_command(self, *args, **kwargs):
        self._unavailable("Maintenance")

    def _start_maint_poll(self, *args, **kwargs):
        self._unavailable("Maintenance")

    def _check_maint_on_startup(self, *args, **kwargs):
        self._unavailable("Maintenance")

    def _open_serial_page(self, *args, **kwargs):
        self._unavailable("Serial page")

    def _maint_ack_timeout(self, *args, **kwargs):
        self._unavailable("Maintenance")

    def open_settings(self, *args, **kwargs):
        self._unavailable("Settings")

    def _show_settings_dialog(self, *args, **kwargs):
        self._unavailable("Settings")

    def open_ride_picker(self, *args, **kwargs):
        self._unavailable("Last Day of Rides")

    def open_ride_finder(self, *args, **kwargs):
        self._unavailable("Ride Finder")

    def open_dynaride(self):
        from .dynatune_window import DynaTuneWindow
        checks = ("charge_delta", "crank_sag", "sag_recovery", "charge_high",
                  "charge_stability", "under_duration", "over_duration")
        DynaTuneWindow(self.root, self, {"ride": {
            key: {"status": "IDLE", "metric": "—", "detail": "No qualified Moto-Mate log evidence"}
            for key in checks}}, {"runway_note": "Analysis pending; no health assessment."})

    def open_dynabike(self):
        from .dynatune_window import DynaTuneWindow
        groups = {"data": ("clock_validity", "upload_success", "gps_fix_rate"),
                  "device": ("panic_count", "storage_usage"),
                  "hardware": ("sht40_plausible", "divider_plausible"),
                  "config": ("run_order", "monitor_order", "sag_floor")}
        DynaTuneWindow(self.root, self, {
            group: {key: {"status": "IDLE", "metric": "—",
                         "detail": "No qualified Moto-Mate log evidence"}
                    for key in keys} for group, keys in groups.items()},
            {"runway_note": "Analysis pending; no health assessment."})

    def open_dynatune(self):
        self.open_dynaride()

    def force_upload(self, *args, **kwargs):
        self._unavailable("Collection")

    def tick(self):
        # Commissioning status only; never turn a host response into a sample.
        self.state_lbl.config(text="NO DEVICE DATA", fg=MUTED)
        self.wake_lbl.config(text="COMMISSIONING", fg=MUTED)
        self.volt_lbl.config(text="--.-- V", fg=MUTED)
        self.temp_lbl.config(text="--.-C", fg=MUTED)
        self.stor_lbl.config(text="--%", fg=MUTED)
        self.acc_lbl.config(text="--", fg=MUTED)
        self.eng_lbl.config(text="--", fg=MUTED)
        self.warn_lbl.config(text="--", fg=MUTED)
        self.time_lbl.config(text=f"{time_emoji()} {local_time_str()}")
        self.weather_label.configure(text=self.worker.status_text)
        self.footer_lbl.config(text=f"GUI v{GUI_VERSION}  ·  FW unknown")
        self.volt_graph.set_data(state.volt_hist)
        self.temp_graph.set_data(state.temp_hist)
        self._update_maint_menu()
        self.root.after(1000, self.tick)

