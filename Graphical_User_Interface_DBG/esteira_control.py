"""
╔══════════════════════════════════════════════════════════╗
║     ESTEIRA SEPARADORA — PAINEL DE CONTROLE UART         ║
║     STM32G070 ↔ Raspberry Pi 3B                          ║
║     Firmware v2.1 — SOE 2026.1                           ║
╚══════════════════════════════════════════════════════════╝
Dependências:
    pip3 install pyserial
"""

import tkinter as tk
from tkinter import ttk, font as tkfont, messagebox
import serial
import serial.tools.list_ports
import threading
import time
from datetime import datetime
from collections import deque

# ─── PROTOCOLO (espelho do firmware v2.1) ──────────────────────────────────────
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

# Mapeamento de bytes TX para nomes legíveis
RX_NAMES = {
    CMD_OK:       "CMD_OK",
    CMD_ERR:      "CMD_ERR",
    OBJ_DETECTED: "OBJ_DETECTED",
    CLSS_REQUEST: "CLSS_REQUEST",
    ROUTE_A_FWD:  "ROUTE_A_FWD",
    ROUTE_B_FWD:  "ROUTE_B_FWD",
    ROUTE_A_OK:   "ROUTE_A_OK",
    ROUTE_B_OK:   "ROUTE_B_OK",
    SYS_INIT_MSG: "SYS_INIT",
    LIGHT_EN:     "LIGHT_EN (eco)",
    LIGHT_DIS:    "LIGHT_DIS (eco)",
    GATE_OPEN:    "GATE_OPEN (eco)",
    GATE_CLOSE:   "GATE_CLOSE (eco)",
    STPR_EN:      "STPR_EN (eco)",
    STPR_DIS:     "STPR_DIS (eco)",
    STPR_FORWARD: "STPR_FORWARD (eco)",
    STPR_BACKWARD:"STPR_BACKWARD (eco)",
    STPR_TGT_STPS:"STPR_TGT_STPS (eco)",
    DEBUG_TOGGLE: "DEBUG_TOGGLE (eco)",
    ROUTE_A_RECV: "ROUTE_A (eco)",
    ROUTE_B_RECV: "ROUTE_B (eco)",
}

TX_NAMES = {
    LIGHT_EN:     "LIGHT_EN",
    LIGHT_DIS:    "LIGHT_DIS",
    GATE_OPEN:    "GATE_OPEN",
    GATE_CLOSE:   "GATE_CLOSE",
    STPR_EN:      "STPR_EN",
    STPR_DIS:     "STPR_DIS",
    STPR_FORWARD: "STPR_FORWARD",
    STPR_BACKWARD:"STPR_BACKWARD",
    STPR_TGT_STPS:"STPR_TGT_STPS",
    DEBUG_TOGGLE: "DEBUG_TOGGLE",
    ROUTE_A_RECV: "ROUTE_A",
    ROUTE_B_RECV: "ROUTE_B",
    SYS_RDY_MSG:  "SYS_RDY",
}

# ─── PALETA ────────────────────────────────────────────────────────────────────
C = {
    "bg":        "#0D0F14",
    "panel":     "#13161E",
    "border":    "#1E2330",
    "accent":    "#00D4FF",
    "accent2":   "#FF6B35",
    "green":     "#00FF9C",
    "red":       "#FF3B5C",
    "yellow":    "#FFD700",
    "text":      "#C8D0E0",
    "text_dim":  "#4A5568",
    "text_head": "#E8EDF5",
    "ok":        "#00C87A",
    "err":       "#FF3B5C",
    "warn":      "#FF9F00",
}

FSM_STATES = {
    0: ("IDLE",                C["text_dim"]),
    1: ("OBJECT DETECTED",     C["yellow"]),
    2: ("WAIT CLASSIFICATION", C["accent"]),
    3: ("ROUTE A",             C["green"]),
    4: ("ROUTE B",             C["accent2"]),
    255: ("—",                 C["text_dim"]),
}

# ─── SERIAL MANAGER ───────────────────────────────────────────────────────────
class SerialManager:
    def __init__(self):
        self.ser = None
        self.connected = False
        self._lock = threading.Lock()
        self._rx_buf = bytearray()
        self._callbacks = []   # (event_type, data)
        self._running = False
        self._thread = None

    def list_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port, baud=115200):
        with self._lock:
            try:
                self.ser = serial.Serial(port, baud, timeout=0.05)
                self.connected = True
                self._running = True
                self._thread = threading.Thread(target=self._rx_loop, daemon=True)
                self._thread.start()
                return True
            except Exception as e:
                self.connected = False
                return str(e)

    def disconnect(self):
        self._running = False
        with self._lock:
            if self.ser and self.ser.is_open:
                self.ser.close()
            self.connected = False

    def send_frame(self, cmd, data=None):
        """Envia [0xAA][CMD] ou [0xAA][CMD][DATA]"""
        if not self.connected:
            return False
        frame = bytes([START_FRAME, cmd])
        if data is not None:
            frame += bytes([data])
        with self._lock:
            try:
                self.ser.write(frame)
                return True
            except:
                self.connected = False
                return False

    def add_callback(self, cb):
        self._callbacks.append(cb)

    def _notify(self, event, data):
        for cb in self._callbacks:
            cb(event, data)

    def _rx_loop(self):
        """Parser de frame RX: [STATUS][PAYLOAD?]"""
        state = "IDLE"
        status_byte = 0
        while self._running:
            try:
                with self._lock:
                    if not self.ser or not self.ser.is_open:
                        break
                    raw = self.ser.read(self.ser.in_waiting or 1)
            except:
                break
            for b in raw:
                if state == "IDLE":
                    if b in (CMD_OK, CMD_ERR):
                        status_byte = b
                        state = "WAIT_PAYLOAD"
                    # bytes fora de frame ignorados
                elif state == "WAIT_PAYLOAD":
                    # Payload do frame anterior
                    self._notify("rx", (status_byte, b))
                    state = "IDLE"
                    status_byte = 0
            # Se ficou esperando payload e não veio (CMD_ERR é 1 byte)
            # Tratar timeout simples: próximo byte decidirá
        self._notify("disconnected", None)


# ─── APLICAÇÃO ────────────────────────────────────────────────────────────────
class EsteiraApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ESTEIRA SEPARADORA — Painel de Controle")
        self.configure(bg=C["bg"])
        self.resizable(True, True)
        self.minsize(900, 640)

        self.serial = SerialManager()
        self.serial.add_callback(self._on_serial_event)

        # Estado da interface
        self._op_mode = tk.StringVar(value="FSM")          # "FSM" | "DEBUG"
        self._fsm_state = tk.IntVar(value=255)
        self._gate_state = tk.StringVar(value="FECHADA")
        self._motor_state = tk.StringVar(value="PARADO")
        self._motor_dir = tk.StringVar(value="FRENTE")
        self._flash_state = tk.StringVar(value="OFF")
        self._step_loop_running = False
        self._step_loop_thread = None
        self._step_dir = tk.IntVar(value=0)   # 0=frente, 1=trás
        self._step_count = tk.IntVar(value=100)
        self._loop_mode = tk.BooleanVar(value=False)

        self._log_entries = deque(maxlen=200)

        self._build_fonts()
        self._build_ui()
        self._refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_fonts(self):
        self.f_mono  = tkfont.Font(family="Courier New", size=9)
        self.f_mono_b= tkfont.Font(family="Courier New", size=9, weight="bold")
        self.f_label = tkfont.Font(family="Courier New", size=8)
        self.f_title = tkfont.Font(family="Courier New", size=11, weight="bold")
        self.f_state = tkfont.Font(family="Courier New", size=14, weight="bold")
        self.f_btn   = tkfont.Font(family="Courier New", size=9, weight="bold")
        self.f_head  = tkfont.Font(family="Courier New", size=10, weight="bold")

    # ── UI ────────────────────────────────────────────────────────────────────
    def _build_ui(self):
        # Header
        hdr = tk.Frame(self, bg=C["bg"], pady=8)
        hdr.pack(fill="x", padx=12, pady=(10, 0))
        tk.Label(hdr, text="▸ ESTEIRA SEPARADORA", bg=C["bg"],
                 fg=C["accent"], font=self.f_title).pack(side="left")
        tk.Label(hdr, text="STM32G070 — FIRMWARE v2.1",
                 bg=C["bg"], fg=C["text_dim"], font=self.f_label).pack(side="left", padx=16)

        # Linha de conexão
        conn_frame = self._panel(self, label="CONEXÃO SERIAL")
        conn_frame.pack(fill="x", padx=12, pady=6)
        self._build_connection_bar(conn_frame)

        # Corpo principal: 2 colunas
        body = tk.Frame(self, bg=C["bg"])
        body.pack(fill="both", expand=True, padx=12, pady=4)
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=2)
        body.rowconfigure(0, weight=1)

        # Coluna esquerda
        left = tk.Frame(body, bg=C["bg"])
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 6))
        self._build_status_panel(left)
        self._build_fsm_panel(left)
        self._build_fsm_controls(left)

        # Coluna direita
        right = tk.Frame(body, bg=C["bg"])
        right.grid(row=0, column=1, sticky="nsew")
        self._build_debug_panel(right)
        self._build_log_panel(right)

    # ─── Conexão ──────────────────────────────────────────────────────────────
    def _build_connection_bar(self, parent):
        inner = tk.Frame(parent, bg=C["panel"])
        inner.pack(fill="x", padx=6, pady=6)

        tk.Label(inner, text="PORTA:", bg=C["panel"], fg=C["text_dim"],
                 font=self.f_label).pack(side="left", padx=(6, 2))
        self._port_var = tk.StringVar()
        self._port_cb = ttk.Combobox(inner, textvariable=self._port_var,
                                     width=18, state="readonly")
        self._port_cb.pack(side="left", padx=4)
        self._style_combobox()

        tk.Label(inner, text="BAUD:", bg=C["panel"], fg=C["text_dim"],
                 font=self.f_label).pack(side="left", padx=(8, 2))
        self._baud_var = tk.StringVar(value="115200")
        baud_cb = ttk.Combobox(inner, textvariable=self._baud_var,
                               values=["9600","19200","57600","115200","230400"],
                               width=8, state="readonly")
        baud_cb.pack(side="left", padx=4)

        self._btn_refresh = self._btn(inner, "⟳", self._refresh_ports,
                                      color=C["text_dim"], w=3)
        self._btn_refresh.pack(side="left", padx=4)

        self._btn_connect = self._btn(inner, "CONECTAR", self._toggle_connect,
                                      color=C["green"], w=12)
        self._btn_connect.pack(side="left", padx=8)

        self._conn_led = tk.Label(inner, text="●", bg=C["panel"],
                                  fg=C["text_dim"], font=self.f_head)
        self._conn_led.pack(side="left", padx=4)
        self._conn_label = tk.Label(inner, text="DESCONECTADO", bg=C["panel"],
                                    fg=C["text_dim"], font=self.f_label)
        self._conn_label.pack(side="left")

        # Handshake
        self._btn_handshake = self._btn(inner, "HANDSHAKE", self._do_handshake,
                                         color=C["accent"], w=12)
        self._btn_handshake.pack(side="right", padx=8)
        self._btn_handshake.config(state="disabled")

    # ─── Status ───────────────────────────────────────────────────────────────
    def _build_status_panel(self, parent):
        p = self._panel(parent, label="STATUS DO HARDWARE")
        p.pack(fill="x", pady=(0, 6))
        inner = tk.Frame(p, bg=C["panel"])
        inner.pack(fill="x", padx=6, pady=6)

        rows = [
            ("MODO",        self._op_mode,    None),
            ("CANCELA",     self._gate_state,  None),
            ("MOTOR",       self._motor_state, None),
            ("DIREÇÃO",     self._motor_dir,   None),
            ("FLASH",       self._flash_state, None),
        ]
        self._status_labels = {}
        for i, (name, var, _) in enumerate(rows):
            tk.Label(inner, text=f"{name}:", bg=C["panel"], fg=C["text_dim"],
                     font=self.f_label, anchor="w", width=10).grid(
                row=i, column=0, sticky="w", padx=6, pady=2)
            lbl = tk.Label(inner, textvariable=var, bg=C["panel"],
                           fg=C["accent"], font=self.f_mono_b, anchor="w")
            lbl.grid(row=i, column=1, sticky="w", padx=4, pady=2)
            self._status_labels[name] = lbl
        self._update_status_colors()

    def _update_status_colors(self):
        mode = self._op_mode.get()
        self._status_labels["MODO"].config(
            fg=C["accent"] if mode == "FSM" else C["accent2"])
        gate = self._gate_state.get()
        self._status_labels["CANCELA"].config(
            fg=C["green"] if gate == "ABERTA" else C["red"])
        motor = self._motor_state.get()
        self._status_labels["MOTOR"].config(
            fg=C["green"] if motor == "GIRANDO" else C["text_dim"])
        flash = self._flash_state.get()
        self._status_labels["FLASH"].config(
            fg=C["yellow"] if flash == "ON" else C["text_dim"])

    # ─── FSM State ────────────────────────────────────────────────────────────
    def _build_fsm_panel(self, parent):
        p = self._panel(parent, label="MÁQUINA DE ESTADOS")
        p.pack(fill="x", pady=(0, 6))
        inner = tk.Frame(p, bg=C["panel"])
        inner.pack(fill="x", padx=6, pady=10)

        self._fsm_state_label = tk.Label(inner, text="—", bg=C["panel"],
                                          fg=C["text_dim"], font=self.f_state,
                                          anchor="center")
        self._fsm_state_label.pack(fill="x")

        # Pipeline visual dos 5 estados
        pipe_frame = tk.Frame(inner, bg=C["panel"])
        pipe_frame.pack(fill="x", pady=(8, 0))
        states_order = [0, 1, 2, 3, 4]
        state_names  = ["IDLE", "OBJ\nDET", "WAIT\nCLSS", "ROUTE\nA", "ROUTE\nB"]
        self._state_boxes = {}
        for i, (sid, sname) in enumerate(zip(states_order, state_names)):
            col = tk.Frame(pipe_frame, bg=C["panel"])
            col.pack(side="left", expand=True, fill="x")
            box = tk.Label(col, text=sname, bg=C["border"],
                           fg=C["text_dim"], font=self.f_label,
                           relief="flat", padx=4, pady=4, justify="center")
            box.pack(fill="x", padx=2)
            self._state_boxes[sid] = box
            if i < len(states_order) - 1:
                tk.Label(pipe_frame, text="▸", bg=C["panel"],
                         fg=C["text_dim"], font=self.f_label).pack(side="left")

    def _refresh_fsm_display(self, state_id):
        name, color = FSM_STATES.get(state_id, ("?", C["text_dim"]))
        self._fsm_state_label.config(text=name, fg=color)
        for sid, box in self._state_boxes.items():
            if sid == state_id:
                sn, sc = FSM_STATES.get(sid, ("?", C["text_dim"]))
                box.config(bg=sc, fg=C["bg"])
            else:
                box.config(bg=C["border"], fg=C["text_dim"])

    # ─── Controles FSM ────────────────────────────────────────────────────────
    def _build_fsm_controls(self, parent):
        p = self._panel(parent, label="COMANDOS FSM")
        p.pack(fill="x", pady=(0, 6))
        inner = tk.Frame(p, bg=C["panel"])
        inner.pack(fill="x", padx=6, pady=6)

        row1 = tk.Frame(inner, bg=C["panel"])
        row1.pack(fill="x", pady=2)
        self._btn(row1, "→ ROTA A", lambda: self._send(ROUTE_A_RECV),
                  color=C["green"], w=14).pack(side="left", padx=4)
        self._btn(row1, "→ ROTA B", lambda: self._send(ROUTE_B_RECV),
                  color=C["accent2"], w=14).pack(side="left", padx=4)

        row2 = tk.Frame(inner, bg=C["panel"])
        row2.pack(fill="x", pady=2)
        self._btn(row2, "⇄ TOGGLE DEBUG", self._toggle_debug,
                  color=C["yellow"], w=30).pack(side="left", padx=4)

    # ─── Debug Panel ──────────────────────────────────────────────────────────
    def _build_debug_panel(self, parent):
        p = self._panel(parent, label="CONTROLE ASSÍNCRONO (MODO DEBUG)")
        p.pack(fill="x", pady=(0, 6))
        inner = tk.Frame(p, bg=C["panel"])
        inner.pack(fill="x", padx=6, pady=6)

        # Linha 1: Flash + Cancela
        row1 = tk.Frame(inner, bg=C["panel"])
        row1.pack(fill="x", pady=3)
        tk.Label(row1, text="FLASH:", bg=C["panel"], fg=C["text_dim"],
                 font=self.f_label, width=8).pack(side="left")
        self._btn(row1, "ON",  lambda: self._send(LIGHT_EN),
                  color=C["yellow"], w=6).pack(side="left", padx=3)
        self._btn(row1, "OFF", lambda: self._send(LIGHT_DIS),
                  color=C["text_dim"], w=6).pack(side="left", padx=3)

        tk.Label(row1, text="CANCELA:", bg=C["panel"], fg=C["text_dim"],
                 font=self.f_label, width=9).pack(side="left", padx=(16, 0))
        self._btn(row1, "ABRIR",   lambda: self._send(GATE_OPEN),
                  color=C["green"], w=7).pack(side="left", padx=3)
        self._btn(row1, "FECHAR",  lambda: self._send(GATE_CLOSE),
                  color=C["red"], w=7).pack(side="left", padx=3)

        # Linha 2: Motor enable
        row2 = tk.Frame(inner, bg=C["panel"])
        row2.pack(fill="x", pady=3)
        tk.Label(row2, text="MOTOR:", bg=C["panel"], fg=C["text_dim"],
                 font=self.f_label, width=8).pack(side="left")
        self._btn(row2, "ENGAJAR",    lambda: self._send(STPR_EN),
                  color=C["green"], w=10).pack(side="left", padx=3)
        self._btn(row2, "LIVRE",      lambda: self._send(STPR_DIS),
                  color=C["red"], w=10).pack(side="left", padx=3)

        # Linha 3: Direção
        row3 = tk.Frame(inner, bg=C["panel"])
        row3.pack(fill="x", pady=3)
        tk.Label(row3, text="DIREÇÃO:", bg=C["panel"], fg=C["text_dim"],
                 font=self.f_label, width=8).pack(side="left")

        def set_fwd():
            self._step_dir.set(0)
            self._motor_dir.set("FRENTE")
            self._send(STPR_FORWARD)
        def set_bwd():
            self._step_dir.set(1)
            self._motor_dir.set("TRÁS")
            self._send(STPR_BACKWARD)

        self._btn(row3, "◀ FRENTE", set_fwd, color=C["accent"], w=10).pack(side="left", padx=3)
        self._btn(row3, "TRÁS ▶",   set_bwd, color=C["accent"], w=10).pack(side="left", padx=3)

        # Separador
        tk.Frame(inner, bg=C["border"], height=1).pack(fill="x", pady=6)

        # Bloco de passos
        tk.Label(inner, text="CONTROLE DE PASSOS",
                 bg=C["panel"], fg=C["accent"], font=self.f_label).pack(anchor="w")

        row4 = tk.Frame(inner, bg=C["panel"])
        row4.pack(fill="x", pady=4)
        tk.Label(row4, text="Nº DE PASSOS:", bg=C["panel"], fg=C["text_dim"],
                 font=self.f_label).pack(side="left", padx=(0, 6))
        vcmd = (self.register(lambda s: s.isdigit() and int(s) <= 255), "%P")
        self._step_entry = tk.Spinbox(row4, from_=1, to=255,
                                      textvariable=self._step_count,
                                      width=6, font=self.f_mono,
                                      bg=C["border"], fg=C["text_head"],
                                      buttonbackground=C["border"],
                                      relief="flat", insertbackground=C["accent"])
        self._step_entry.pack(side="left", padx=4)
        tk.Label(row4, text="(0–255)", bg=C["panel"], fg=C["text_dim"],
                 font=self.f_label).pack(side="left")

        row5 = tk.Frame(inner, bg=C["panel"])
        row5.pack(fill="x", pady=3)
        tk.Checkbutton(row5, text="MODO LOOP (rebobina steps continuamente)",
                       variable=self._loop_mode,
                       bg=C["panel"], fg=C["text"], selectcolor=C["border"],
                       activebackground=C["panel"], activeforeground=C["text"],
                       font=self.f_label, command=self._on_loop_toggle).pack(side="left")

        row6 = tk.Frame(inner, bg=C["panel"])
        row6.pack(fill="x", pady=3)
        self._btn_send_steps = self._btn(row6, "▶ ENVIAR PASSOS",
                                          self._send_steps,
                                          color=C["green"], w=18)
        self._btn_send_steps.pack(side="left", padx=3)
        self._btn_stop_loop = self._btn(row6, "■ PARAR LOOP",
                                         self._stop_loop,
                                         color=C["red"], w=14)
        self._btn_stop_loop.pack(side="left", padx=3)
        self._btn_stop_loop.config(state="disabled")

        # Loop status
        self._loop_status = tk.Label(inner, text="", bg=C["panel"],
                                      fg=C["yellow"], font=self.f_label)
        self._loop_status.pack(anchor="w", pady=2)

    # ─── Log ──────────────────────────────────────────────────────────────────
    def _build_log_panel(self, parent):
        p = self._panel(parent, label="LOG DE COMUNICAÇÃO")
        p.pack(fill="both", expand=True)
        inner = tk.Frame(p, bg=C["panel"])
        inner.pack(fill="both", expand=True, padx=6, pady=6)
        inner.rowconfigure(0, weight=1)
        inner.columnconfigure(0, weight=1)

        self._log_text = tk.Text(inner, bg=C["bg"], fg=C["text"],
                                  font=self.f_mono, relief="flat",
                                  state="disabled", wrap="none",
                                  insertbackground=C["accent"])
        self._log_text.grid(row=0, column=0, sticky="nsew")

        sb_y = ttk.Scrollbar(inner, orient="vertical",
                              command=self._log_text.yview)
        sb_y.grid(row=0, column=1, sticky="ns")
        self._log_text.config(yscrollcommand=sb_y.set)

        sb_x = ttk.Scrollbar(inner, orient="horizontal",
                              command=self._log_text.xview)
        sb_x.grid(row=1, column=0, sticky="ew")
        self._log_text.config(xscrollcommand=sb_x.set)

        # Tags de cor
        self._log_text.tag_config("tx",  foreground=C["accent"])
        self._log_text.tag_config("rx",  foreground=C["green"])
        self._log_text.tag_config("ok",  foreground=C["ok"])
        self._log_text.tag_config("err", foreground=C["err"])
        self._log_text.tag_config("sys", foreground=C["yellow"])
        self._log_text.tag_config("dim", foreground=C["text_dim"])

        # Botão limpar
        ctrl = tk.Frame(p, bg=C["panel"])
        ctrl.pack(fill="x", padx=6, pady=(0, 4))
        self._btn(ctrl, "LIMPAR LOG", self._clear_log,
                  color=C["text_dim"], w=12).pack(side="right")

    # ─── Helpers de UI ────────────────────────────────────────────────────────
    def _panel(self, parent, label=""):
        frame = tk.Frame(parent, bg=C["panel"], bd=1, relief="flat",
                         highlightbackground=C["border"], highlightthickness=1)
        if label:
            tk.Label(frame, text=f" {label} ", bg=C["border"],
                     fg=C["text_dim"], font=self.f_label,
                     padx=6, pady=2).pack(anchor="nw", fill="x")
        return frame

    def _btn(self, parent, text, cmd, color=C["text"], w=10):
        b = tk.Button(parent, text=text, command=cmd,
                      bg=C["border"], fg=color, font=self.f_btn,
                      relief="flat", width=w, cursor="hand2",
                      activebackground=C["bg"], activeforeground=color,
                      bd=0, padx=4, pady=4)
        b.bind("<Enter>", lambda e: b.config(bg=C["bg"]))
        b.bind("<Leave>", lambda e: b.config(bg=C["border"]))
        return b

    def _style_combobox(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TCombobox",
                         fieldbackground=C["border"],
                         background=C["border"],
                         foreground=C["text_head"],
                         selectbackground=C["border"],
                         selectforeground=C["accent"])

    # ─── Log helpers ──────────────────────────────────────────────────────────
    def _log(self, direction, cmd_byte, label, extra=""):
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        tag = "tx" if direction == "TX" else "rx"
        if extra:
            line = f"[{ts}] {direction}  0x{cmd_byte:02X}  {label:<24} {extra}\n"
        else:
            line = f"[{ts}] {direction}  0x{cmd_byte:02X}  {label}\n"
        self._log_text.config(state="normal")
        self._log_text.insert("end", line, tag)
        self._log_text.see("end")
        self._log_text.config(state="disabled")

    def _log_sys(self, msg):
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
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
        status_str = "OK" if status == CMD_OK else "ERR"

        if status == CMD_ERR:
            self._log("RX", status, f"CMD_ERR", "")
            return

        self._log("RX", payload, payload_name, f"[{status_str}]")

        # Atualiza estado da FSM com base nas mensagens espontâneas
        if payload == OBJ_DETECTED:
            self._fsm_state.set(1)
            self.after(0, lambda: self._refresh_fsm_display(1))
        elif payload == CLSS_REQUEST:
            self._fsm_state.set(2)
            self.after(0, lambda: self._refresh_fsm_display(2))
        elif payload == ROUTE_A_FWD:
            self._fsm_state.set(3)
            self._gate_state.set("FECHADA")
            self.after(0, lambda: self._refresh_fsm_display(3))
            self._update_status_colors()
        elif payload == ROUTE_B_FWD:
            self._fsm_state.set(4)
            self._gate_state.set("ABERTA")
            self.after(0, lambda: self._refresh_fsm_display(4))
            self._update_status_colors()
        elif payload in (ROUTE_A_OK, ROUTE_B_OK):
            self._fsm_state.set(0)
            self.after(0, lambda: self._refresh_fsm_display(0))
        elif payload == SYS_INIT_MSG:
            self._log_sys("✓ HANDSHAKE CONCLUÍDO — Sistema inicializado")
        elif payload == DEBUG_TOGGLE:
            # Toggle do modo
            cur = self._op_mode.get()
            new = "DEBUG" if cur == "FSM" else "FSM"
            self._op_mode.set(new)
            self._update_status_colors()
            self._log_sys(f"MODO ALTERNADO → {new}")
        # Ecos de comandos assíncronos
        elif payload == GATE_OPEN:
            self._gate_state.set("ABERTA");   self._update_status_colors()
        elif payload == GATE_CLOSE:
            self._gate_state.set("FECHADA");  self._update_status_colors()
        elif payload == LIGHT_EN:
            self._flash_state.set("ON");      self._update_status_colors()
        elif payload == LIGHT_DIS:
            self._flash_state.set("OFF");     self._update_status_colors()
        elif payload == STPR_EN:
            self._motor_state.set("GIRANDO"); self._update_status_colors()
        elif payload == STPR_DIS:
            self._motor_state.set("LIVRE");   self._update_status_colors()
        elif payload == STPR_FORWARD:
            self._motor_dir.set("FRENTE");    self._update_status_colors()
        elif payload == STPR_BACKWARD:
            self._motor_dir.set("TRÁS");      self._update_status_colors()

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
            port = self._port_var.get()
            baud = int(self._baud_var.get())
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
        ok = self.serial.send_frame(cmd, data)
        name = TX_NAMES.get(cmd, f"0x{cmd:02X}")
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
        self._loop_status.config(text=f"● LOOP ATIVO — {steps} passos / ciclo")

        def loop():
            while self._step_loop_running:
                self.serial.send_frame(STPR_TGT_STPS, steps)
                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                line = f"[{ts}] TX  0x{STPR_TGT_STPS:02X}  STPR_TGT_STPS  [LOOP] DATA=0x{steps:02X}\n"
                self._log_text.config(state="normal")
                self._log_text.insert("end", line, "tx")
                self._log_text.see("end")
                self._log_text.config(state="disabled")
                time.sleep(0.15)  # Intervalo do loop

        self._step_loop_thread = threading.Thread(target=loop, daemon=True)
        self._step_loop_thread.start()

    def _stop_loop(self):
        self._step_loop_running = False
        self._btn_send_steps.config(state="normal")
        self._btn_stop_loop.config(state="disabled")
        self._loop_status.config(text="")
        if self._step_loop_thread:
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
