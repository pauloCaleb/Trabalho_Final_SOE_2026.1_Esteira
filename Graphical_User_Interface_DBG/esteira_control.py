"""
╔══════════════════════════════════════════════════════════╗
║     ESTEIRA SEPARADORA — PAINEL DE CONTROLE UART         ║
║     STM32G070 ↔ Raspberry Pi 3B   — GUI v2              ║
║     Firmware v2.1 — SOE 2026.1                           ║
╚══════════════════════════════════════════════════════════╝
Dependências:
    pip install pyserial        (ou: sudo apt install python3-serial)

Melhorias v2:
    - Fila TX dedicada: TX e RX rodam em threads separadas,
      sem lock compartilhado → latência mínima em todos os comandos
    - Loop de passos reduzido para 60 ms (≈16 cmd/s)
    - Tema claro / escuro alternável em tempo real
"""

import tkinter as tk
from tkinter import ttk, font as tkfont, messagebox
import serial
import serial.tools.list_ports
import threading
import queue
import time
from datetime import datetime

# ─── PROTOCOLO ────────────────────────────────────────────────────────────────
START_FRAME     = 0xAA
CMD_OK          = 0x90
CMD_ERR         = 0x91
SYS_RDY_MSG     = 0x10
SYS_INIT_MSG    = 0x01
ROUTE_A_RECV    = 0xDA
ROUTE_B_RECV    = 0xDB
OBJ_DETECTED    = 0xA0
CLSS_REQUEST    = 0xC0
ROUTE_A_FWD     = 0xFA
ROUTE_B_FWD     = 0xFB
ROUTE_A_OK      = 0xBA
ROUTE_B_OK      = 0xBB
LIGHT_EN        = 0xE1
LIGHT_DIS       = 0xD1
GATE_OPEN       = 0xE2
GATE_CLOSE      = 0xD2
STPR_EN         = 0xE3
STPR_DIS        = 0xD3
STPR_FORWARD    = 0xE4
STPR_BACKWARD   = 0xD4
STPR_TGT_STPS   = 0xE5
DEBUG_TOGGLE    = 0xDD

RX_NAMES = {
    CMD_OK:        "CMD_OK",        CMD_ERR:       "CMD_ERR",
    OBJ_DETECTED:  "OBJ_DETECTED",  CLSS_REQUEST:  "CLSS_REQUEST",
    ROUTE_A_FWD:   "ROUTE_A_FWD",   ROUTE_B_FWD:   "ROUTE_B_FWD",
    ROUTE_A_OK:    "ROUTE_A_OK",    ROUTE_B_OK:    "ROUTE_B_OK",
    SYS_INIT_MSG:  "SYS_INIT",
    LIGHT_EN:      "LIGHT_EN (eco)",     LIGHT_DIS:     "LIGHT_DIS (eco)",
    GATE_OPEN:     "GATE_OPEN (eco)",    GATE_CLOSE:    "GATE_CLOSE (eco)",
    STPR_EN:       "STPR_EN (eco)",      STPR_DIS:      "STPR_DIS (eco)",
    STPR_FORWARD:  "STPR_FORWARD (eco)", STPR_BACKWARD: "STPR_BACKWARD (eco)",
    STPR_TGT_STPS: "STPR_TGT_STPS (eco)",
    DEBUG_TOGGLE:  "DEBUG_TOGGLE (eco)",
    ROUTE_A_RECV:  "ROUTE_A (eco)",      ROUTE_B_RECV:  "ROUTE_B (eco)",
}
TX_NAMES = {
    LIGHT_EN: "LIGHT_EN",       LIGHT_DIS: "LIGHT_DIS",
    GATE_OPEN: "GATE_OPEN",     GATE_CLOSE: "GATE_CLOSE",
    STPR_EN: "STPR_EN",         STPR_DIS: "STPR_DIS",
    STPR_FORWARD: "STPR_FORWARD", STPR_BACKWARD: "STPR_BACKWARD",
    STPR_TGT_STPS: "STPR_TGT_STPS", DEBUG_TOGGLE: "DEBUG_TOGGLE",
    ROUTE_A_RECV: "ROUTE_A",    ROUTE_B_RECV: "ROUTE_B",
    SYS_RDY_MSG: "SYS_RDY",
}

# ─── TEMAS ────────────────────────────────────────────────────────────────────
THEMES = {
    "dark": {
        "bg":                   "#0D0F14",
        "panel":                "#13161E",
        "border":               "#1E2330",
        "accent":               "#00D4FF",
        "accent2":              "#FF6B35",
        "green":                "#00D97A",
        "red":                  "#FF3B5C",
        "yellow":               "#FFD700",
        "text":                 "#C8D0E0",
        "text_dim":             "#4A5568",
        "text_head":            "#E8EDF5",
        "ok":                   "#00C87A",
        "err":                  "#FF3B5C",
        "log_tx":               "#38BDF8",
        "log_rx":               "#34D399",
        "log_sys":              "#FBBF24",
        "spinbox_bg":           "#1E2330",
        "state_box_active_fg":  "#0D0F14",
    },
    "light": {
        "bg":                   "#EEF1F7",
        "panel":                "#FFFFFF",
        "border":               "#C8D3E6",
        "accent":               "#0077AA",
        "accent2":              "#C04400",
        "green":                "#006B44",
        "red":                  "#BB1133",
        "yellow":               "#886600",
        "text":                 "#2D3748",
        "text_dim":             "#7A8899",
        "text_head":            "#111827",
        "ok":                   "#006B44",
        "err":                  "#BB1133",
        "log_tx":               "#005F99",
        "log_rx":               "#005533",
        "log_sys":              "#775500",
        "spinbox_bg":           "#E4EAF4",
        "state_box_active_fg":  "#FFFFFF",
    },
}

C = dict(THEMES["dark"])   # tema ativo global

# color_key → (nome exibido, color_key do tema)
FSM_STATE_DEFS = {
    0:   ("IDLE",                "text_dim"),
    1:   ("OBJECT DETECTED",     "yellow"),
    2:   ("WAIT CLASSIFICATION", "accent"),
    3:   ("ROUTE A",             "green"),
    4:   ("ROUTE B",             "accent2"),
    255: ("—",                   "text_dim"),
}

LOOP_INTERVAL = 0.060   # 60 ms → ≈16 comandos/s


# ─── SERIAL MANAGER ───────────────────────────────────────────────────────────
class SerialManager:
    """
    TX e RX correm em threads independentes.
    Thread TX drena uma queue.Queue sem disputar lock com thread RX.
    Elimina a contenção que causava latência variável nos comandos.
    """
    def __init__(self):
        self.ser        = None
        self.connected  = False
        self._tx_queue  = queue.Queue()
        self._callbacks = []
        self._running   = False
        self._port_lock = threading.Lock()

    def list_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port, baud=115200):
        with self._port_lock:
            try:
                self.ser = serial.Serial(port, baud, timeout=0.02,
                                          write_timeout=0.1)
                self.connected = True
                self._running  = True
                threading.Thread(target=self._rx_loop, daemon=True,
                                  name="serial-rx").start()
                threading.Thread(target=self._tx_loop, daemon=True,
                                  name="serial-tx").start()
                return True
            except Exception as e:
                self.connected = False
                return str(e)

    def disconnect(self):
        self._running = False
        self._tx_queue.put(None)          # desbloqueia thread TX
        with self._port_lock:
            try:
                if self.ser and self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.connected = False

    def send_frame(self, cmd, data=None):
        """Enfileira frame — não bloqueia a thread principal."""
        if not self.connected:
            return False
        frame = bytes([START_FRAME, cmd])
        if data is not None:
            frame += bytes([data])
        self._tx_queue.put(frame)
        return True

    def add_callback(self, cb):
        self._callbacks.append(cb)

    def _notify(self, event, data):
        for cb in self._callbacks:
            cb(event, data)

    def _tx_loop(self):
        while self._running:
            try:
                frame = self._tx_queue.get(timeout=0.5)
                if frame is None:
                    break
                with self._port_lock:
                    if self.ser and self.ser.is_open:
                        self.ser.write(frame)
            except queue.Empty:
                continue
            except Exception:
                self.connected = False
                self._notify("disconnected", None)
                break

    def _rx_loop(self):
        state       = "IDLE"
        status_byte = 0
        while self._running:
            try:
                with self._port_lock:
                    if not self.ser or not self.ser.is_open:
                        break
                    waiting = self.ser.in_waiting
                raw = self.ser.read(waiting or 1) if waiting >= 0 else b""
            except Exception:
                break
            for b in raw:
                if state == "IDLE":
                    if b in (CMD_OK, CMD_ERR):
                        status_byte = b
                        state = "WAIT_PAYLOAD"
                elif state == "WAIT_PAYLOAD":
                    self._notify("rx", (status_byte, b))
                    state = "IDLE"
                    status_byte = 0
        self._notify("disconnected", None)


# ─── APLICAÇÃO ────────────────────────────────────────────────────────────────
class EsteiraApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ESTEIRA SEPARADORA — Painel de Controle")
        self.resizable(True, True)
        self.minsize(920, 660)

        self._theme_name = "dark"
        self.serial = SerialManager()
        self.serial.add_callback(self._on_serial_event)

        # Variáveis de estado
        self._op_mode    = tk.StringVar(value="FSM")
        self._fsm_state  = tk.IntVar(value=255)
        self._gate_state = tk.StringVar(value="FECHADA")
        self._motor_state= tk.StringVar(value="PARADO")
        self._motor_dir  = tk.StringVar(value="FRENTE")
        self._flash_state= tk.StringVar(value="OFF")
        self._step_dir   = tk.IntVar(value=0)
        self._step_count = tk.IntVar(value=100)
        self._loop_mode  = tk.BooleanVar(value=False)

        self._step_loop_running = False
        self._step_loop_thread  = None

        # Listas para recoloração de tema
        self._tw_list   = []   # (widget, {prop: color_key})
        self._btn_list  = []   # (button, color_key)
        self._state_boxes = {}
        self._checkbuttons = []

        self._build_fonts()
        self.configure(bg=C["bg"])
        self._build_ui()
        self._refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ─── Fontes ───────────────────────────────────────────────────────────────
    def _build_fonts(self):
        self.f_mono   = tkfont.Font(family="Courier New", size=9)
        self.f_mono_b = tkfont.Font(family="Courier New", size=9,  weight="bold")
        self.f_label  = tkfont.Font(family="Courier New", size=8)
        self.f_title  = tkfont.Font(family="Courier New", size=11, weight="bold")
        self.f_state  = tkfont.Font(family="Courier New", size=14, weight="bold")
        self.f_btn    = tkfont.Font(family="Courier New", size=9,  weight="bold")
        self.f_head   = tkfont.Font(family="Courier New", size=10, weight="bold")

    # ─── Tema ─────────────────────────────────────────────────────────────────
    def _toggle_theme(self):
        global C
        self._theme_name = "light" if self._theme_name == "dark" else "dark"
        C = dict(THEMES[self._theme_name])
        self.configure(bg=C["bg"])
        self._recolor_all()
        icon = "☀" if self._theme_name == "dark" else "◑"
        self._btn_theme.config(text=icon, fg=C["text_dim"],
                                bg=C["border"], activebackground=C["bg"],
                                activeforeground=C["text_dim"])
        self._update_status_colors()
        self._refresh_fsm_display(self._fsm_state.get())
        self._apply_log_tags()

    def _recolor_all(self):
        # ttk styles
        style = ttk.Style()
        style.configure("TCombobox",
                         fieldbackground=C["border"], background=C["border"],
                         foreground=C["text_head"], selectbackground=C["border"],
                         selectforeground=C["accent"])
        style.configure("Vertical.TScrollbar",
                         background=C["border"], troughcolor=C["bg"],
                         arrowcolor=C["text_dim"])
        style.configure("Horizontal.TScrollbar",
                         background=C["border"], troughcolor=C["bg"],
                         arrowcolor=C["text_dim"])
        # Widgets genéricos
        for widget, props in self._tw_list:
            try:
                widget.config(**{k: C[v] for k, v in props.items()})
            except tk.TclError:
                pass
        # Botões
        for btn, ck in self._btn_list:
            try:
                btn.config(bg=C["border"], fg=C[ck],
                           activebackground=C["bg"], activeforeground=C[ck])
            except tk.TclError:
                pass
        # Checkbuttons
        for cb in self._checkbuttons:
            try:
                cb.config(bg=C["panel"], fg=C["text"],
                          selectcolor=C["border"],
                          activebackground=C["panel"],
                          activeforeground=C["text"])
            except tk.TclError:
                pass

    def _tw(self, widget, **props):
        """Registra widget para recoloração automática."""
        self._tw_list.append((widget, props))
        return widget

    def _apply_log_tags(self):
        self._log_text.tag_config("tx",  foreground=C["log_tx"])
        self._log_text.tag_config("rx",  foreground=C["log_rx"])
        self._log_text.tag_config("sys", foreground=C["log_sys"])
        self._log_text.tag_config("err", foreground=C["err"])
        self._log_text.config(bg=C["bg"], fg=C["text"],
                               insertbackground=C["accent"])

    # ─── UI raiz ──────────────────────────────────────────────────────────────
    def _build_ui(self):
        # Header
        hdr = self._tw(tk.Frame(self, pady=8), bg="bg")
        hdr.pack(fill="x", padx=12, pady=(10, 0))
        self._tw(tk.Label(hdr, text="▸ ESTEIRA SEPARADORA", font=self.f_title),
                 bg="bg", fg="accent").pack(side="left")
        self._tw(tk.Label(hdr, text="STM32G070 — FIRMWARE v2.1", font=self.f_label),
                 bg="bg", fg="text_dim").pack(side="left", padx=16)

        # Botão de tema
        self._btn_theme = self._btn(hdr, "☀", self._toggle_theme,
                                     color="text_dim", w=3)
        self._btn_theme.pack(side="right", padx=(4, 0))
        self._tw(tk.Label(hdr, text="TEMA", font=self.f_label),
                 bg="bg", fg="text_dim").pack(side="right")

        # Conexão
        conn_p = self._panel(self, label="CONEXÃO SERIAL")
        conn_p.pack(fill="x", padx=12, pady=6)
        self._build_connection_bar(conn_p)

        # Corpo 2 colunas
        body = self._tw(tk.Frame(self), bg="bg")
        body.pack(fill="both", expand=True, padx=12, pady=4)
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=2)
        body.rowconfigure(0, weight=1)

        left = self._tw(tk.Frame(body), bg="bg")
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 6))
        self._build_status_panel(left)
        self._build_fsm_panel(left)
        self._build_fsm_controls(left)

        right = self._tw(tk.Frame(body), bg="bg")
        right.grid(row=0, column=1, sticky="nsew")
        self._build_debug_panel(right)
        self._build_log_panel(right)

    # ─── Conexão ──────────────────────────────────────────────────────────────
    def _build_connection_bar(self, parent):
        inner = self._tw(tk.Frame(parent), bg="panel")
        inner.pack(fill="x", padx=6, pady=6)

        self._tw(tk.Label(inner, text="PORTA:", font=self.f_label),
                 bg="panel", fg="text_dim").pack(side="left", padx=(6, 2))
        self._port_var = tk.StringVar()
        self._port_cb  = ttk.Combobox(inner, textvariable=self._port_var,
                                       width=18, state="readonly")
        self._port_cb.pack(side="left", padx=4)
        self._style_combobox()

        self._tw(tk.Label(inner, text="BAUD:", font=self.f_label),
                 bg="panel", fg="text_dim").pack(side="left", padx=(8, 2))
        self._baud_var = tk.StringVar(value="115200")
        ttk.Combobox(inner, textvariable=self._baud_var,
                     values=["9600","19200","57600","115200","230400"],
                     width=8, state="readonly").pack(side="left", padx=4)

        self._btn(inner, "⟳", self._refresh_ports,
                  color="text_dim", w=3).pack(side="left", padx=4)

        self._btn_connect = self._btn(inner, "CONECTAR", self._toggle_connect,
                                       color="green", w=12)
        self._btn_connect.pack(side="left", padx=8)

        self._conn_led = self._tw(tk.Label(inner, text="●", font=self.f_head),
                                   bg="panel", fg="text_dim")
        self._conn_led.pack(side="left", padx=4)
        self._conn_label = self._tw(tk.Label(inner, text="DESCONECTADO",
                                              font=self.f_label),
                                     bg="panel", fg="text_dim")
        self._conn_label.pack(side="left")

        self._btn_handshake = self._btn(inner, "HANDSHAKE", self._do_handshake,
                                         color="accent", w=12)
        self._btn_handshake.pack(side="right", padx=8)
        self._btn_handshake.config(state="disabled")

    # ─── Status ───────────────────────────────────────────────────────────────
    def _build_status_panel(self, parent):
        p = self._panel(parent, label="STATUS DO HARDWARE")
        p.pack(fill="x", pady=(0, 6))
        inner = self._tw(tk.Frame(p), bg="panel")
        inner.pack(fill="x", padx=6, pady=6)

        rows = [
            ("MODO",    self._op_mode),
            ("CANCELA", self._gate_state),
            ("MOTOR",   self._motor_state),
            ("DIREÇÃO", self._motor_dir),
            ("FLASH",   self._flash_state),
        ]
        self._status_labels = {}
        for i, (name, var) in enumerate(rows):
            self._tw(tk.Label(inner, text=f"{name}:", font=self.f_label,
                               anchor="w", width=10),
                     bg="panel", fg="text_dim").grid(
                row=i, column=0, sticky="w", padx=6, pady=2)
            lbl = self._tw(tk.Label(inner, textvariable=var,
                                     font=self.f_mono_b, anchor="w"),
                           bg="panel", fg="accent")
            lbl.grid(row=i, column=1, sticky="w", padx=4, pady=2)
            self._status_labels[name] = lbl
        self._update_status_colors()

    def _update_status_colors(self):
        self._status_labels["MODO"].config(
            fg=C["accent"] if self._op_mode.get() == "FSM" else C["accent2"])
        self._status_labels["CANCELA"].config(
            fg=C["green"] if self._gate_state.get() == "ABERTA" else C["red"])
        self._status_labels["MOTOR"].config(
            fg=C["green"] if self._motor_state.get() == "GIRANDO" else C["text_dim"])
        self._status_labels["FLASH"].config(
            fg=C["yellow"] if self._flash_state.get() == "ON" else C["text_dim"])
        self._status_labels["DIREÇÃO"].config(fg=C["text"])

    # ─── FSM ──────────────────────────────────────────────────────────────────
    def _build_fsm_panel(self, parent):
        p = self._panel(parent, label="MÁQUINA DE ESTADOS")
        p.pack(fill="x", pady=(0, 6))
        inner = self._tw(tk.Frame(p), bg="panel")
        inner.pack(fill="x", padx=6, pady=10)

        self._fsm_state_label = self._tw(
            tk.Label(inner, text="—", font=self.f_state, anchor="center"),
            bg="panel", fg="text_dim")
        self._fsm_state_label.pack(fill="x")

        pipe = self._tw(tk.Frame(inner), bg="panel")
        pipe.pack(fill="x", pady=(8, 0))

        order = [0, 1, 2, 3, 4]
        names = ["IDLE", "OBJ\nDET", "WAIT\nCLSS", "ROUTE\nA", "ROUTE\nB"]
        for i, (sid, sname) in enumerate(zip(order, names)):
            col = self._tw(tk.Frame(pipe), bg="panel")
            col.pack(side="left", expand=True, fill="x")
            box = tk.Label(col, text=sname, bg=C["border"], fg=C["text_dim"],
                            font=self.f_label, relief="flat",
                            padx=4, pady=4, justify="center")
            box.pack(fill="x", padx=2)
            self._state_boxes[sid] = box
            if i < len(order) - 1:
                self._tw(tk.Label(pipe, text="▸", font=self.f_label),
                         bg="panel", fg="text_dim").pack(side="left")

    def _refresh_fsm_display(self, state_id):
        name, ck = FSM_STATE_DEFS.get(state_id, ("?", "text_dim"))
        self._fsm_state_label.config(text=name, fg=C[ck])
        for sid, box in self._state_boxes.items():
            if sid == state_id:
                _, bck = FSM_STATE_DEFS.get(sid, ("?", "text_dim"))
                box.config(bg=C[bck], fg=C["state_box_active_fg"])
            else:
                box.config(bg=C["border"], fg=C["text_dim"])

    # ─── Controles FSM ────────────────────────────────────────────────────────
    def _build_fsm_controls(self, parent):
        p = self._panel(parent, label="COMANDOS FSM")
        p.pack(fill="x", pady=(0, 6))
        inner = self._tw(tk.Frame(p), bg="panel")
        inner.pack(fill="x", padx=6, pady=6)

        r1 = self._tw(tk.Frame(inner), bg="panel")
        r1.pack(fill="x", pady=2)
        self._btn(r1, "→ ROTA A", lambda: self._send(ROUTE_A_RECV),
                  color="green", w=14).pack(side="left", padx=4)
        self._btn(r1, "→ ROTA B", lambda: self._send(ROUTE_B_RECV),
                  color="accent2", w=14).pack(side="left", padx=4)

        r2 = self._tw(tk.Frame(inner), bg="panel")
        r2.pack(fill="x", pady=2)
        self._btn(r2, "⇄ TOGGLE DEBUG", self._toggle_debug,
                  color="yellow", w=30).pack(side="left", padx=4)

    # ─── Debug Panel ──────────────────────────────────────────────────────────
    def _build_debug_panel(self, parent):
        p = self._panel(parent, label="CONTROLE ASSÍNCRONO (MODO DEBUG)")
        p.pack(fill="x", pady=(0, 6))
        inner = self._tw(tk.Frame(p), bg="panel")
        inner.pack(fill="x", padx=6, pady=6)

        # Flash + Cancela
        r1 = self._tw(tk.Frame(inner), bg="panel")
        r1.pack(fill="x", pady=3)
        self._tw(tk.Label(r1, text="FLASH:", font=self.f_label, width=8),
                 bg="panel", fg="text_dim").pack(side="left")
        self._btn(r1, "ON",  lambda: self._send(LIGHT_EN),
                  color="yellow", w=6).pack(side="left", padx=3)
        self._btn(r1, "OFF", lambda: self._send(LIGHT_DIS),
                  color="text_dim", w=6).pack(side="left", padx=3)
        self._tw(tk.Label(r1, text="CANCELA:", font=self.f_label, width=9),
                 bg="panel", fg="text_dim").pack(side="left", padx=(16, 0))
        self._btn(r1, "ABRIR",  lambda: self._send(GATE_OPEN),
                  color="green", w=7).pack(side="left", padx=3)
        self._btn(r1, "FECHAR", lambda: self._send(GATE_CLOSE),
                  color="red",   w=7).pack(side="left", padx=3)

        # Motor enable
        r2 = self._tw(tk.Frame(inner), bg="panel")
        r2.pack(fill="x", pady=3)
        self._tw(tk.Label(r2, text="MOTOR:", font=self.f_label, width=8),
                 bg="panel", fg="text_dim").pack(side="left")
        self._btn(r2, "ENGAJAR", lambda: self._send(STPR_EN),
                  color="green", w=10).pack(side="left", padx=3)
        self._btn(r2, "LIVRE",   lambda: self._send(STPR_DIS),
                  color="red",   w=10).pack(side="left", padx=3)

        # Direção
        r3 = self._tw(tk.Frame(inner), bg="panel")
        r3.pack(fill="x", pady=3)
        self._tw(tk.Label(r3, text="DIREÇÃO:", font=self.f_label, width=8),
                 bg="panel", fg="text_dim").pack(side="left")

        def set_fwd():
            self._step_dir.set(0); self._motor_dir.set("FRENTE")
            self._send(STPR_FORWARD); self._update_status_colors()

        def set_bwd():
            self._step_dir.set(1); self._motor_dir.set("TRÁS")
            self._send(STPR_BACKWARD); self._update_status_colors()

        self._btn(r3, "◀ FRENTE", set_fwd, color="accent", w=10).pack(side="left", padx=3)
        self._btn(r3, "TRÁS ▶",   set_bwd, color="accent", w=10).pack(side="left", padx=3)

        # Separador
        self._tw(tk.Frame(inner, height=1), bg="border").pack(fill="x", pady=6)

        # Bloco de passos
        self._tw(tk.Label(inner, text="CONTROLE DE PASSOS", font=self.f_label),
                 bg="panel", fg="accent").pack(anchor="w")

        r4 = self._tw(tk.Frame(inner), bg="panel")
        r4.pack(fill="x", pady=4)
        self._tw(tk.Label(r4, text="Nº DE PASSOS:", font=self.f_label),
                 bg="panel", fg="text_dim").pack(side="left", padx=(0, 6))

        self._step_spinbox = tk.Spinbox(
            r4, from_=1, to=255, textvariable=self._step_count,
            width=6, font=self.f_mono,
            bg=C["spinbox_bg"], fg=C["text_head"],
            buttonbackground=C["border"], relief="flat",
            insertbackground=C["accent"])
        self._step_spinbox.pack(side="left", padx=4)
        self._tw_list.append((self._step_spinbox,
                               {"bg": "spinbox_bg", "fg": "text_head",
                                "buttonbackground": "border",
                                "insertbackground": "accent"}))
        self._tw(tk.Label(r4, text="(1–255)", font=self.f_label),
                 bg="panel", fg="text_dim").pack(side="left")

        r5 = self._tw(tk.Frame(inner), bg="panel")
        r5.pack(fill="x", pady=3)
        cb = tk.Checkbutton(r5, text="MODO LOOP  (rebobina steps continuamente)",
                             variable=self._loop_mode,
                             bg=C["panel"], fg=C["text"],
                             selectcolor=C["border"],
                             activebackground=C["panel"],
                             activeforeground=C["text"],
                             font=self.f_label,
                             command=self._on_loop_toggle)
        cb.pack(side="left")
        self._checkbuttons.append(cb)

        r6 = self._tw(tk.Frame(inner), bg="panel")
        r6.pack(fill="x", pady=3)
        self._btn_send_steps = self._btn(r6, "▶ ENVIAR PASSOS",
                                          self._send_steps, color="green", w=18)
        self._btn_send_steps.pack(side="left", padx=3)
        self._btn_stop_loop = self._btn(r6, "■ PARAR LOOP",
                                         self._stop_loop, color="red", w=14)
        self._btn_stop_loop.pack(side="left", padx=3)
        self._btn_stop_loop.config(state="disabled")

        self._loop_status = self._tw(tk.Label(inner, text="", font=self.f_label),
                                      bg="panel", fg="yellow")
        self._loop_status.pack(anchor="w", pady=2)

    # ─── Log ──────────────────────────────────────────────────────────────────
    def _build_log_panel(self, parent):
        p = self._panel(parent, label="LOG DE COMUNICAÇÃO")
        p.pack(fill="both", expand=True)
        inner = self._tw(tk.Frame(p), bg="panel")
        inner.pack(fill="both", expand=True, padx=6, pady=6)
        inner.rowconfigure(0, weight=1)
        inner.columnconfigure(0, weight=1)

        self._log_text = tk.Text(inner, bg=C["bg"], fg=C["text"],
                                  font=self.f_mono, relief="flat",
                                  state="disabled", wrap="none",
                                  insertbackground=C["accent"])
        self._log_text.grid(row=0, column=0, sticky="nsew")

        sb_y = ttk.Scrollbar(inner, orient="vertical",   command=self._log_text.yview)
        sb_y.grid(row=0, column=1, sticky="ns")
        sb_x = ttk.Scrollbar(inner, orient="horizontal", command=self._log_text.xview)
        sb_x.grid(row=1, column=0, sticky="ew")
        self._log_text.config(yscrollcommand=sb_y.set, xscrollcommand=sb_x.set)
        self._apply_log_tags()

        ctrl = self._tw(tk.Frame(p), bg="panel")
        ctrl.pack(fill="x", padx=6, pady=(0, 4))
        self._btn(ctrl, "LIMPAR LOG", self._clear_log,
                  color="text_dim", w=12).pack(side="right")

    # ─── Helpers ──────────────────────────────────────────────────────────────
    def _panel(self, parent, label=""):
        frame = tk.Frame(parent, bg=C["panel"], bd=1, relief="flat",
                          highlightbackground=C["border"], highlightthickness=1)
        self._tw_list.append((frame, {"bg": "panel",
                                       "highlightbackground": "border"}))
        if label:
            lbl = tk.Label(frame, text=f" {label} ", bg=C["border"],
                            fg=C["text_dim"], font=self.f_label, padx=6, pady=2)
            lbl.pack(anchor="nw", fill="x")
            self._tw_list.append((lbl, {"bg": "border", "fg": "text_dim"}))
        return frame

    def _btn(self, parent, text, cmd, color="text", w=10):
        """color é chave do dicionário C, não valor hex."""
        b = tk.Button(parent, text=text, command=cmd,
                       bg=C["border"], fg=C[color], font=self.f_btn,
                       relief="flat", width=w, cursor="hand2",
                       activebackground=C["bg"], activeforeground=C[color],
                       bd=0, padx=4, pady=4)
        b.bind("<Enter>", lambda e, bt=b: bt.config(bg=C["bg"]))
        b.bind("<Leave>", lambda e, bt=b: bt.config(bg=C["border"]))
        self._btn_list.append((b, color))
        return b

    def _style_combobox(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TCombobox",
                         fieldbackground=C["border"], background=C["border"],
                         foreground=C["text_head"], selectbackground=C["border"],
                         selectforeground=C["accent"])

    # ─── Log ──────────────────────────────────────────────────────────────────
    def _log(self, direction, cmd_byte, label, extra=""):
        ts   = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        tag  = "tx" if direction == "TX" else "rx"
        line = (f"[{ts}] {direction}  0x{cmd_byte:02X}  {label:<26} {extra}\n"
                if extra else
                f"[{ts}] {direction}  0x{cmd_byte:02X}  {label}\n")
        self._log_text.config(state="normal")
        self._log_text.insert("end", line, tag)
        self._log_text.see("end")
        self._log_text.config(state="disabled")

    def _log_sys(self, msg):
        ts   = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        line = f"[{ts}] ···  {msg}\n"
        self._log_text.config(state="normal")
        self._log_text.insert("end", line, "sys")
        self._log_text.see("end")
        self._log_text.config(state="disabled")

    def _clear_log(self):
        self._log_text.config(state="normal")
        self._log_text.delete("1.0", "end")
        self._log_text.config(state="disabled")

    # ─── Serial callbacks ─────────────────────────────────────────────────────
    def _on_serial_event(self, event, data):
        if event == "rx":
            self.after(0, self._handle_rx, data)
        elif event == "disconnected":
            self.after(0, self._on_disconnected)

    def _handle_rx(self, data):
        status, payload = data
        payload_name = RX_NAMES.get(payload, f"0x{payload:02X}")
        status_str   = "OK" if status == CMD_OK else "ERR"

        if status == CMD_ERR:
            self._log("RX", status, "CMD_ERR", "")
            return

        self._log("RX", payload, payload_name, f"[{status_str}]")

        if payload == OBJ_DETECTED:
            self._fsm_state.set(1); self._refresh_fsm_display(1)
        elif payload == CLSS_REQUEST:
            self._fsm_state.set(2); self._refresh_fsm_display(2)
        elif payload == ROUTE_A_FWD:
            self._gate_state.set("FECHADA")
            self._fsm_state.set(3); self._refresh_fsm_display(3)
            self._update_status_colors()
        elif payload == ROUTE_B_FWD:
            self._gate_state.set("ABERTA")
            self._fsm_state.set(4); self._refresh_fsm_display(4)
            self._update_status_colors()
        elif payload in (ROUTE_A_OK, ROUTE_B_OK):
            self._fsm_state.set(0); self._refresh_fsm_display(0)
        elif payload == SYS_INIT_MSG:
            self._log_sys("✓ HANDSHAKE CONCLUÍDO — Sistema inicializado")
        elif payload == DEBUG_TOGGLE:
            new = "DEBUG" if self._op_mode.get() == "FSM" else "FSM"
            self._op_mode.set(new)
            self._update_status_colors()
            self._log_sys(f"MODO ALTERNADO → {new}")
        elif payload == GATE_OPEN:
            self._gate_state.set("ABERTA");    self._update_status_colors()
        elif payload == GATE_CLOSE:
            self._gate_state.set("FECHADA");   self._update_status_colors()
        elif payload == LIGHT_EN:
            self._flash_state.set("ON");       self._update_status_colors()
        elif payload == LIGHT_DIS:
            self._flash_state.set("OFF");      self._update_status_colors()
        elif payload == STPR_EN:
            self._motor_state.set("GIRANDO");  self._update_status_colors()
        elif payload == STPR_DIS:
            self._motor_state.set("LIVRE");    self._update_status_colors()
        elif payload == STPR_FORWARD:
            self._motor_dir.set("FRENTE");     self._update_status_colors()
        elif payload == STPR_BACKWARD:
            self._motor_dir.set("TRÁS");       self._update_status_colors()

    def _on_disconnected(self):
        self._conn_led.config(fg=C["red"])
        self._conn_label.config(text="CONEXÃO PERDIDA", fg=C["red"])
        self._btn_connect.config(text="CONECTAR", fg=C["green"])
        self._btn_handshake.config(state="disabled")
        self._log_sys("⚠ Conexão encerrada inesperadamente")

    # ─── Ações ────────────────────────────────────────────────────────────────
    def _refresh_ports(self):
        ports = self.serial.list_ports()
        self._port_cb["values"] = ports
        if ports:
            self._port_var.set(ports[0])

    def _toggle_connect(self):
        if self.serial.connected:
            self._stop_loop()
            self.serial.disconnect()
            self._conn_led.config(fg=C["text_dim"])
            self._conn_label.config(text="DESCONECTADO", fg=C["text_dim"])
            self._btn_connect.config(text="CONECTAR", fg=C["green"])
            self._btn_handshake.config(state="disabled")
            self._log_sys("Desconectado")
        else:
            port   = self._port_var.get()
            baud   = int(self._baud_var.get())
            result = self.serial.connect(port, baud)
            if result is True:
                self._conn_led.config(fg=C["green"])
                self._conn_label.config(text=f"CONECTADO  {port}", fg=C["green"])
                self._btn_connect.config(text="DESCONECTAR", fg=C["red"])
                self._btn_handshake.config(state="normal")
                self._log_sys(f"Conectado em {port} @ {baud} baud")
            else:
                messagebox.showerror("Erro de conexão", str(result))

    def _do_handshake(self):
        self._send(SYS_RDY_MSG)
        self._log_sys("Handshake enviado — aguardando SYS_INIT do STM32...")

    def _toggle_debug(self):
        self._send(DEBUG_TOGGLE)

    def _send(self, cmd, data=None):
        if not self.serial.connected:
            self._log_sys("⚠ Não conectado")
            return
        self.serial.send_frame(cmd, data)
        name  = TX_NAMES.get(cmd, f"0x{cmd:02X}")
        extra = f"DATA=0x{data:02X}" if data is not None else ""
        self._log("TX", cmd, name, extra)

    def _send_steps(self):
        steps = self._step_count.get()
        if self._loop_mode.get():
            self._start_loop(steps)
        else:
            self._send(STPR_TGT_STPS, steps)

    def _on_loop_toggle(self):
        if not self._loop_mode.get():
            self._stop_loop()

    def _start_loop(self, steps):
        if self._step_loop_running:
            return
        self._step_loop_running = True
        self._btn_send_steps.config(state="disabled")
        self._btn_stop_loop.config(state="normal")
        rate = int(1 / LOOP_INTERVAL)
        self._loop_status.config(
            text=f"● LOOP ATIVO — {steps} passos/ciclo  ({rate} cmd/s)")

        def loop():
            while self._step_loop_running:
                if self.serial.connected:
                    # Enfileira sem logar (evita flood no Text widget)
                    self.serial.send_frame(STPR_TGT_STPS, steps)
                time.sleep(LOOP_INTERVAL)

        self._step_loop_thread = threading.Thread(target=loop, daemon=True)
        self._step_loop_thread.start()

    def _stop_loop(self):
        self._step_loop_running = False
        self._btn_send_steps.config(state="normal")
        self._btn_stop_loop.config(state="disabled")
        self._loop_status.config(text="")
        self._step_loop_thread = None

    def _on_close(self):
        self._stop_loop()
        if self.serial.connected:
            self.serial.disconnect()
        self.destroy()


# ─── ENTRY POINT ──────────────────────────────────────────────────────────────
if __name__ == "__main__":
    app = EsteiraApp()
    app.mainloop()
