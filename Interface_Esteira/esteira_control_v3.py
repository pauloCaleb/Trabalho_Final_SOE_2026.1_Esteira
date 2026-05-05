"""
╔══════════════════════════════════════════════════════════════════╗
║   ESTEIRA SEPARADORA — SISTEMA DE CONTROLE                       ║
║   STM32G070 ↔ Raspberry Pi 3B  — v3.0                          ║
║   Firmware v2.1 — SOE 2026.1                                     ║
╚══════════════════════════════════════════════════════════════════╝

Dependências:
    pip install pyserial opencv-python Pillow

Formato do QR Code:
    Frame UART completo em hexadecimal.
    Exemplos aceitos:
        "0xAADA"  →  Rota A
        "0xAADB"  →  Rota B
        "AADB"    →  Rota B  (sem prefixo, também aceito)
        "AA DA"   →  Rota A  (com espaço, também aceito)

Modos de operação:
    APLICAÇÃO PRINCIPAL  — leitura de QR + envio automático à FSM do STM32
    MODO DEBUG           — painel de controle UART manual completo
"""

import tkinter as tk
from tkinter import ttk, font as tkfont, messagebox
import serial
import serial.tools.list_ports
import threading
import queue
import time
from datetime import datetime

try:
    import cv2
    CV2_AVAILABLE = True
except ImportError:
    CV2_AVAILABLE = False

try:
    from PIL import Image, ImageTk
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False

QR_AVAILABLE = CV2_AVAILABLE and PIL_AVAILABLE

# ─── PROTOCOLO ────────────────────────────────────────────────────────────────
START_FRAME  = 0xAA
CMD_OK       = 0x90
CMD_ERR      = 0x91
SYS_RDY_MSG  = 0x10
SYS_INIT_MSG = 0x01
ROUTE_A_RECV = 0xDA
ROUTE_B_RECV = 0xDB
OBJ_DETECTED = 0xA0
CLSS_REQUEST = 0xC0
ROUTE_A_FWD  = 0xFA
ROUTE_B_FWD  = 0xFB
ROUTE_A_OK   = 0xBA
ROUTE_B_OK   = 0xBB
LIGHT_EN     = 0xE1
LIGHT_DIS    = 0xD1
GATE_OPEN    = 0xE2
GATE_CLOSE   = 0xD2
STPR_EN      = 0xE3
STPR_DIS     = 0xD3
STPR_FORWARD = 0xE4
STPR_BACKWARD= 0xD4
STPR_TGT_STPS= 0xE5
DEBUG_TOGGLE = 0xDD

CMD_LABELS = {
    ROUTE_A_RECV: "ROTA A",        ROUTE_B_RECV: "ROTA B",
    OBJ_DETECTED: "OBJ_DETECTED",  CLSS_REQUEST: "CLSS_REQUEST",
    ROUTE_A_FWD:  "ROUTE_A_FWD",   ROUTE_B_FWD:  "ROUTE_B_FWD",
    ROUTE_A_OK:   "ROUTE_A_OK",    ROUTE_B_OK:   "ROUTE_B_OK",
    SYS_INIT_MSG: "SYS_INIT",      SYS_RDY_MSG:  "SYS_RDY",
    LIGHT_EN:     "LIGHT_EN",      LIGHT_DIS:    "LIGHT_DIS",
    GATE_OPEN:    "GATE_OPEN",     GATE_CLOSE:   "GATE_CLOSE",
    STPR_EN:      "STPR_EN",       STPR_DIS:     "STPR_DIS",
    STPR_FORWARD: "STPR_FWD",      STPR_BACKWARD:"STPR_BCK",
    STPR_TGT_STPS:"STPR_STPS",     DEBUG_TOGGLE: "DBG_TOGGLE",
    CMD_OK:       "CMD_OK",        CMD_ERR:      "CMD_ERR",
}

QR_CMD_COLOR = {
    ROUTE_A_RECV: "green",   ROUTE_B_RECV: "accent2",
    LIGHT_EN:     "yellow",  LIGHT_DIS:    "text_dim",
    GATE_OPEN:    "green",   GATE_CLOSE:   "red",
    STPR_EN:      "green",   STPR_DIS:     "red",
    STPR_FORWARD: "accent",  STPR_BACKWARD:"accent",
    DEBUG_TOGGLE: "yellow",
}

FSM_STATE_DEFS = {
    0:   ("IDLE",                "text_dim"),
    1:   ("OBJECT DETECTED",     "yellow"),
    2:   ("WAIT CLASSIFICATION", "accent"),
    3:   ("ROUTE A",             "green"),
    4:   ("ROUTE B",             "accent2"),
    255: ("—",                   "text_dim"),
}

# ─── TEMAS ────────────────────────────────────────────────────────────────────
THEMES = {
    "dark": {
        "bg":          "#0D0F14", "panel":    "#13161E",
        "border":      "#1E2330", "accent":   "#00D4FF",
        "accent2":     "#FF6B35", "green":    "#00D97A",
        "red":         "#FF3B5C", "yellow":   "#FFD700",
        "text":        "#C8D0E0", "text_dim": "#4A5568",
        "text_head":   "#E8EDF5",
        "log_tx":      "#38BDF8", "log_rx":   "#34D399",
        "log_sys":     "#FBBF24", "log_qr":   "#C084FC",
        "err":         "#FF3B5C",
        "tab_act_bg":  "#00D4FF", "tab_act_fg":  "#0D0F14",
        "tab_idle_bg": "#1E2330", "tab_idle_fg": "#4A5568",
        "cam_idle":    "#1E2330", "cam_scan":    "#FFD700",
        "cam_found":   "#00D97A",
        "spinbox_bg":  "#1E2330",
        "state_box_active_fg": "#0D0F14",
    },
    "light": {
        "bg":          "#EEF1F7", "panel":    "#FFFFFF",
        "border":      "#C8D3E6", "accent":   "#0077AA",
        "accent2":     "#C04400", "green":    "#006B44",
        "red":         "#BB1133", "yellow":   "#886600",
        "text":        "#2D3748", "text_dim": "#7A8899",
        "text_head":   "#111827",
        "log_tx":      "#005F99", "log_rx":   "#005533",
        "log_sys":     "#775500", "log_qr":   "#7C3AED",
        "err":         "#BB1133",
        "tab_act_bg":  "#0077AA", "tab_act_fg":  "#FFFFFF",
        "tab_idle_bg": "#C8D3E6", "tab_idle_fg": "#7A8899",
        "cam_idle":    "#C8D3E6", "cam_scan":    "#886600",
        "cam_found":   "#006B44",
        "spinbox_bg":  "#E4EAF4",
        "state_box_active_fg": "#FFFFFF",
    },
}
C = dict(THEMES["dark"])

QR_COOLDOWN   = 2.0
LOOP_INTERVAL = 0.060


# ─── UTILS ────────────────────────────────────────────────────────────────────
def parse_qr_frame(raw: str):
    """
    Converte string do QR em (start_byte, cmd_byte) ou None.
    Aceita: "0xAADA" | "AADB" | "AA DB" | "AA-DA"
    """
    if not raw:
        return None
    cleaned = (raw.strip().upper()
               .replace("0X","").replace(" ","")
               .replace("-","").replace(":",""))
    try:
        if len(cleaned) >= 4:
            b0 = int(cleaned[0:2], 16)
            b1 = int(cleaned[2:4], 16)
            if b0 == START_FRAME:
                return (b0, b1)
    except ValueError:
        pass
    return None


def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


# ─── QR READER ────────────────────────────────────────────────────────────────
class QRCodeReader:
    def __init__(self):
        self._running   = False
        self._last_data = ""
        self._last_time = 0.0
        self.on_frame    = None
        self.on_detected = None
        self.on_lost     = None

    def start(self, idx=0):
        cap = cv2.VideoCapture(idx)
        ok  = cap.isOpened()
        cap.release()
        if not ok:
            return False
        self._running = True
        threading.Thread(target=self._loop, args=(idx,),
                          daemon=True, name="qr-reader").start()
        return True

    def stop(self):
        self._running = False

    def _loop(self, idx):
        cap = cv2.VideoCapture(idx)
        det = cv2.QRCodeDetector()
        while self._running:
            ret, frame = cap.read()
            if not ret:
                if self.on_lost:
                    self.on_lost()
                break
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            data, pts, _ = det.detectAndDecode(frame)
            if data and pts is not None:
                p = pts[0].astype(int)
                for i in range(4):
                    cv2.line(rgb, tuple(p[i]), tuple(p[(i+1)%4]),
                             (0, 217, 122), 2)
                cv2.putText(rgb, data, (p[0][0], max(p[0][1]-10,14)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55,
                            (0, 217, 122), 2)
                now = time.time()
                if data != self._last_data or now - self._last_time > QR_COOLDOWN:
                    self._last_data = data
                    self._last_time = now
                    parsed = parse_qr_frame(data)
                    if parsed and self.on_detected:
                        self.on_detected(data, parsed[1])
            if self.on_frame:
                self.on_frame(rgb)
            time.sleep(0.033)
        cap.release()


# ─── SERIAL MANAGER ───────────────────────────────────────────────────────────
class SerialManager:
    def __init__(self):
        self.ser       = None
        self.connected = False
        self._tx_q     = queue.Queue()
        self._cbs      = []
        self._running  = False
        self._lock     = threading.Lock()

    def list_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port, baud=115200):
        with self._lock:
            try:
                self.ser = serial.Serial(port, baud, timeout=0.02,
                                          write_timeout=0.1)
                self.connected = True
                self._running  = True
                threading.Thread(target=self._rx, daemon=True,
                                  name="ser-rx").start()
                threading.Thread(target=self._tx, daemon=True,
                                  name="ser-tx").start()
                return True
            except Exception as e:
                self.connected = False
                return str(e)

    def disconnect(self):
        self._running = False
        self._tx_q.put(None)
        with self._lock:
            try:
                if self.ser and self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.connected = False

    def send(self, cmd, data=None):
        if not self.connected:
            return False
        frame = bytes([START_FRAME, cmd])
        if data is not None:
            frame += bytes([data])
        self._tx_q.put(frame)
        return True

    def add_callback(self, cb):
        self._cbs.append(cb)

    def _notify(self, ev, d):
        for cb in self._cbs:
            cb(ev, d)

    def _tx(self):
        while self._running:
            try:
                frame = self._tx_q.get(timeout=0.5)
                if frame is None:
                    break
                with self._lock:
                    if self.ser and self.ser.is_open:
                        self.ser.write(frame)
            except queue.Empty:
                continue
            except Exception:
                self.connected = False
                self._notify("disconnected", None)
                break

    def _rx(self):
        st, sb = "IDLE", 0
        while self._running:
            try:
                with self._lock:
                    if not self.ser or not self.ser.is_open:
                        break
                    w = self.ser.in_waiting
                raw = self.ser.read(w or 1) if w >= 0 else b""
            except Exception:
                break
            for b in raw:
                if st == "IDLE":
                    if b in (CMD_OK, CMD_ERR):
                        sb, st = b, "WAIT"
                elif st == "WAIT":
                    self._notify("rx", (sb, b))
                    st, sb = "IDLE", 0
        self._notify("disconnected", None)


# ══════════════════════════════════════════════════════════════════════════════
#  APLICAÇÃO
# ══════════════════════════════════════════════════════════════════════════════
class EsteiraApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ESTEIRA SEPARADORA — Sistema de Controle")
        self.resizable(True, True)
        self.minsize(1020, 680)

        self._theme    = "dark"
        self.serial    = SerialManager()
        self.serial.add_callback(self._on_serial_event)
        self.qr_reader = QRCodeReader() if QR_AVAILABLE else None

        # ── Estado câmera ──
        self._cam_active  = False
        self._cam_img_ref = None

        # ── Estado FSM / hardware ──
        self._op_mode    = tk.StringVar(value="FSM")
        self._fsm_id     = tk.IntVar(value=255)
        self._gate       = tk.StringVar(value="FECHADA")
        self._motor_st   = tk.StringVar(value="PARADO")
        self._motor_dir  = tk.StringVar(value="FRENTE")
        self._flash      = tk.StringVar(value="OFF")
        self._waiting_cls = False
        self._qr_pending  = None
        self._qr_raw      = ""

        # ── Controles debug ──
        self._step_count  = tk.IntVar(value=100)
        self._loop_mode   = tk.BooleanVar(value=False)
        self._step_dir    = tk.IntVar(value=0)
        self._loop_run    = False
        self._loop_thread = None

        # ── Opções QR ──
        self._auto_route = tk.BooleanVar(value=True)
        self._cam_index  = tk.IntVar(value=0)

        # ── Recoloração ──
        self._tw_list   = []
        self._btn_list  = []
        self._cbs_list  = []   # checkbuttons

        self._state_boxes = {}

        self._build_fonts()
        self.configure(bg=C["bg"])
        self._build_ui()
        self._refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Fontes ────────────────────────────────────────────────────────────────
    def _build_fonts(self):
        self.f_mono  = tkfont.Font(family="Courier New", size=9)
        self.f_monob = tkfont.Font(family="Courier New", size=9,  weight="bold")
        self.f_sm    = tkfont.Font(family="Courier New", size=8)
        self.f_title = tkfont.Font(family="Courier New", size=11, weight="bold")
        self.f_fsm   = tkfont.Font(family="Courier New", size=13, weight="bold")
        self.f_btn   = tkfont.Font(family="Courier New", size=9,  weight="bold")
        self.f_head  = tkfont.Font(family="Courier New", size=10, weight="bold")
        self.f_tab   = tkfont.Font(family="Courier New", size=10, weight="bold")
        self.f_qrbig = tkfont.Font(family="Courier New", size=17, weight="bold")

    # ── UI raiz ───────────────────────────────────────────────────────────────
    def _build_ui(self):
        self._build_topbar()

        self._container = self._tw(tk.Frame(self), bg="bg")
        self._container.pack(fill="both", expand=True, padx=10, pady=(0,10))

        self._pg_main  = self._tw(tk.Frame(self._container), bg="bg")
        self._pg_debug = self._tw(tk.Frame(self._container), bg="bg")

        self._build_page_main(self._pg_main)
        self._build_page_debug(self._pg_debug)

        self._show_page("main")

    # ── Barra superior ────────────────────────────────────────────────────────
    def _build_topbar(self):
        # Faixa 1: título + tabs + tema
        f1 = self._tw(tk.Frame(self, pady=6), bg="bg")
        f1.pack(fill="x", padx=10)

        self._tw(tk.Label(f1, text="▸ ESTEIRA SEPARADORA", font=self.f_title),
                 bg="bg", fg="accent").pack(side="left")
        self._tw(tk.Label(f1, text="STM32G070  v2.1", font=self.f_sm),
                 bg="bg", fg="text_dim").pack(side="left", padx=10)

        # Tema
        self._btn_theme = tk.Button(
            f1, text="☀", font=self.f_head, width=3,
            bg=C["border"], fg=C["text_dim"], relief="flat",
            cursor="hand2", bd=0,
            command=self._toggle_theme,
            activebackground=C["bg"], activeforeground=C["text_dim"])
        self._btn_theme.pack(side="right", padx=4)
        self._btn_list.append((self._btn_theme, "text_dim"))

        # Tabs
        tabs = self._tw(tk.Frame(f1), bg="bg")
        tabs.pack(side="left", padx=18)
        self._tab_main = tk.Button(
            tabs, text="  APLICAÇÃO PRINCIPAL  ", font=self.f_tab,
            relief="flat", bd=0, cursor="hand2", padx=12, pady=7,
            command=lambda: self._show_page("main"))
        self._tab_main.pack(side="left", padx=(0,3))
        self._tab_debug = tk.Button(
            tabs, text="  MODO DEBUG  ", font=self.f_tab,
            relief="flat", bd=0, cursor="hand2", padx=12, pady=7,
            command=lambda: self._show_page("debug"))
        self._tab_debug.pack(side="left")

        # Faixa 2: conexão serial
        f2 = self._tw(tk.Frame(self, pady=0), bg="border")
        f2.pack(fill="x")
        self._build_conn_bar(f2)

    def _show_page(self, name):
        self._pg_main.pack_forget()
        self._pg_debug.pack_forget()
        if name == "main":
            self._pg_main.pack(fill="both", expand=True)
            self._tab_main.config(bg=C["tab_act_bg"],  fg=C["tab_act_fg"])
            self._tab_debug.config(bg=C["tab_idle_bg"], fg=C["tab_idle_fg"])
        else:
            self._pg_debug.pack(fill="both", expand=True)
            self._tab_debug.config(bg=C["tab_act_bg"],  fg=C["tab_act_fg"])
            self._tab_main.config(bg=C["tab_idle_bg"], fg=C["tab_idle_fg"])

    # ── Barra de conexão ──────────────────────────────────────────────────────
    def _build_conn_bar(self, parent):
        row = self._tw(tk.Frame(parent), bg="border")
        row.pack(fill="x", padx=10, pady=5)

        self._tw(tk.Label(row, text="PORTA:", font=self.f_sm),
                 bg="border", fg="text_dim").pack(side="left", padx=(0,2))
        self._port_var = tk.StringVar()
        self._port_cb  = ttk.Combobox(row, textvariable=self._port_var,
                                       width=16, state="readonly")
        self._port_cb.pack(side="left", padx=4)

        self._tw(tk.Label(row, text="BAUD:", font=self.f_sm),
                 bg="border", fg="text_dim").pack(side="left", padx=(6,2))
        self._baud_var = tk.StringVar(value="115200")
        ttk.Combobox(row, textvariable=self._baud_var, width=7,
                     values=["9600","19200","57600","115200","230400"],
                     state="readonly").pack(side="left", padx=4)

        self._btn(row, "⟳", self._refresh_ports, "text_dim", 3).pack(
            side="left", padx=4)

        self._btn_conn = self._btn(row, "CONECTAR",
                                    self._toggle_connect, "green", 12)
        self._btn_conn.pack(side="left", padx=8)

        self._led = self._tw(tk.Label(row, text="●", font=self.f_head),
                              bg="border", fg="text_dim")
        self._led.pack(side="left", padx=2)
        self._lbl_conn = self._tw(tk.Label(row, text="DESCONECTADO",
                                            font=self.f_sm),
                                   bg="border", fg="text_dim")
        self._lbl_conn.pack(side="left", padx=4)

        self._btn_hs = self._btn(row, "HANDSHAKE",
                                  self._do_handshake, "accent", 12)
        self._btn_hs.pack(side="right", padx=8)
        self._btn_hs.config(state="disabled")

        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TCombobox",
                         fieldbackground=C["border"], background=C["border"],
                         foreground=C["text_head"],
                         selectbackground=C["border"],
                         selectforeground=C["accent"])
        style.configure("Vertical.TScrollbar",
                         background=C["border"], troughcolor=C["bg"],
                         arrowcolor=C["text_dim"])

    # ══════════════════════════════════════════════════════════════════════════
    #  PÁGINA PRINCIPAL: Log de Leitura │ Câmera em Tempo Real
    # ══════════════════════════════════════════════════════════════════════════
    def _build_page_main(self, parent):
        parent.columnconfigure(0, weight=5, minsize=320)
        parent.columnconfigure(1, weight=7, minsize=440)
        parent.rowconfigure(0, weight=1)

        # ── Coluna esquerda ────────────────────────────────────────────────────
        left = self._tw(tk.Frame(parent), bg="bg")
        left.grid(row=0, column=0, sticky="nsew", padx=(0,6), pady=6)
        left.rowconfigure(1, weight=1)
        left.columnconfigure(0, weight=1)

        self._build_main_statusbar(left)

        # Log de leitura — usa pack pois _panel() ja empacota o label de titulo
        lp = self._panel(left, "LOG DE LEITURA")
        lp.grid(row=1, column=0, sticky="nsew", pady=(6, 0))

        foot = self._tw(tk.Frame(lp), bg="panel")
        foot.pack(side="bottom", fill="x", padx=6, pady=(0, 4))
        self._btn(foot, "LIMPAR LOG",
                  lambda: self._clear_txt(self._main_log),
                  "text_dim", 12).pack(side="right")

        txt_frame = self._tw(tk.Frame(lp), bg="panel")
        txt_frame.pack(fill="both", expand=True, padx=6, pady=6)
        txt_frame.rowconfigure(0, weight=1)
        txt_frame.columnconfigure(0, weight=1)

        self._main_log = tk.Text(
            txt_frame, bg=C["bg"], fg=C["text"], font=self.f_mono,
            relief="flat", state="disabled", wrap="none",
            insertbackground=C["accent"])
        self._main_log.grid(row=0, column=0, sticky="nsew")

        sb = ttk.Scrollbar(txt_frame, orient="vertical",
                            command=self._main_log.yview)
        sb.grid(row=0, column=1, sticky="ns")
        self._main_log.config(yscrollcommand=sb.set)
        self._tw_list.append((self._main_log, {"bg": "bg", "fg": "text"}))
        self._apply_main_tags()

        # ── Coluna direita ─────────────────────────────────────────────────────
        right = self._tw(tk.Frame(parent), bg="bg")
        right.grid(row=0, column=1, sticky="nsew", pady=6)
        right.rowconfigure(0, weight=1)
        right.columnconfigure(0, weight=1)

        self._build_cam_panel(right)

    # ── Status strip ──────────────────────────────────────────────────────────
    def _build_main_statusbar(self, parent):
        p = self._panel(parent, "STATUS")
        p.grid(row=0, column=0, sticky="ew")

        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=8, pady=6)
        inn.columnconfigure(1, weight=1)
        inn.columnconfigure(3, weight=1)

        # Linha 0: MODO | FSM
        self._tw(tk.Label(inn, text="MODO:", font=self.f_sm, anchor="w"),
                 bg="panel", fg="text_dim").grid(row=0, column=0, sticky="w")
        self._lbl_modo = self._tw(
            tk.Label(inn, textvariable=self._op_mode, font=self.f_monob, anchor="w"),
            bg="panel", fg="accent")
        self._lbl_modo.grid(row=0, column=1, sticky="w", padx=4)

        self._tw(tk.Label(inn, text="FSM:", font=self.f_sm, anchor="w"),
                 bg="panel", fg="text_dim").grid(row=0, column=2, sticky="w",
                                                  padx=(12,0))
        self._lbl_fsm_main = self._tw(
            tk.Label(inn, text="—", font=self.f_monob, anchor="w"),
            bg="panel", fg="text_dim")
        self._lbl_fsm_main.grid(row=0, column=3, sticky="w", padx=4)

        # Linha 1: último QR
        self._tw(tk.Label(inn, text="ÚLTIMO QR:", font=self.f_sm, anchor="w"),
                 bg="panel", fg="text_dim").grid(row=1, column=0, sticky="w",
                                                  pady=(4,0))
        self._lbl_qr_main = self._tw(
            tk.Label(inn, text="—", font=self.f_monob, anchor="w"),
            bg="panel", fg="text_dim")
        self._lbl_qr_main.grid(row=1, column=1, columnspan=3,
                                sticky="w", padx=4, pady=(4,0))

        # Linha 2: auto-rota
        ar = self._tw(tk.Frame(inn), bg="panel")
        ar.grid(row=2, column=0, columnspan=4, sticky="w", pady=(6,0))
        cb = tk.Checkbutton(
            ar, text="AUTO-ROTEAMENTO  (envia automaticamente ao receber CLSS_REQUEST)",
            variable=self._auto_route,
            bg=C["panel"], fg=C["text"], selectcolor=C["border"],
            activebackground=C["panel"], activeforeground=C["text"],
            font=self.f_sm)
        cb.pack(side="left")
        self._cbs_list.append(cb)

        # Linha 3: botões envio manual
        br = self._tw(tk.Frame(inn), bg="panel")
        br.grid(row=3, column=0, columnspan=4, sticky="w", pady=(4,2))
        self._btn_qrsend = self._btn(br, "⬆ ENVIAR CMD DO QR",
                                      self._send_qr_manual, "accent", 20)
        self._btn_qrsend.pack(side="left")
        self._btn_qrsend.config(state="disabled")
        self._btn_qrclr = self._btn(br, "✕ LIMPAR",
                                     self._clear_qr, "text_dim", 10)
        self._btn_qrclr.pack(side="left", padx=6)

    # ── Painel câmera ─────────────────────────────────────────────────────────
    def _build_cam_panel(self, parent):
        # _panel() already packs its title label, so ALL children must use pack
        p = self._panel(parent, "CÂMERA EM TEMPO REAL")
        p.pack(fill="both", expand=True)

        # ── Barra de controles (pack) ──
        bar = self._tw(tk.Frame(p), bg="panel")
        bar.pack(fill="x", padx=8, pady=6)

        self._tw(tk.Label(bar, text="CÂMERA Nº:", font=self.f_sm),
                 bg="panel", fg="text_dim").pack(side="left")
        tk.Spinbox(bar, from_=0, to=9, textvariable=self._cam_index,
                   width=3, font=self.f_mono,
                   bg=C["border"], fg=C["text_head"],
                   buttonbackground=C["border"],
                   relief="flat").pack(side="left", padx=6)

        self._btn_cam = self._btn(bar, "▶ INICIAR CÂMERA",
                                   self._toggle_camera, "green", 18)
        self._btn_cam.pack(side="left", padx=6)

        self._lbl_cam_st = self._tw(
            tk.Label(bar, text="Câmera inativa", font=self.f_sm),
            bg="panel", fg="text_dim")
        self._lbl_cam_st.pack(side="left", padx=10)

        if not QR_AVAILABLE:
            self._tw(tk.Label(bar, font=self.f_sm,
                               text="⚠ pip install opencv-python Pillow"),
                     bg="panel", fg="yellow").pack(side="right", padx=8)

        # ── Linha de info QR (pack, fica na base) ──
        qr_bar = self._tw(tk.Frame(p), bg="panel")
        qr_bar.pack(side="bottom", fill="x", padx=8, pady=(0, 8))

        self._tw(tk.Label(qr_bar, text="QR BRUTO:", font=self.f_sm),
                 bg="panel", fg="text_dim").pack(side="left")
        self._lbl_qr_raw = self._tw(
            tk.Label(qr_bar, text="—", font=self.f_monob),
            bg="panel", fg="text_dim")
        self._lbl_qr_raw.pack(side="left", padx=6)

        self._tw(tk.Label(qr_bar, text="CMD:", font=self.f_sm),
                 bg="panel", fg="text_dim").pack(side="left", padx=(14, 0))
        self._lbl_qr_cmd = self._tw(
            tk.Label(qr_bar, text="—", font=self.f_qrbig),
            bg="panel", fg="text_dim")
        self._lbl_qr_cmd.pack(side="left", padx=6)

        # ── Canvas expansível (pack, fill+expand entre bar e qr_bar) ──
        self._cam_canvas = tk.Canvas(
            p, bg=C["bg"], highlightthickness=2,
            highlightbackground=C["cam_idle"])
        self._cam_canvas.pack(fill="both", expand=True, padx=8, pady=(0, 4))
        self._tw_list.append((self._cam_canvas, {"bg": "bg"}))
        self._cam_canvas.bind("<Configure>", lambda e: self._draw_placeholder())

    def _draw_placeholder(self):
        self._cam_canvas.update_idletasks()
        w = max(self._cam_canvas.winfo_width(),  10)
        h = max(self._cam_canvas.winfo_height(), 10)
        self._cam_canvas.delete("all")
        self._cam_canvas.create_rectangle(0, 0, w, h,
                                           fill=C["bg"], outline="")
        for x in range(0, w, 40):
            self._cam_canvas.create_line(x, 0, x, h,
                                          fill=C["border"], width=1)
        for y in range(0, h, 40):
            self._cam_canvas.create_line(0, y, w, y,
                                          fill=C["border"], width=1)
        cx, cy, sz = w//2, h//2, 52
        self._cam_canvas.create_rectangle(
            cx-sz, cy-sz, cx+sz, cy+sz,
            outline=C["text_dim"], width=2)
        for ox, oy in [(-sz+6,-sz+6), (sz//3,-sz+6), (-sz+6, sz//3)]:
            s = sz//3-4
            self._cam_canvas.create_rectangle(
                cx+ox, cy+oy, cx+ox+s, cy+oy+s,
                outline=C["text_dim"], width=2)
        self._cam_canvas.create_text(
            cx, cy+sz+24,
            fill=C["text_dim"], font=("Courier New",10),
            text="Câmera inativa — pressione  ▶ INICIAR CÂMERA")

    # ── Câmera: toggle, callbacks ──────────────────────────────────────────────
    def _toggle_camera(self):
        if not QR_AVAILABLE:
            messagebox.showerror("Dependências ausentes",
                                  "Instale: pip install opencv-python Pillow")
            return
        if self._cam_active:
            self.qr_reader.stop()
            self._cam_active = False
            self._btn_cam.config(text="▶ INICIAR CÂMERA", fg=C["green"])
            self._lbl_cam_st.config(text="Câmera inativa", fg=C["text_dim"])
            self._cam_canvas.config(highlightbackground=C["cam_idle"])
            self._draw_placeholder()
            self._mlog("sys", "Câmera desligada.")
        else:
            self.qr_reader.on_frame    = lambda f: self.after(0, self._upd_cam, f)
            self.qr_reader.on_detected = lambda r,c: self.after(0, self._on_qr, r, c)
            self.qr_reader.on_lost     = lambda: self.after(0, self._cam_lost)
            ok = self.qr_reader.start(self._cam_index.get())
            if ok:
                self._cam_active = True
                self._btn_cam.config(text="■ PARAR CÂMERA", fg=C["red"])
                self._lbl_cam_st.config(text="Ativa — buscando QR...",
                                         fg=C["yellow"])
                self._cam_canvas.config(highlightbackground=C["cam_scan"])
                self._mlog("sys", f"Câmera iniciada (índice {self._cam_index.get()}).")
            else:
                messagebox.showerror(
                    "Erro de câmera",
                    f"Não foi possível abrir câmera {self._cam_index.get()}.")

    def _upd_cam(self, rgb):
        cw = self._cam_canvas.winfo_width()
        ch = self._cam_canvas.winfo_height()
        if cw < 10 or ch < 10:
            return
        img = Image.fromarray(rgb).resize((cw, ch), Image.LANCZOS)
        self._cam_img_ref = ImageTk.PhotoImage(img)
        self._cam_canvas.create_image(0, 0, anchor="nw",
                                       image=self._cam_img_ref)

    def _on_qr(self, raw: str, cmd: int):
        self._qr_pending = cmd
        self._qr_raw     = raw
        label = CMD_LABELS.get(cmd, f"0x{cmd:02X}")
        color = QR_CMD_COLOR.get(cmd, "text_dim")

        self._lbl_qr_raw.config(text=raw, fg=C["accent"])
        self._lbl_qr_cmd.config(text=f"{label}  (0x{cmd:02X})", fg=C[color])
        self._lbl_cam_st.config(text=f"✓  {label}", fg=C["green"])
        self._cam_canvas.config(highlightbackground=C["cam_found"])
        self._lbl_qr_main.config(
            text=f"{raw}  →  {label} (0x{cmd:02X})", fg=C[color])
        self._btn_qrsend.config(state="normal")

        self._mlog("qr", f"QR LIDO: '{raw}'  →  CMD=0x{cmd:02X}  [{label}]")

        # Auto-roteamento
        if self._auto_route.get():
            if self._waiting_cls and cmd in (ROUTE_A_RECV, ROUTE_B_RECV):
                self._waiting_cls = False
                self._mlog("qr", f"AUTO-ROTA → 0x{cmd:02X} [{label}]")
                self._send(cmd)
            elif self._op_mode.get() == "DEBUG":
                self._mlog("qr", f"AUTO-DEBUG → 0x{cmd:02X} [{label}]")
                self._send(cmd)

        self.after(3000, self._qr_scan_reset)

    def _qr_scan_reset(self):
        if self._cam_active:
            self._lbl_cam_st.config(text="Ativa — buscando QR...",
                                     fg=C["yellow"])
            self._cam_canvas.config(highlightbackground=C["cam_scan"])

    def _cam_lost(self):
        self._cam_active = False
        self._btn_cam.config(text="▶ INICIAR CÂMERA", fg=C["green"])
        self._lbl_cam_st.config(text="⚠ Câmera desconectada!", fg=C["red"])
        self._cam_canvas.config(highlightbackground=C["cam_idle"])
        self._draw_placeholder()
        self._mlog("sys", "⚠ Câmera perdeu conexão.")

    def _send_qr_manual(self):
        if self._qr_pending is None:
            return
        label = CMD_LABELS.get(self._qr_pending, f"0x{self._qr_pending:02X}")
        self._mlog("qr", f"ENVIO MANUAL: 0x{self._qr_pending:02X} [{label}]")
        self._send(self._qr_pending)

    def _clear_qr(self):
        self._qr_pending = None
        self._qr_raw     = ""
        for lbl in (self._lbl_qr_raw, self._lbl_qr_cmd, self._lbl_qr_main):
            lbl.config(text="—", fg=C["text_dim"])
        self._btn_qrsend.config(state="disabled")

    # ══════════════════════════════════════════════════════════════════════════
    #  PÁGINA DEBUG
    # ══════════════════════════════════════════════════════════════════════════
    def _build_page_debug(self, parent):
        parent.columnconfigure(0, weight=1, minsize=230)
        parent.columnconfigure(1, weight=1, minsize=290)
        parent.columnconfigure(2, weight=2, minsize=360)
        parent.rowconfigure(0, weight=1)

        ca = self._tw(tk.Frame(parent), bg="bg")
        ca.grid(row=0, column=0, sticky="nsew", padx=(0,6), pady=6)
        self._build_hw_status(ca)
        self._build_fsm_panel(ca)
        self._build_fsm_cmds(ca)

        cb = self._tw(tk.Frame(parent), bg="bg")
        cb.grid(row=0, column=1, sticky="nsew", padx=(0,6), pady=6)
        self._build_async_ctrl(cb)

        cc = self._tw(tk.Frame(parent), bg="bg")
        cc.grid(row=0, column=2, sticky="nsew", pady=6)
        self._build_uart_log(cc)

    # ── Status HW ─────────────────────────────────────────────────────────────
    def _build_hw_status(self, parent):
        p = self._panel(parent, "STATUS DO HARDWARE")
        p.pack(fill="x", pady=(4,6))
        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=8, pady=6)

        rows = [("MODO",   self._op_mode),  ("CANCELA", self._gate),
                ("MOTOR",  self._motor_st), ("DIREÇÃO", self._motor_dir),
                ("FLASH",  self._flash)]
        self._hw = {}
        for i, (n, v) in enumerate(rows):
            self._tw(tk.Label(inn, text=f"{n}:", font=self.f_sm,
                               anchor="w", width=9),
                     bg="panel", fg="text_dim").grid(row=i, column=0,
                                                      sticky="w", pady=2)
            lbl = self._tw(tk.Label(inn, textvariable=v,
                                     font=self.f_monob, anchor="w"),
                           bg="panel", fg="accent")
            lbl.grid(row=i, column=1, sticky="w", padx=4, pady=2)
            self._hw[n] = lbl
        self._upd_hw_colors()

    def _upd_hw_colors(self):
        if not hasattr(self, "_hw"):
            return
        self._hw["MODO"].config(
            fg=C["accent"] if self._op_mode.get()=="FSM" else C["accent2"])
        self._hw["CANCELA"].config(
            fg=C["green"] if self._gate.get()=="ABERTA" else C["red"])
        self._hw["MOTOR"].config(
            fg=C["green"] if self._motor_st.get()=="GIRANDO" else C["text_dim"])
        self._hw["FLASH"].config(
            fg=C["yellow"] if self._flash.get()=="ON" else C["text_dim"])
        self._hw["DIREÇÃO"].config(fg=C["text"])
        if hasattr(self, "_lbl_modo"):
            self._lbl_modo.config(
                fg=C["accent"] if self._op_mode.get()=="FSM" else C["accent2"])

    # ── FSM ───────────────────────────────────────────────────────────────────
    def _build_fsm_panel(self, parent):
        p = self._panel(parent, "MÁQUINA DE ESTADOS")
        p.pack(fill="x", pady=(0,6))
        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=6, pady=8)

        self._lbl_fsm_debug = self._tw(
            tk.Label(inn, text="—", font=self.f_fsm, anchor="center"),
            bg="panel", fg="text_dim")
        self._lbl_fsm_debug.pack(fill="x")

        pipe = self._tw(tk.Frame(inn), bg="panel")
        pipe.pack(fill="x", pady=(8,0))
        names = ["IDLE","OBJ\nDET","WAIT\nCLSS","ROUTE\nA","ROUTE\nB"]
        for i, (sid, sname) in enumerate(zip([0,1,2,3,4], names)):
            col = self._tw(tk.Frame(pipe), bg="panel")
            col.pack(side="left", expand=True, fill="x")
            box = tk.Label(col, text=sname, bg=C["border"], fg=C["text_dim"],
                            font=self.f_sm, relief="flat",
                            padx=3, pady=3, justify="center")
            box.pack(fill="x", padx=1)
            self._state_boxes[sid] = box
            if i < 4:
                self._tw(tk.Label(pipe, text="▸", font=self.f_sm),
                         bg="panel", fg="text_dim").pack(side="left")

    def _refresh_fsm(self, sid):
        name, ck = FSM_STATE_DEFS.get(sid, ("?","text_dim"))
        if hasattr(self, "_lbl_fsm_debug"):
            self._lbl_fsm_debug.config(text=name, fg=C[ck])
        if hasattr(self, "_lbl_fsm_main"):
            self._lbl_fsm_main.config(text=name, fg=C[ck])
        for s, box in self._state_boxes.items():
            if s == sid:
                _, bck = FSM_STATE_DEFS.get(s, ("?","text_dim"))
                box.config(bg=C[bck], fg=C["state_box_active_fg"])
            else:
                box.config(bg=C["border"], fg=C["text_dim"])

    # ── Comandos FSM ──────────────────────────────────────────────────────────
    def _build_fsm_cmds(self, parent):
        p = self._panel(parent, "COMANDOS FSM")
        p.pack(fill="x", pady=(0,6))
        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=6, pady=6)

        r1 = self._tw(tk.Frame(inn), bg="panel")
        r1.pack(fill="x", pady=2)
        self._btn(r1, "→ ROTA A", lambda: self._send(ROUTE_A_RECV),
                  "green", 14).pack(side="left", padx=4)
        self._btn(r1, "→ ROTA B", lambda: self._send(ROUTE_B_RECV),
                  "accent2", 14).pack(side="left", padx=4)

        r2 = self._tw(tk.Frame(inn), bg="panel")
        r2.pack(fill="x", pady=2)
        self._btn(r2, "⇄ TOGGLE DEBUG",
                  lambda: self._send(DEBUG_TOGGLE),
                  "yellow", 30).pack(side="left", padx=4)

    # ── Controles assíncronos ─────────────────────────────────────────────────
    def _build_async_ctrl(self, parent):
        p = self._panel(parent, "CONTROLE ASSÍNCRONO  (MODO DEBUG)")
        p.pack(fill="both", expand=True, pady=4)
        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=6, pady=6)

        def row():
            f = self._tw(tk.Frame(inn), bg="panel")
            f.pack(fill="x", pady=3)
            return f

        def lbl(f, t, w=9):
            self._tw(tk.Label(f, text=t, font=self.f_sm, width=w, anchor="w"),
                     bg="panel", fg="text_dim").pack(side="left")

        r = row(); lbl(r,"FLASH:")
        self._btn(r,"ON",  lambda:self._send(LIGHT_EN), "yellow",6).pack(side="left",padx=3)
        self._btn(r,"OFF", lambda:self._send(LIGHT_DIS),"text_dim",6).pack(side="left",padx=3)

        r = row(); lbl(r,"CANCELA:")
        self._btn(r,"ABRIR",  lambda:self._send(GATE_OPEN), "green",7).pack(side="left",padx=3)
        self._btn(r,"FECHAR", lambda:self._send(GATE_CLOSE),"red",7).pack(side="left",padx=3)

        r = row(); lbl(r,"MOTOR:")
        self._btn(r,"ENGAJAR",lambda:self._send(STPR_EN), "green",10).pack(side="left",padx=3)
        self._btn(r,"LIVRE",  lambda:self._send(STPR_DIS),"red",10).pack(side="left",padx=3)

        def fwd():
            self._step_dir.set(0); self._motor_dir.set("FRENTE")
            self._send(STPR_FORWARD); self._upd_hw_colors()
        def bwd():
            self._step_dir.set(1); self._motor_dir.set("TRÁS")
            self._send(STPR_BACKWARD); self._upd_hw_colors()

        r = row(); lbl(r,"DIREÇÃO:")
        self._btn(r,"◀ FRENTE",fwd,"accent",10).pack(side="left",padx=3)
        self._btn(r,"TRÁS ▶",  bwd,"accent",10).pack(side="left",padx=3)

        self._tw(tk.Frame(inn,height=1),bg="border").pack(fill="x",pady=8)
        self._tw(tk.Label(inn,text="CONTROLE DE PASSOS",font=self.f_sm),
                 bg="panel",fg="accent").pack(anchor="w")

        r = row()
        self._tw(tk.Label(r,text="Nº PASSOS:",font=self.f_sm),
                 bg="panel",fg="text_dim").pack(side="left",padx=(0,4))
        self._spin = tk.Spinbox(
            r, from_=1, to=255, textvariable=self._step_count,
            width=6, font=self.f_mono,
            bg=C["border"], fg=C["text_head"],
            buttonbackground=C["border"], relief="flat",
            insertbackground=C["accent"])
        self._spin.pack(side="left",padx=4)
        self._tw_list.append((self._spin,
                               {"bg":"spinbox_bg","fg":"text_head",
                                "buttonbackground":"border",
                                "insertbackground":"accent"}))

        r = row()
        cb = tk.Checkbutton(r, text="MODO LOOP",
                             variable=self._loop_mode,
                             bg=C["panel"], fg=C["text"],
                             selectcolor=C["border"],
                             activebackground=C["panel"],
                             activeforeground=C["text"],
                             font=self.f_sm,
                             command=lambda: self._stop_loop()
                             if not self._loop_mode.get() else None)
        cb.pack(side="left")
        self._cbs_list.append(cb)

        r = row()
        self._btn_steps = self._btn(r,"▶ ENVIAR PASSOS",
                                     self._send_steps,"green",18)
        self._btn_steps.pack(side="left",padx=3)
        self._btn_lstop = self._btn(r,"■ PARAR",self._stop_loop,"red",8)
        self._btn_lstop.pack(side="left",padx=3)
        self._btn_lstop.config(state="disabled")

        self._lbl_loop = self._tw(tk.Label(inn,text="",font=self.f_sm),
                                   bg="panel",fg="yellow")
        self._lbl_loop.pack(anchor="w",pady=2)

    # ── Log UART ──────────────────────────────────────────────────────────────
    def _build_uart_log(self, parent):
        p = self._panel(parent, "LOG DE COMUNICAÇÃO UART")
        p.pack(fill="both", expand=True, pady=4)

        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="both", expand=True, padx=6, pady=6)
        inn.rowconfigure(0, weight=1)
        inn.columnconfigure(0, weight=1)

        self._uart_log = tk.Text(
            inn, bg=C["bg"], fg=C["text"], font=self.f_mono,
            relief="flat", state="disabled", wrap="none",
            insertbackground=C["accent"])
        self._uart_log.grid(row=0, column=0, sticky="nsew")

        sb_y = ttk.Scrollbar(inn, orient="vertical",
                              command=self._uart_log.yview)
        sb_y.grid(row=0, column=1, sticky="ns")
        sb_x = ttk.Scrollbar(inn, orient="horizontal",
                              command=self._uart_log.xview)
        sb_x.grid(row=1, column=0, sticky="ew")
        self._uart_log.config(yscrollcommand=sb_y.set,
                               xscrollcommand=sb_x.set)
        self._tw_list.append((self._uart_log, {"bg":"bg","fg":"text"}))
        self._apply_uart_tags()

        foot = self._tw(tk.Frame(p), bg="panel")
        foot.pack(fill="x", padx=6, pady=(0,4))
        self._btn(foot, "LIMPAR LOG",
                  lambda: self._clear_txt(self._uart_log),
                  "text_dim", 12).pack(side="right")

    # ── Log helpers ───────────────────────────────────────────────────────────
    def _apply_main_tags(self):
        t = self._main_log
        t.tag_config("qr",  foreground=C["log_qr"])
        t.tag_config("fsm", foreground=C["log_rx"])
        t.tag_config("sys", foreground=C["log_sys"])
        t.tag_config("err", foreground=C["err"])
        t.config(bg=C["bg"], fg=C["text"])

    def _apply_uart_tags(self):
        t = self._uart_log
        t.tag_config("tx",  foreground=C["log_tx"])
        t.tag_config("rx",  foreground=C["log_rx"])
        t.tag_config("sys", foreground=C["log_sys"])
        t.tag_config("err", foreground=C["err"])
        t.config(bg=C["bg"], fg=C["text"])

    def _write(self, widget, line, tag):
        widget.config(state="normal")
        widget.insert("end", line, tag)
        widget.see("end")
        widget.config(state="disabled")

    def _clear_txt(self, w):
        w.config(state="normal")
        w.delete("1.0","end")
        w.config(state="disabled")

    def _mlog(self, tag, msg):
        prefixes = {"qr":"QR  ","fsm":"FSM ","sys":"··· ","err":"ERR "}
        self._write(self._main_log,
                    f"[{now_ts()}] {prefixes.get(tag,'    ')}{msg}\n", tag)

    def _ulog(self, tag, msg):
        self._write(self._uart_log,
                    f"[{now_ts()}] {msg}\n", tag)

    # ── Callbacks seriais ─────────────────────────────────────────────────────
    def _on_serial_event(self, ev, data):
        if ev == "rx":
            self.after(0, self._handle_rx, data)
        elif ev == "disconnected":
            self.after(0, self._disconnected)

    def _handle_rx(self, data):
        status, payload = data
        label = CMD_LABELS.get(payload, f"0x{payload:02X}")
        ok    = status == CMD_OK

        if not ok:
            self._ulog("err", f"RX   0x{status:02X}  CMD_ERR")
            return

        self._ulog("rx", f"RX   0x{payload:02X}  {label+' (eco)':<24} [OK]")

        # FSM
        if payload == OBJ_DETECTED:
            self._fsm_id.set(1); self._refresh_fsm(1)
            self._waiting_cls = False
            self._mlog("fsm","Objeto detectado — Sensor 1")
            self._clear_qr()

        elif payload == CLSS_REQUEST:
            self._fsm_id.set(2); self._refresh_fsm(2)
            self._waiting_cls = True
            self._mlog("fsm","CLSS_REQUEST — aguardando rota")
            if (self._auto_route.get()
                    and self._qr_pending in (ROUTE_A_RECV, ROUTE_B_RECV)):
                lbl = CMD_LABELS.get(self._qr_pending,"?")
                self._mlog("qr",
                    f"AUTO-ROTA (QR pré-lido) → 0x{self._qr_pending:02X} [{lbl}]")
                self._send(self._qr_pending)
                self._waiting_cls = False

        elif payload == ROUTE_A_FWD:
            self._gate.set("FECHADA"); self._fsm_id.set(3)
            self._refresh_fsm(3); self._waiting_cls = False
            self._mlog("fsm","Encaminhando → ROTA A")

        elif payload == ROUTE_B_FWD:
            self._gate.set("ABERTA"); self._fsm_id.set(4)
            self._refresh_fsm(4); self._waiting_cls = False
            self._mlog("fsm","Encaminhando → ROTA B")

        elif payload in (ROUTE_A_OK, ROUTE_B_OK):
            r = "A" if payload == ROUTE_A_OK else "B"
            self._fsm_id.set(0); self._refresh_fsm(0)
            self._mlog("fsm", f"✓ Entrega confirmada — ROTA {r}")

        elif payload == SYS_INIT_MSG:
            msg = "✓ HANDSHAKE OK — sistema inicializado"
            self._mlog("sys", msg); self._ulog("sys", msg)

        elif payload == DEBUG_TOGGLE:
            new = "DEBUG" if self._op_mode.get() == "FSM" else "FSM"
            self._op_mode.set(new)
            self._mlog("sys", f"Modo alternado → {new}")
            self._ulog("sys", f"Modo alternado → {new}")

        elif payload == GATE_OPEN:   self._gate.set("ABERTA")
        elif payload == GATE_CLOSE:  self._gate.set("FECHADA")
        elif payload == LIGHT_EN:    self._flash.set("ON")
        elif payload == LIGHT_DIS:   self._flash.set("OFF")
        elif payload == STPR_EN:     self._motor_st.set("GIRANDO")
        elif payload == STPR_DIS:    self._motor_st.set("LIVRE")
        elif payload == STPR_FORWARD:self._motor_dir.set("FRENTE")
        elif payload == STPR_BACKWARD:self._motor_dir.set("TRÁS")

        self._upd_hw_colors()

    def _disconnected(self):
        self._led.config(fg=C["red"])
        self._lbl_conn.config(text="CONEXÃO PERDIDA", fg=C["red"])
        self._btn_conn.config(text="CONECTAR", fg=C["green"])
        self._btn_hs.config(state="disabled")
        self._mlog("sys","⚠ Conexão UART encerrada inesperadamente")
        self._ulog("sys","⚠ Conexão encerrada")

    # ── Ações UART ────────────────────────────────────────────────────────────
    def _send(self, cmd, data=None):
        if not self.serial.connected:
            self._mlog("sys","⚠ Não conectado à UART")
            return
        self.serial.send(cmd, data)
        label = CMD_LABELS.get(cmd, f"0x{cmd:02X}")
        extra = f" DATA=0x{data:02X}" if data is not None else ""
        self._ulog("tx", f"TX   0x{cmd:02X}  {label:<24}{extra}")

    def _refresh_ports(self):
        ports = self.serial.list_ports()
        self._port_cb["values"] = ports
        if ports:
            self._port_var.set(ports[0])

    def _toggle_connect(self):
        if self.serial.connected:
            self._stop_loop()
            self.serial.disconnect()
            self._led.config(fg=C["text_dim"])
            self._lbl_conn.config(text="DESCONECTADO", fg=C["text_dim"])
            self._btn_conn.config(text="CONECTAR",     fg=C["green"])
            self._btn_hs.config(state="disabled")
            self._mlog("sys","UART desconectada")
        else:
            port   = self._port_var.get()
            baud   = int(self._baud_var.get())
            result = self.serial.connect(port, baud)
            if result is True:
                self._led.config(fg=C["green"])
                self._lbl_conn.config(text=f"CONECTADO  {port}", fg=C["green"])
                self._btn_conn.config(text="DESCONECTAR", fg=C["red"])
                self._btn_hs.config(state="normal")
                self._mlog("sys",f"UART conectada: {port} @ {baud}")
                self._ulog("sys",f"Conectado: {port} @ {baud} baud")
            else:
                messagebox.showerror("Erro de conexão", str(result))

    def _do_handshake(self):
        self._send(SYS_RDY_MSG)
        self._mlog("sys","Handshake enviado — aguardando SYS_INIT...")

    # ── Motor de passo ────────────────────────────────────────────────────────
    def _send_steps(self):
        steps = self._step_count.get()
        if self._loop_mode.get():
            self._start_loop(steps)
        else:
            self._send(STPR_TGT_STPS, steps)

    def _start_loop(self, steps):
        if self._loop_run:
            return
        self._loop_run = True
        self._btn_steps.config(state="disabled")
        self._btn_lstop.config(state="normal")
        self._lbl_loop.config(
            text=f"● LOOP  {steps} passos  ({int(1/LOOP_INTERVAL)} cmd/s)")
        def loop():
            while self._loop_run:
                if self.serial.connected:
                    self.serial.send(STPR_TGT_STPS, steps)
                time.sleep(LOOP_INTERVAL)
        self._loop_thread = threading.Thread(target=loop, daemon=True)
        self._loop_thread.start()

    def _stop_loop(self):
        self._loop_run = False
        if hasattr(self,"_btn_steps"):  self._btn_steps.config(state="normal")
        if hasattr(self,"_btn_lstop"):  self._btn_lstop.config(state="disabled")
        if hasattr(self,"_lbl_loop"):   self._lbl_loop.config(text="")

    # ── Tema ──────────────────────────────────────────────────────────────────
    def _toggle_theme(self):
        global C
        self._theme = "light" if self._theme == "dark" else "dark"
        C = dict(THEMES[self._theme])
        self.configure(bg=C["bg"])
        self._recolor_all()
        self._btn_theme.config(
            text="☀" if self._theme=="dark" else "◑")
        self._apply_main_tags()
        self._apply_uart_tags()
        active = "main" if self._pg_main.winfo_ismapped() else "debug"
        self._show_page(active)
        self._refresh_fsm(self._fsm_id.get())
        self._upd_hw_colors()

    def _recolor_all(self):
        style = ttk.Style()
        style.configure("TCombobox",
                         fieldbackground=C["border"], background=C["border"],
                         foreground=C["text_head"],
                         selectbackground=C["border"],
                         selectforeground=C["accent"])
        style.configure("Vertical.TScrollbar",
                         background=C["border"], troughcolor=C["bg"],
                         arrowcolor=C["text_dim"])
        for widget, props in self._tw_list:
            try:
                widget.config(**{k: C[v] for k, v in props.items()})
            except tk.TclError:
                pass
        for btn, ck in self._btn_list:
            try:
                btn.config(bg=C["border"], fg=C[ck],
                           activebackground=C["bg"],
                           activeforeground=C[ck])
            except tk.TclError:
                pass
        for cb in self._cbs_list:
            try:
                cb.config(bg=C["panel"], fg=C["text"],
                          selectcolor=C["border"],
                          activebackground=C["panel"],
                          activeforeground=C["text"])
            except tk.TclError:
                pass
        if hasattr(self,"_cam_canvas"):
            bdr = C["cam_found"] if self._cam_active else C["cam_idle"]
            self._cam_canvas.config(highlightbackground=bdr)
        self._btn_theme.config(bg=C["border"], fg=C["text_dim"],
                                activebackground=C["bg"],
                                activeforeground=C["text_dim"])

    # ── Helpers UI ────────────────────────────────────────────────────────────
    def _panel(self, parent, label=""):
        f = tk.Frame(parent, bg=C["panel"], bd=1, relief="flat",
                      highlightbackground=C["border"], highlightthickness=1)
        self._tw_list.append((f,{"bg":"panel","highlightbackground":"border"}))
        if label:
            l = tk.Label(f, text=f" {label} ", bg=C["border"],
                          fg=C["text_dim"], font=self.f_sm, padx=6, pady=2)
            l.pack(anchor="nw", fill="x")
            self._tw_list.append((l,{"bg":"border","fg":"text_dim"}))
        return f

    def _btn(self, parent, text, cmd, color="text", w=10):
        b = tk.Button(parent, text=text, command=cmd,
                       bg=C["border"], fg=C[color], font=self.f_btn,
                       relief="flat", width=w, cursor="hand2",
                       activebackground=C["bg"], activeforeground=C[color],
                       bd=0, padx=4, pady=4)
        b.bind("<Enter>", lambda e, bt=b: bt.config(bg=C["bg"]))
        b.bind("<Leave>", lambda e, bt=b: bt.config(bg=C["border"]))
        self._btn_list.append((b, color))
        return b

    def _tw(self, widget, **props):
        self._tw_list.append((widget, props))
        return widget

    # ── Fechamento ────────────────────────────────────────────────────────────
    def _on_close(self):
        self._stop_loop()
        if self._cam_active and self.qr_reader:
            self.qr_reader.stop()
        if self.serial.connected:
            self.serial.disconnect()
        self.destroy()


# ── ENTRY POINT ───────────────────────────────────────────────────────────────
if __name__ == "__main__":
    if not QR_AVAILABLE:
        print("=" * 58)
        print("AVISO: Dependências de câmera/QR não encontradas.")
        print("Instale com:  pip install opencv-python Pillow")
        print("A GUI funcionará sem câmera (MODO DEBUG disponível).")
        print("=" * 58)
    app = EsteiraApp()
    app.mainloop()