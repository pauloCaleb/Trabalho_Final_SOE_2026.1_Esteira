"""
+=================================================================+
|   ESTEIRA SEPARADORA -- SISTEMA DE CONTROLE v4.0               |
|   STM32G070 <-> Raspberry Pi 3B  |  Firmware v3.0  |  SOE 2026 |
+=================================================================+

Dependencias:
    pip install pyserial opencv-python Pillow

Duas abas:
  APLICACAO  -- camera QR + FSM automatica
  DEBUG      -- controle assincrono completo + sensores

Regras de log:
  - Cada aba tem seu proprio log independente
  - Eventos UART so alimentam o log da aba ATIVA
  - Ao trocar de aba, o usuario e perguntado se deseja salvar o log
  - Arquivos: logFSM_NNN.txt  e  logDBG_NNN.txt
    onde NNN e a contagem de saves na sessao (001, 002, ...)
"""

import re
import threading
import queue
import time
import os
from datetime import datetime

import tkinter as tk
from tkinter import ttk, font as tkfont, messagebox

try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except Exception:
    SERIAL_AVAILABLE = False

try:
    import cv2
    CV2_AVAILABLE = True
except Exception:
    CV2_AVAILABLE = False

try:
    from PIL import Image, ImageTk
    PIL_AVAILABLE = True
except Exception:
    PIL_AVAILABLE = False

QR_AVAILABLE = CV2_AVAILABLE and PIL_AVAILABLE

# =============================================================================
#  PROTOCOLO COMPLETO (firmware v3.0)
# =============================================================================
START_FRAME      = 0xAA
CMD_OK           = 0x90
CMD_ERR          = 0x91
# Handshake
SYS_RDY          = 0x10
SYS_INIT         = 0x01
# FSM RX (GUI -> STM32)
ROUTE_A_SEND     = 0xDA
ROUTE_B_SEND     = 0xDB
# FSM TX (STM32 -> GUI)
OBJ_DETECTED     = 0xA0
CLSS_REQUEST     = 0xC0
ROUTE_A_FWD      = 0xFA
ROUTE_B_FWD      = 0xFB
ROUTE_A_OK       = 0xBA
ROUTE_B_OK       = 0xBB
# Controle assincrono RX
LIGHT_EN         = 0xE1
LIGHT_DIS        = 0xD1
GATE_OPEN        = 0xE2
GATE_CLOSE       = 0xD2
STPR_EN          = 0xE3
STPR_DIS         = 0xD3
STPR_FORWARD     = 0xE4
STPR_BACKWARD    = 0xD4
STPR_TGT_STPS    = 0xE5
# Modo
DEBUG_TOGGLE     = 0xDD
MODE_FSM_MSG     = 0x11
MODE_DEBUG_MSG   = 0x22
# v3.0
SW_RESET_MSG     = 0x33
SENS_STATUS_MSG  = 0x55

RX_NAMES = {
    SYS_INIT:        "SYS_INIT       (0x01)",
    OBJ_DETECTED:    "OBJ_DETECTED   (0xA0)",
    CLSS_REQUEST:    "CLSS_REQUEST   (0xC0)",
    ROUTE_A_FWD:     "ROUTE_A_FWD    (0xFA)",
    ROUTE_B_FWD:     "ROUTE_B_FWD    (0xFB)",
    ROUTE_A_OK:      "ROUTE_A_OK     (0xBA)",
    ROUTE_B_OK:      "ROUTE_B_OK     (0xBB)",
    MODE_FSM_MSG:    "MODE_FSM       (0x11)",
    MODE_DEBUG_MSG:  "MODE_DEBUG     (0x22)",
    SENS_STATUS_MSG: "SENS_STATUS    (0x55)",
    LIGHT_EN:        "LIGHT_EN       (0xE1 eco)",
    LIGHT_DIS:       "LIGHT_DIS      (0xD1 eco)",
    GATE_OPEN:       "GATE_OPEN      (0xE2 eco)",
    GATE_CLOSE:      "GATE_CLOSE     (0xD2 eco)",
    STPR_EN:         "STPR_EN        (0xE3 eco)",
    STPR_DIS:        "STPR_DIS       (0xD3 eco)",
    STPR_FORWARD:    "STPR_FORWARD   (0xE4 eco)",
    STPR_BACKWARD:   "STPR_BACKWARD  (0xD4 eco)",
    STPR_TGT_STPS:   "STPR_TGT_STPS  (0xE5 eco)",
    DEBUG_TOGGLE:    "DEBUG_TOGGLE   (0xDD eco)",
    ROUTE_A_SEND:    "ROUTE_A        (0xDA eco)",
    ROUTE_B_SEND:    "ROUTE_B        (0xDB eco)",
    SW_RESET_MSG:    "SW_RESET       (0x33 eco)",
}
TX_NAMES = {
    SYS_RDY:      "SYS_RDY        (0x10)",
    ROUTE_A_SEND: "ROUTE_A        (0xDA)",
    ROUTE_B_SEND: "ROUTE_B        (0xDB)",
    LIGHT_EN:     "LIGHT_EN       (0xE1)",
    LIGHT_DIS:    "LIGHT_DIS      (0xD1)",
    GATE_OPEN:    "GATE_OPEN      (0xE2)",
    GATE_CLOSE:   "GATE_CLOSE     (0xD2)",
    STPR_EN:      "STPR_EN        (0xE3)",
    STPR_DIS:     "STPR_DIS       (0xD3)",
    STPR_FORWARD: "STPR_FORWARD   (0xE4)",
    STPR_BACKWARD:"STPR_BACKWARD  (0xD4)",
    STPR_TGT_STPS:"STPR_TGT_STPS  (0xE5)",
    DEBUG_TOGGLE: "DEBUG_TOGGLE   (0xDD)",
    SW_RESET_MSG: "SW_RESET       (0x33)",
}

# =============================================================================
#  TEMAS  (cores dark corrigidas)
# =============================================================================
THEMES = {
    "dark": {
        "bg":                  "#0D0F14",
        "panel":               "#13161E",
        "border":              "#1E2330",
        "accent":              "#00D4FF",
        "accent2":             "#FF6B35",
        "green":               "#00D97A",
        "red":                 "#FF3B5C",
        "yellow":              "#FFD700",
        "text":                "#05348A",
        "text_dim":            "#0850CD",
        "text_head":           "#16181D",
        "log_tx":              "#38BDF8",
        "log_rx":              "#34D399",
        "log_sys":             "#FBBF24",
        "log_qr":              "#C084FC",
        "log_warn":            "#FFD700",
        "log_err":             "#FF3B5C",
        "spinbox_bg":          "#1E2330",
        "tab_act_bg":          "#00D4FF",
        "tab_act_fg":          "#0D0F14",
        "tab_idle_bg":         "#1E2330",
        "tab_idle_fg":         "#4A5568",
        "cam_idle":            "#1E2330",
        "cam_scan":            "#FFD700",
        "cam_found":           "#00D97A",
        "fsm_idle":            "#4A5568",
        "fsm_obj":             "#886600",
        "fsm_clss":            "#005580",
        "fsm_route":           "#005533",
        "fsm_done":            "#4A5568",
        "state_box_active_fg": "#E8EDF5",
        "sens_active":         "#00D97A",
        "sens_inactive":       "#1E2330",
        "sens_active_fg":      "#0D0F14",
        "sens_inactive_fg":    "#4A5568",
    },
    "light": {
        "bg":                  "#EEF1F7",
        "panel":               "#FFFFFF",
        "border":              "#C8D3E6",
        "accent":              "#0077AA",
        "accent2":             "#C04400",
        "green":               "#006B44",
        "red":                 "#BB1133",
        "yellow":              "#886600",
        "text":                "#2D3748",
        "text_dim":            "#7A8899",
        "text_head":           "#111827",
        "log_tx":              "#005F99",
        "log_rx":              "#005533",
        "log_sys":             "#775500",
        "log_qr":              "#7C3AED",
        "log_warn":            "#886600",
        "log_err":             "#BB1133",
        "spinbox_bg":          "#E4EAF4",
        "tab_act_bg":          "#0077AA",
        "tab_act_fg":          "#FFFFFF",
        "tab_idle_bg":         "#C8D3E6",
        "tab_idle_fg":         "#7A8899",
        "cam_idle":            "#C8D3E6",
        "cam_scan":            "#886600",
        "cam_found":           "#006B44",
        "fsm_idle":            "#7A8899",
        "fsm_obj":             "#775500",
        "fsm_clss":            "#005580",
        "fsm_route":           "#005533",
        "fsm_done":            "#7A8899",
        "state_box_active_fg": "#FFFFFF",
        "sens_active":         "#04D589",
        "sens_inactive":       "#55575A",
        "sens_active_fg":      "#FFFFFF",
        "sens_inactive_fg":    "#7A8899",
    },
}
C = dict(THEMES["dark"])

# Estados FSM compartilhados (formato unificado)
FSM_STATE_DEFS = {
    0:   ("IDLE",                "fsm_idle",  "text_dim"),
    1:   ("OBJETO DETECTADO",    "fsm_obj",   "yellow"),
    2:   ("CLASSIFICANDO",       "fsm_clss",  "accent"),
    3:   ("ROTA A",              "fsm_route", "green"),
    4:   ("ROTA B",              "fsm_route", "accent2"),
    5:   ("ENTREGUE - ROTA A",   "fsm_done",  "green"),
    6:   ("ENTREGUE - ROTA B",   "fsm_done",  "accent2"),
    255: ("--",                  "fsm_idle",  "text_dim"),
}
FSM_PIPELINE      = [0, 1, 2, 3, 4]
FSM_PIPELINE_LBLS = ["IDLE", "OBJ\nDET", "CLASS\nIF", "ROTA\nA", "ROTA\nB"]

LOOP_INTERVAL   = 0.060
RECONNECT_DELAY = 3.0
ACTIVITY_ON_MS  = 120

# =============================================================================
#  UTILS
# =============================================================================
def now_ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def parse_qr(raw: str):
    """
    Extrai byte de rota do texto do QR.
    Formato do gerador: "Nome da Peca: X\nDestino: 0xAADA\n..."
    Estrategia 1: campo 'Destino:' no texto (robusto, sem falsos positivos).
    Estrategia 2: fallback para tokens com prefixo 0x.
    """
    if not raw:
        return None
    match = re.search(
        r'[Dd]estino[^:=]*[:=][^0-9a-fA-F]*([0-9a-fA-F xX]{2,6})', raw)
    if match:
        val = match.group(1).strip().upper().replace("0X", "").replace(" ", "")
        try:
            if len(val) == 4:
                b0, b1 = int(val[:2], 16), int(val[2:], 16)
                if b0 == START_FRAME and b1 in (ROUTE_A_SEND, ROUTE_B_SEND):
                    return b1
            elif len(val) == 2:
                b = int(val, 16)
                if b in (ROUTE_A_SEND, ROUTE_B_SEND):
                    return b
        except ValueError:
            pass
        return None
    for token in re.findall(r'0[xX]([0-9a-fA-F]{2,4})', raw):
        val = token.upper()
        try:
            if len(val) == 4:
                b0, b1 = int(val[:2], 16), int(val[2:], 16)
                if b0 == START_FRAME and b1 in (ROUTE_A_SEND, ROUTE_B_SEND):
                    return b1
            elif len(val) == 2:
                b = int(val, 16)
                if b in (ROUTE_A_SEND, ROUTE_B_SEND):
                    return b
        except ValueError:
            pass
    return None

# =============================================================================
#  SERIAL MANAGER  (instancia unica, parser 3 bytes)
# =============================================================================
class SerialManager:
    def __init__(self):
        self.ser        = None
        self.connected  = False
        self._tx_q      = queue.Queue()
        self._cbs       = []
        self._running   = False
        self._lock      = threading.Lock()
        self._last_port = None
        self._last_baud = 115200

    def list_ports(self):
        if not SERIAL_AVAILABLE:
            return []
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port, baud=115200):
        with self._lock:
            try:
                self.ser = serial.Serial(port, baud,
                                         timeout=0.02, write_timeout=0.1)
                self.connected  = True
                self._running   = True
                self._last_port = port
                self._last_baud = baud
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

    def send_frame(self, cmd, data=None):
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
        state  = "IDLE"
        status = 0
        cmd    = 0
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
                if state == "IDLE":
                    if b in (CMD_OK, CMD_ERR):
                        status = b
                        state  = "WAIT_CMD"
                elif state == "WAIT_CMD":
                    cmd = b
                    if b == SENS_STATUS_MSG:
                        state = "WAIT_DATA"
                    else:
                        self._notify("rx", (status, cmd, None))
                        state = "IDLE"
                elif state == "WAIT_DATA":
                    self._notify("rx", (status, cmd, b))
                    state = "IDLE"
        self._notify("disconnected", None)

# =============================================================================
#  CAMERA QR  (callback unico por frame)
# =============================================================================
class CameraReader:
    def __init__(self):
        self._running = False
        self.on_tick  = None
        self.on_lost  = None

    def start(self, idx=0):
        cap = cv2.VideoCapture(idx)
        ok  = cap.isOpened()
        cap.release()
        if not ok:
            return False
        self._running = True
        threading.Thread(target=self._loop, args=(idx,),
                         daemon=True, name="cam").start()
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
            rgb    = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            qr_raw = None
            qr_cmd = None
            data, pts, _ = det.detectAndDecode(frame)
            if data and pts is not None:
                p = pts[0].astype(int)
                for i in range(4):
                    cv2.line(rgb, tuple(p[i]), tuple(p[(i+1) % 4]),
                             (0, 217, 122), 2)
                cv2.putText(rgb, data,
                            (p[0][0], max(p[0][1] - 10, 14)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55,
                            (0, 217, 122), 2)
                cmd = parse_qr(data)
                if cmd:
                    qr_raw = data
                    qr_cmd = cmd
            if self.on_tick:
                self.on_tick(rgb, qr_raw, qr_cmd)
            time.sleep(0.033)
        cap.release()

# =============================================================================
#  CLASSIFICADOR QR
# =============================================================================
class Classifier:
    def __init__(self):
        self._active         = False
        self._attempts       = 0
        self.on_classified   = None
        self.on_attempt_warn = None

    @property
    def active(self):
        return self._active

    @property
    def attempts(self):
        return self._attempts

    def start(self):
        self._active   = True
        self._attempts = 0

    def stop(self):
        total          = self._attempts
        self._active   = False
        self._attempts = 0
        return total

    def feed(self, raw, cmd):
        if not self._active:
            return
        self._attempts += 1
        if cmd in (ROUTE_A_SEND, ROUTE_B_SEND):
            self._active = False
            if self.on_classified:
                self.on_classified(raw, cmd)
            return
        if self._attempts % 10 == 0 and self.on_attempt_warn:
            self.on_attempt_warn(self._attempts)

    def count_attempt(self):
        if not self._active:
            return
        self._attempts += 1
        if self._attempts % 10 == 0 and self.on_attempt_warn:
            self.on_attempt_warn(self._attempts)

# =============================================================================
#  APLICACAO PRINCIPAL
# =============================================================================
class EsteiraApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ESTEIRA SEPARADORA -- Sistema de Controle v4.0")
        self.resizable(True, True)
        self.minsize(1100, 720)

        self._theme_name = "dark"

        # --- Instancias compartilhadas ---
        self.serial     = SerialManager()
        self.serial.add_callback(self._on_serial_event)
        self.camera     = CameraReader() if QR_AVAILABLE else None
        self.classifier = Classifier()
        self.classifier.on_classified   = self._on_classified
        self.classifier.on_attempt_warn = self._on_attempt_warn

        # --- Estado FSM compartilhado ---
        self._fsm_id     = 255
        self._op_mode    = tk.StringVar(value="FSM")
        self._gate       = tk.StringVar(value="FECHADA")
        self._motor_st   = tk.StringVar(value="LIVRE")
        self._motor_dir  = tk.StringVar(value="FRENTE")
        self._flash      = tk.StringVar(value="OFF")
        self._sens_flags = [tk.BooleanVar(value=False) for _ in range(4)]

        # --- Camera ---
        self._cam_active  = False
        self._cam_img_ref = None
        self._cam_idx     = tk.IntVar(value=0)

        # --- Loop de passos ---
        self._step_count        = tk.IntVar(value=100)
        self._loop_mode         = tk.BooleanVar(value=False)
        self._step_loop_running = False
        self._step_loop_thread  = None

        # --- Reconexao automatica ---
        self._auto_reconnect   = True
        self._reconnect_thread = None

        # --- Indicador de atividade ---
        self._activity_after_id = None

        # --- Logs independentes por aba ---
        self._active_tab  = "fsm"   # "fsm" ou "dbg"
        self._fsm_lines   = []      # buffer para exportacao FSM
        self._dbg_lines   = []      # buffer para exportacao Debug
        self._fsm_save_n  = 0       # contador de saves FSM na sessao
        self._dbg_save_n  = 0       # contador de saves Debug na sessao
        self._error_count = 0
        self._error_var   = tk.StringVar(value="ERR: 0")

        # --- Recoloracao de tema ---
        self._tw_list      = []
        self._btn_list     = []
        self._cbs_list     = []
        self._fsm_boxes    = {}     # pipeline da aba FSM
        self._dbg_boxes    = {}     # pipeline da aba Debug
        self._sens_boxes   = []
        self._sens_lbls    = []

        self._build_fonts()
        self.configure(bg=C["bg"])
        self._build_ui()
        self._recolor_all()
        self._apply_fsm_log_tags()
        self._apply_dbg_log_tags()
        self._refresh_fsm_panels(255)
        self._upd_hw_colors()
        self._refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # =========================================================================
    #  FONTES
    # =========================================================================
    def _build_fonts(self):
        self.f_mono  = tkfont.Font(family="Courier New", size=9)
        self.f_monob = tkfont.Font(family="Courier New", size=9,  weight="bold")
        self.f_sm    = tkfont.Font(family="Courier New", size=8)
        self.f_title = tkfont.Font(family="Courier New", size=11, weight="bold")
        self.f_fsm   = tkfont.Font(family="Courier New", size=13, weight="bold")
        self.f_btn   = tkfont.Font(family="Courier New", size=9,  weight="bold")
        self.f_head  = tkfont.Font(family="Courier New", size=10, weight="bold")
        self.f_tab   = tkfont.Font(family="Courier New", size=10, weight="bold")

    # =========================================================================
    #  UI RAIZ
    # =========================================================================
    def _build_ui(self):
        self._build_topbar()
        body = self._tw(tk.Frame(self), bg="bg")
        body.pack(fill="both", expand=True, padx=10, pady=(0, 10))
        self._pg_fsm = self._tw(tk.Frame(body), bg="bg")
        self._pg_dbg = self._tw(tk.Frame(body), bg="bg")
        self._build_page_fsm(self._pg_fsm)
        self._build_page_dbg(self._pg_dbg)
        self._show_tab("fsm")

    # =========================================================================
    #  BARRA SUPERIOR (titulo, tabs, tema, conexao)
    # =========================================================================
    def _build_topbar(self):
        f1 = self._tw(tk.Frame(self, pady=6), bg="bg")
        f1.pack(fill="x", padx=10)

        self._tw(tk.Label(f1, text="ESTEIRA SEPARADORA", font=self.f_title),
                 bg="bg", fg="accent").pack(side="left")
        self._tw(tk.Label(f1, text="STM32G070  v4.0", font=self.f_sm),
                 bg="bg", fg="text_dim").pack(side="left", padx=10)

        # Botao tema
        self._btn_theme = tk.Button(
            f1, text="*", font=self.f_head, width=3,
            bg=C["border"], fg=C["text_dim"], relief="flat", cursor="hand2",
            bd=0, command=self._toggle_theme,
            activebackground=C["bg"], activeforeground=C["text_dim"])
        self._btn_theme.pack(side="right", padx=4)
        self._btn_list.append((self._btn_theme, "text_dim"))

        # Tabs
        tabs = self._tw(tk.Frame(f1), bg="bg")
        tabs.pack(side="left", padx=18)
        self._tab_fsm = tk.Button(
            tabs, text="  APLICACAO  ", font=self.f_tab,
            relief="flat", bd=0, cursor="hand2", padx=12, pady=7,
            command=lambda: self._request_tab("fsm"))
        self._tab_fsm.pack(side="left", padx=(0, 3))
        self._tab_dbg = tk.Button(
            tabs, text="  DEBUG  ", font=self.f_tab,
            relief="flat", bd=0, cursor="hand2", padx=12, pady=7,
            command=lambda: self._request_tab("dbg"))
        self._tab_dbg.pack(side="left")

        # Barra conexao
        f2 = self._tw(tk.Frame(self), bg="border")
        f2.pack(fill="x")
        self._build_conn_bar(f2)

    def _show_tab(self, name):
        self._pg_fsm.pack_forget()
        self._pg_dbg.pack_forget()
        self._active_tab = name
        if name == "fsm":
            self._pg_fsm.pack(fill="both", expand=True)
            self._tab_fsm.config(bg=C["tab_act_bg"], fg=C["tab_act_fg"])
            self._tab_dbg.config(bg=C["tab_idle_bg"], fg=C["tab_idle_fg"])
        else:
            self._pg_dbg.pack(fill="both", expand=True)
            self._tab_dbg.config(bg=C["tab_act_bg"], fg=C["tab_act_fg"])
            self._tab_fsm.config(bg=C["tab_idle_bg"], fg=C["tab_idle_fg"])

    def _request_tab(self, target):
        """Pergunta ao usuario se deseja salvar o log antes de trocar de aba."""
        if target == self._active_tab:
            return
        current = self._active_tab
        # Verifica se ha algo no log atual
        lines = self._fsm_lines if current == "fsm" else self._dbg_lines
        if lines:
            prefix = "FSM" if current == "fsm" else "Debug"
            ans = messagebox.askyesnocancel(
                "Trocar de aba",
                f"O log da aba {prefix} possui {len(lines)} linha(s).\n"
                "Deseja salvar antes de trocar de aba?")
            if ans is None:       # Cancelar: nao troca
                return
            if ans:               # Sim: salva e troca
                self._save_log(current)
        self._show_tab(target)

    def _save_log(self, tab):
        """Salva o log da aba especificada num arquivo txt."""
        if tab == "fsm":
            self._fsm_save_n += 1
            fname = f"logFSM_{self._fsm_save_n:03d}.txt"
            lines = self._fsm_lines[:]
            self._fsm_lines.clear()
            self._clear_txt(self._fsm_log)
        else:
            self._dbg_save_n += 1
            fname = f"logDBG_{self._dbg_save_n:03d}.txt"
            lines = self._dbg_lines[:]
            self._dbg_lines.clear()
            self._clear_txt(self._dbg_log)
            self._error_count = 0
            self._error_var.set("ERR: 0")

        with open(fname, "w", encoding="utf-8") as f:
            prefix = "FSM" if tab == "fsm" else "Debug"
            f.write(f"# Esteira Separadora v4.0 -- Log {prefix}\n")
            f.write(f"# Salvo em: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}\n\n")
            f.writelines(lines)

        msg = f"Log salvo em: {os.path.abspath(fname)}"
        if tab == self._active_tab:
            self._write_log_to("fsm" if tab == "fsm" else "dbg",
                               "sys", f"Log exportado -> {fname}")
        messagebox.showinfo("Log salvo", msg)

    # =========================================================================
    #  BARRA DE CONEXAO
    # =========================================================================
    def _build_conn_bar(self, parent):
        row = self._tw(tk.Frame(parent), bg="border")
        row.pack(fill="x", padx=10, pady=5)

        self._tw(tk.Label(row, text="PORTA:", font=self.f_sm),
                 bg="border", fg="text_dim").pack(side="left", padx=(0, 2))
        self._port_var = tk.StringVar()
        self._port_cb  = ttk.Combobox(row, textvariable=self._port_var,
                                       width=16, state="readonly")
        self._port_cb.pack(side="left", padx=4)

        self._tw(tk.Label(row, text="BAUD:", font=self.f_sm),
                 bg="border", fg="text_dim").pack(side="left", padx=(6, 2))
        self._baud_var = tk.StringVar(value="115200")
        ttk.Combobox(row, textvariable=self._baud_var,
                     values=["9600","19200","57600","115200","230400"],
                     width=7, state="readonly").pack(side="left", padx=4)

        self._btn(row, "R", self._refresh_ports, "text_dim", 3).pack(
            side="left", padx=2)
        self._btn_conn = self._btn(row, "CONECTAR",
                                    self._toggle_connect, "green", 12)
        self._btn_conn.pack(side="left", padx=8)

        self._activity_led = self._tw(
            tk.Label(row, text="*", font=self.f_sm),
            bg="border", fg="text_dim")
        self._activity_led.pack(side="left", padx=2)

        self._led = self._tw(tk.Label(row, text="*", font=self.f_head),
                              bg="border", fg="text_dim")
        self._led.pack(side="left", padx=4)
        self._lbl_conn = self._tw(
            tk.Label(row, text="DESCONECTADO", font=self.f_sm),
            bg="border", fg="text_dim")
        self._lbl_conn.pack(side="left", padx=4)

        self._btn_reset = self._btn(row, "SW RESET",
                                     self._do_sw_reset, "red", 10)
        self._btn_reset.pack(side="right", padx=4)
        self._btn_reset.config(state="disabled")

        self._btn_hs = self._btn(row, "HANDSHAKE",
                                  self._do_handshake, "accent", 12)
        self._btn_hs.pack(side="right", padx=8)
        self._btn_hs.config(state="disabled")

        self._style_ttk()

    # =========================================================================
    #  ABA FSM: pipeline FSM | log FSM | camera
    # =========================================================================
    def _build_page_fsm(self, parent):
        parent.columnconfigure(0, weight=3, minsize=280)
        parent.columnconfigure(1, weight=2, minsize=260)
        parent.columnconfigure(2, weight=5, minsize=380)
        parent.rowconfigure(0, weight=1)

        col0 = self._tw(tk.Frame(parent), bg="bg")
        col0.grid(row=0, column=0, sticky="nsew", padx=(0, 5), pady=6)
        col0.rowconfigure(1, weight=1)
        col0.columnconfigure(0, weight=1)

        col1 = self._tw(tk.Frame(parent), bg="bg")
        col1.grid(row=0, column=1, sticky="nsew", padx=(0, 5), pady=6)
        col1.rowconfigure(0, weight=1)
        col1.columnconfigure(0, weight=1)

        col2 = self._tw(tk.Frame(parent), bg="bg")
        col2.grid(row=0, column=2, sticky="nsew", pady=6)
        col2.rowconfigure(0, weight=1)
        col2.columnconfigure(0, weight=1)

        # Col 0: FSM pipeline + status resumido
        self._build_fsm_pipeline_panel(col0, "fsm")
        self._build_fsm_status_mini(col0)

        # Col 1: Log FSM
        self._build_fsm_log_panel(col1)

        # Col 2: Camera
        self._build_cam_panel(col2)

    def _build_fsm_pipeline_panel(self, parent, tag):
        """Pipeline de estados -- reutilizado nas duas abas."""
        p = self._panel(parent, "MAQUINA DE ESTADOS")
        p.grid(row=0, column=0, sticky="ew", pady=(6, 0)) if tag == "fsm" \
            else p.pack(fill="x", pady=(0, 6))

        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=8, pady=8)

        lbl = self._tw(
            tk.Label(inn, text="--", font=self.f_fsm, anchor="center"),
            bg="panel", fg="text_dim")
        lbl.pack(fill="x", pady=(0, 8))

        pipe = self._tw(tk.Frame(inn), bg="panel")
        pipe.pack(fill="x")
        boxes = {}
        for i, (sid, slbl) in enumerate(zip(FSM_PIPELINE, FSM_PIPELINE_LBLS)):
            col = self._tw(tk.Frame(pipe), bg="panel")
            col.pack(side="left", expand=True, fill="x")
            box = tk.Label(col, text=slbl,
                            bg=C["border"], fg=C["text_dim"],
                            font=self.f_sm, relief="flat",
                            padx=3, pady=4, justify="center")
            box.pack(fill="x", padx=1)
            boxes[sid] = box
            if i < len(FSM_PIPELINE) - 1:
                self._tw(tk.Label(pipe, text=">", font=self.f_sm),
                         bg="panel", fg="text_dim").pack(side="left")

        if tag == "fsm":
            self._fsm_lbl   = lbl
            self._fsm_boxes = boxes
        else:
            self._dbg_lbl   = lbl
            self._dbg_boxes = boxes
        return p

    def _build_fsm_status_mini(self, parent):
        """Status resumido (apenas modo e cancela) na aba FSM."""
        p = self._panel(parent, "STATUS")
        p.grid(row=1, column=0, sticky="nsew", pady=(6, 6))

        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=8, pady=6)
        inn.columnconfigure(1, weight=1)

        rows = [("MODO",    self._op_mode),
                ("CANCELA", self._gate),
                ("MOTOR",   self._motor_st),
                ("FLASH",   self._flash)]
        self._fsm_status_lbls = {}
        for i, (n, v) in enumerate(rows):
            self._tw(tk.Label(inn, text=f"{n}:", font=self.f_sm,
                               anchor="w", width=8),
                     bg="panel", fg="text_dim").grid(
                row=i, column=0, sticky="w", padx=4, pady=2)
            lbl = self._tw(tk.Label(inn, textvariable=v,
                                     font=self.f_monob, anchor="w"),
                           bg="panel", fg="accent")
            lbl.grid(row=i, column=1, sticky="w", padx=4, pady=2)
            self._fsm_status_lbls[n] = lbl

        # Ultimo QR lido
        self._tw(tk.Frame(inn, height=1), bg="border").grid(
            row=4, column=0, columnspan=2, sticky="ew", pady=6)
        self._tw(tk.Label(inn, text="ULTIMO QR:", font=self.f_sm, anchor="w"),
                 bg="panel", fg="text_dim").grid(row=5, column=0, sticky="w", padx=4)
        self._lbl_qr = self._tw(
            tk.Label(inn, text="--", font=self.f_sm, anchor="w", wraplength=200),
            bg="panel", fg="text_dim")
        self._lbl_qr.grid(row=5, column=1, sticky="w", padx=4)

    def _build_fsm_log_panel(self, parent):
        p = self._panel(parent, "LOG  [UART | FSM | QR]")
        p.grid(row=0, column=0, sticky="nsew", pady=6)

        foot = self._tw(tk.Frame(p), bg="panel")
        foot.pack(side="bottom", fill="x", padx=6, pady=(0, 4))

        leg = self._tw(tk.Frame(foot), bg="panel")
        leg.pack(side="left")
        for lbl, key in [("UART","log_tx"),("FSM","log_rx"),
                          ("QR","log_qr"),("AVISO","log_warn")]:
            self._tw(tk.Label(leg, text=f"[{lbl}]", font=self.f_sm),
                     bg="panel", fg=key).pack(side="left", padx=3)

        self._btn(foot, "SALVAR",
                  lambda: self._save_log("fsm"),
                  "accent", 8).pack(side="right", padx=3)
        self._btn(foot, "LIMPAR",
                  lambda: (self._clear_txt(self._fsm_log),
                            self._fsm_lines.clear()),
                  "text_dim", 8).pack(side="right", padx=3)

        tf = self._tw(tk.Frame(p), bg="panel")
        tf.pack(fill="both", expand=True, padx=6, pady=6)
        tf.rowconfigure(0, weight=1)
        tf.columnconfigure(0, weight=1)

        self._fsm_log = tk.Text(
            tf, bg=C["bg"], fg=C["text"], font=self.f_mono,
            relief="flat", state="disabled", wrap="none",
            insertbackground=C["accent"])
        self._fsm_log.grid(row=0, column=0, sticky="nsew")

        sb_y = ttk.Scrollbar(tf, orient="vertical",
                              command=self._fsm_log.yview)
        sb_y.grid(row=0, column=1, sticky="ns")
        sb_x = ttk.Scrollbar(tf, orient="horizontal",
                              command=self._fsm_log.xview)
        sb_x.grid(row=1, column=0, sticky="ew")
        self._fsm_log.config(yscrollcommand=sb_y.set,
                              xscrollcommand=sb_x.set)
        self._tw_list.append((self._fsm_log, {"bg": "bg", "fg": "text"}))
        self._apply_fsm_log_tags()

    def _apply_fsm_log_tags(self):
        self._fsm_log.tag_config("uart", foreground=C["log_tx"])
        self._fsm_log.tag_config("fsm",  foreground=C["log_rx"])
        self._fsm_log.tag_config("qr",   foreground=C["log_qr"])
        self._fsm_log.tag_config("warn", foreground=C["log_warn"])
        self._fsm_log.tag_config("err",  foreground=C["log_err"])
        self._fsm_log.tag_config("sys",  foreground=C["log_rx"])
        self._fsm_log.config(bg=C["bg"], fg=C["text"])

    # =========================================================================
    #  PAINEL CAMERA (aba FSM)
    # =========================================================================
    def _build_cam_panel(self, parent):
        p = self._panel(parent, "CAMERA EM TEMPO REAL")
        p.grid(row=0, column=0, sticky="nsew", pady=6)

        bar = self._tw(tk.Frame(p), bg="panel")
        bar.pack(fill="x", padx=8, pady=6)

        self._tw(tk.Label(bar, text="CAM:", font=self.f_sm),
                 bg="panel", fg="text_dim").pack(side="left")
        tk.Spinbox(bar, from_=0, to=9, textvariable=self._cam_idx,
                   width=3, font=self.f_mono,
                   bg=C["border"], fg=C["text_head"],
                   buttonbackground=C["border"],
                   relief="flat").pack(side="left", padx=6)

        self._btn_cam = self._btn(bar, "INICIAR CAMERA",
                                   self._toggle_camera, "green", 16)
        self._btn_cam.pack(side="left", padx=4)

        self._lbl_cam_st = self._tw(
            tk.Label(bar, text="Camera inativa", font=self.f_sm),
            bg="panel", fg="text_dim")
        self._lbl_cam_st.pack(side="left", padx=8)

        if not QR_AVAILABLE:
            self._tw(tk.Label(bar, font=self.f_sm,
                               text="pip install opencv-python Pillow"),
                     bg="panel", fg="log_warn").pack(side="right", padx=8)

        self._canvas = tk.Canvas(
            p, bg=C["bg"], highlightthickness=2,
            highlightbackground=C["cam_idle"])
        self._canvas.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self._tw_list.append((self._canvas, {"bg": "bg"}))
        self._canvas.bind("<Configure>", lambda e: self._draw_placeholder())

    def _draw_placeholder(self):
        self._canvas.update_idletasks()
        w = max(self._canvas.winfo_width(),  10)
        h = max(self._canvas.winfo_height(), 10)
        self._canvas.delete("all")
        self._canvas.create_rectangle(0, 0, w, h, fill=C["bg"], outline="")
        for x in range(0, w, 40):
            self._canvas.create_line(x, 0, x, h, fill=C["border"], width=1)
        for y in range(0, h, 40):
            self._canvas.create_line(0, y, w, y, fill=C["border"], width=1)
        cx, cy, sz = w // 2, h // 2, 52
        self._canvas.create_rectangle(cx-sz, cy-sz, cx+sz, cy+sz,
                                       outline=C["text_dim"], width=2)
        for ox, oy in [(-sz+6,-sz+6),(sz//3,-sz+6),(-sz+6,sz//3)]:
            s = sz//3-4
            self._canvas.create_rectangle(cx+ox,cy+oy,cx+ox+s,cy+oy+s,
                                           outline=C["text_dim"],width=2)
        self._canvas.create_text(cx, cy+sz+24, fill=C["text_dim"],
                                  font=("Courier New",10),
                                  text="Camera inativa -- pressione INICIAR CAMERA")

    def _toggle_camera(self):
        if not QR_AVAILABLE:
            messagebox.showerror("Dependencias ausentes",
                                  "Instale: pip install opencv-python Pillow")
            return
        if self._cam_active:
            self.camera.stop()
            self._cam_active = False
            self._btn_cam.config(text="INICIAR CAMERA", fg=C["green"])
            self._lbl_cam_st.config(text="Camera inativa", fg=C["text_dim"])
            self._canvas.config(highlightbackground=C["cam_idle"])
            self._draw_placeholder()
            self._write_log_to("fsm", "fsm", "Camera desligada.")
        else:
            self.camera.on_tick = \
                lambda f, r, c: self.after(0, self._upd_tick, f, r, c)
            self.camera.on_lost = lambda: self.after(0, self._cam_lost)
            ok = self.camera.start(self._cam_idx.get())
            if ok:
                self._cam_active = True
                self._btn_cam.config(text="PARAR CAMERA", fg=C["red"])
                self._lbl_cam_st.config(text="Ativa", fg=C["green"])
                self._write_log_to("fsm", "fsm",
                    f"Camera iniciada (indice {self._cam_idx.get()}).")
            else:
                messagebox.showerror("Erro de camera",
                    f"Nao foi possivel abrir camera {self._cam_idx.get()}.")

    def _upd_tick(self, rgb, qr_raw, qr_cmd):
        """Handler unico por frame -- sem corrida entre QR e frame."""
        cw = self._canvas.winfo_width()
        ch = self._canvas.winfo_height()
        if cw >= 10 and ch >= 10:
            img = Image.fromarray(rgb).resize((cw, ch), Image.LANCZOS)
            self._cam_img_ref = ImageTk.PhotoImage(img)
            self._canvas.create_image(0, 0, anchor="nw",
                                       image=self._cam_img_ref)
        if qr_raw and qr_cmd:
            route = "A" if qr_cmd == ROUTE_A_SEND else "B"
            self._lbl_qr.config(
                text=f"ROTA {route}  |  {qr_raw[:40]}",
                fg=C["green"])
            if self.classifier.active:
                self._write_log_to("fsm", "qr",
                    f"QR LIDO: '{qr_raw[:60]}'")
                self._write_log_to("fsm", "qr",
                    f"  ROTA EXTRAIDA: {route}  (0x{qr_cmd:02X})")
                self.classifier.feed(qr_raw, qr_cmd)
        else:
            self.classifier.count_attempt()

    def _cam_lost(self):
        self._cam_active = False
        self._btn_cam.config(text="INICIAR CAMERA", fg=C["green"])
        self._lbl_cam_st.config(text="Camera desconectada!", fg=C["red"])
        self._canvas.config(highlightbackground=C["cam_idle"])
        self._draw_placeholder()
        self._write_log_to("fsm", "err", "Camera perdeu conexao.")

    # Callbacks classificador
    def _on_classified(self, raw, cmd):
        route = "A" if cmd == ROUTE_A_SEND else "B"
        self._write_log_to("fsm", "qr",
            f"CLASSIFICACAO OK -> ROTA {route}  |  enviando 0x{cmd:02X}")
        self._send(cmd)

    def _on_attempt_warn(self, n):
        self._write_log_to("fsm", "warn",
            f"AVISO: {n} tentativas sem leitura do QRcode -- aguardando...")

    # =========================================================================
    #  ABA DEBUG: estrutura identica ao codigo de debug original
    # =========================================================================
    def _build_page_dbg(self, parent):
        parent.columnconfigure(0, weight=1, minsize=230)
        parent.columnconfigure(1, weight=1, minsize=290)
        parent.columnconfigure(2, weight=2, minsize=360)
        parent.rowconfigure(0, weight=1)

        ca = self._tw(tk.Frame(parent), bg="bg")
        ca.grid(row=0, column=0, sticky="nsew", padx=(0,6), pady=6)
        self._build_hw_status(ca)
        self._build_fsm_pipeline_panel(ca, "dbg")
        self._build_fsm_cmds(ca)
        self._build_sensors_panel(ca)

        cb = self._tw(tk.Frame(parent), bg="bg")
        cb.grid(row=0, column=1, sticky="nsew", padx=(0,6), pady=6)
        self._build_async_ctrl(cb)

        cc = self._tw(tk.Frame(parent), bg="bg")
        cc.grid(row=0, column=2, sticky="nsew", pady=6)
        self._build_dbg_log_panel(cc)

    def _build_hw_status(self, parent):
        p = self._panel(parent, "STATUS DO HARDWARE")
        p.pack(fill="x", pady=(4,6))
        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=8, pady=6)
        rows = [("MODO",    self._op_mode),
                ("CANCELA", self._gate),
                ("MOTOR",   self._motor_st),
                ("DIRECAO", self._motor_dir),
                ("FLASH",   self._flash)]
        self._hw = {}
        for i, (n, v) in enumerate(rows):
            self._tw(tk.Label(inn, text=f"{n}:", font=self.f_sm,
                               anchor="w", width=9),
                     bg="panel", fg="text_dim").grid(
                row=i, column=0, sticky="w", pady=2)
            lbl = self._tw(tk.Label(inn, textvariable=v,
                                     font=self.f_monob, anchor="w"),
                           bg="panel", fg="accent")
            lbl.grid(row=i, column=1, sticky="w", padx=4, pady=2)
            self._hw[n] = lbl

    def _upd_hw_colors(self):
        if not hasattr(self, "_hw"):
            return
        self._hw["MODO"].config(
            fg=C["accent"] if self._op_mode.get()=="FSM" else C["accent2"])
        self._hw["CANCELA"].config(
            fg=C["green"] if self._gate.get()=="ABERTA" else C["red"])
        self._hw["MOTOR"].config(
            fg=C["green"] if self._motor_st.get()=="ENGAJADO" else C["text_dim"])
        self._hw["FLASH"].config(
            fg=C["yellow"] if self._flash.get()=="ON" else C["text_dim"])
        self._hw["DIRECAO"].config(fg=C["text"])
        if hasattr(self,"_fsm_status_lbls"):
            self._fsm_status_lbls["MODO"].config(
                fg=C["accent"] if self._op_mode.get()=="FSM" else C["accent2"])
            self._fsm_status_lbls["CANCELA"].config(
                fg=C["green"] if self._gate.get()=="ABERTA" else C["red"])
            self._fsm_status_lbls["MOTOR"].config(
                fg=C["green"] if self._motor_st.get()=="ENGAJADO" else C["text_dim"])
            self._fsm_status_lbls["FLASH"].config(
                fg=C["yellow"] if self._flash.get()=="ON" else C["text_dim"])

    def _build_fsm_cmds(self, parent):
        p = self._panel(parent, "COMANDOS FSM")
        p.pack(fill="x", pady=(0,6))
        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=6, pady=6)
        r1 = self._tw(tk.Frame(inn), bg="panel")
        r1.pack(fill="x", pady=2)
        self._btn(r1,"ROTA A",lambda:self._send(ROUTE_A_SEND),"green",14).pack(
            side="left",padx=4)
        self._btn(r1,"ROTA B",lambda:self._send(ROUTE_B_SEND),"accent2",14).pack(
            side="left",padx=4)
        r2 = self._tw(tk.Frame(inn), bg="panel")
        r2.pack(fill="x", pady=2)
        self._btn(r2,"TOGGLE DEBUG",lambda:self._send(DEBUG_TOGGLE),
                  "yellow",30).pack(side="left",padx=4)

    def _build_sensors_panel(self, parent):
        p = self._panel(parent, "SENSORES LASER  [modo DEBUG]")
        p.pack(fill="x", pady=(0,6))
        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=6, pady=8)
        self._sens_boxes = []
        self._sens_lbls  = []
        for i in range(4):
            col = self._tw(tk.Frame(inn), bg="panel")
            col.pack(side="left", expand=True, fill="x", padx=4)
            box = tk.Label(col, text=f"S{i+1}",
                            bg=C["sens_inactive"], fg=C["sens_inactive_fg"],
                            font=self.f_monob, relief="flat",
                            padx=8, pady=10, justify="center")
            box.pack(fill="x")
            self._sens_boxes.append(box)
            lbl = self._tw(tk.Label(col, text="LIVRE",
                                     font=self.f_sm, anchor="center"),
                           bg="panel", fg="text_dim")
            lbl.pack(fill="x")
            self._sens_lbls.append(lbl)

    def _refresh_sensors(self):
        for i, box in enumerate(self._sens_boxes):
            a = self._sens_flags[i].get()
            box.config(bg=C["sens_active"]    if a else C["sens_inactive"],
                       fg=C["sens_active_fg"] if a else C["sens_inactive_fg"])
        for i, lbl in enumerate(self._sens_lbls):
            a = self._sens_flags[i].get()
            lbl.config(text="OBJETO" if a else "LIVRE",
                       fg=C["red"] if a else C["text_dim"])

    def _build_async_ctrl(self, parent):
        p = self._panel(parent, "CONTROLE ASSINCRONO  (MODO DEBUG)")
        p.pack(fill="both", expand=True, pady=4)
        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="x", padx=6, pady=6)

        def row():
            f = self._tw(tk.Frame(inn), bg="panel")
            f.pack(fill="x", pady=3)
            return f
        def lbl(f, t, w=9):
            self._tw(tk.Label(f,text=t,font=self.f_sm,width=w,anchor="w"),
                     bg="panel",fg="text_dim").pack(side="left")

        r = row(); lbl(r,"FLASH:")
        self._btn(r,"ON", lambda:self._send(LIGHT_EN), "yellow",6).pack(side="left",padx=3)
        self._btn(r,"OFF",lambda:self._send(LIGHT_DIS),"text_dim",6).pack(side="left",padx=3)

        r = row(); lbl(r,"CANCELA:")
        self._btn(r,"ABRIR", lambda:self._send(GATE_OPEN), "green",7).pack(side="left",padx=3)
        self._btn(r,"FECHAR",lambda:self._send(GATE_CLOSE),"red",7).pack(side="left",padx=3)

        r = row(); lbl(r,"MOTOR:")
        self._btn(r,"ENGAJAR",lambda:self._send(STPR_EN), "green",10).pack(side="left",padx=3)
        self._btn(r,"LIVRE",  lambda:self._send(STPR_DIS),"red",10).pack(side="left",padx=3)

        def fwd():
            self._motor_dir.set("FRENTE"); self._send(STPR_FORWARD); self._upd_hw_colors()
        def bwd():
            self._motor_dir.set("TRAS");   self._send(STPR_BACKWARD); self._upd_hw_colors()

        r = row(); lbl(r,"DIRECAO:")
        self._btn(r,"FRENTE",fwd,"accent",10).pack(side="left",padx=3)
        self._btn(r,"TRAS",  bwd,"accent",10).pack(side="left",padx=3)

        self._tw(tk.Frame(inn,height=1),bg="border").pack(fill="x",pady=8)
        self._tw(tk.Label(inn,text="CONTROLE DE PASSOS",font=self.f_sm),
                 bg="panel",fg="accent").pack(anchor="w")

        r = row()
        self._tw(tk.Label(r,text="N PASSOS:",font=self.f_sm),
                 bg="panel",fg="text_dim").pack(side="left",padx=(0,4))
        self._spin = tk.Spinbox(
            r,from_=1,to=255,textvariable=self._step_count,
            width=6,font=self.f_mono,
            bg=C["border"],fg=C["text_head"],
            buttonbackground=C["border"],relief="flat",
            insertbackground=C["accent"])
        self._spin.pack(side="left",padx=4)
        self._tw_list.append((self._spin,{
            "bg":"spinbox_bg","fg":"text_head",
            "buttonbackground":"border","insertbackground":"accent"}))

        r = row()
        cb = tk.Checkbutton(r,text="MODO LOOP",variable=self._loop_mode,
                             bg=C["panel"],fg=C["text"],selectcolor=C["border"],
                             activebackground=C["panel"],activeforeground=C["text"],
                             font=self.f_sm,
                             command=lambda: self._stop_loop() if not self._loop_mode.get() else None)
        cb.pack(side="left"); self._cbs_list.append(cb)

        r = row()
        self._btn_steps = self._btn(r,"ENVIAR PASSOS",self._send_steps,"green",18)
        self._btn_steps.pack(side="left",padx=3)
        self._btn_lstop = self._btn(r,"PARAR",self._stop_loop,"red",8)
        self._btn_lstop.pack(side="left",padx=3)
        self._btn_lstop.config(state="disabled")

        self._lbl_loop = self._tw(tk.Label(inn,text="",font=self.f_sm),
                                   bg="panel",fg="yellow")
        self._lbl_loop.pack(anchor="w",pady=2)

        self._tw(tk.Frame(inn,height=1),bg="border").pack(fill="x",pady=8)
        self._btn(inn,"SW RESET STM32",self._do_sw_reset,"red",20).pack(
            anchor="w",padx=4)

    def _build_dbg_log_panel(self, parent):
        p = self._panel(parent, "LOG DE COMUNICACAO UART")
        p.pack(fill="both", expand=True, pady=4)

        inn = self._tw(tk.Frame(p), bg="panel")
        inn.pack(fill="both", expand=True, padx=6, pady=6)
        inn.rowconfigure(0, weight=1)
        inn.columnconfigure(0, weight=1)

        self._dbg_log = tk.Text(
            inn, bg=C["bg"], fg=C["text"], font=self.f_mono,
            relief="flat", state="disabled", wrap="none",
            insertbackground=C["accent"])
        self._dbg_log.grid(row=0, column=0, sticky="nsew")

        sb_y = ttk.Scrollbar(inn, orient="vertical",
                              command=self._dbg_log.yview)
        sb_y.grid(row=0, column=1, sticky="ns")
        sb_x = ttk.Scrollbar(inn, orient="horizontal",
                              command=self._dbg_log.xview)
        sb_x.grid(row=1, column=0, sticky="ew")
        self._dbg_log.config(yscrollcommand=sb_y.set,
                              xscrollcommand=sb_x.set)
        self._tw_list.append((self._dbg_log, {"bg":"bg","fg":"text"}))
        self._apply_dbg_log_tags()

        foot = self._tw(tk.Frame(p), bg="panel")
        foot.pack(fill="x", padx=6, pady=(0,4))

        self._err_lbl = self._tw(
            tk.Label(foot, textvariable=self._error_var, font=self.f_monob),
            bg="panel", fg="log_err")
        self._err_lbl.pack(side="left", padx=6)

        self._btn(foot,"SALVAR LOG",lambda: self._save_log("dbg"),
                  "accent",12).pack(side="right",padx=4)
        self._btn(foot,"LIMPAR LOG",
                  lambda:(self._clear_txt(self._dbg_log),
                           self._dbg_lines.clear(),
                           setattr(self,"_error_count",0) or
                           self._error_var.set("ERR: 0")),
                  "text_dim",12).pack(side="right",padx=4)

    def _apply_dbg_log_tags(self):
        self._dbg_log.tag_config("uart", foreground=C["log_tx"])
        self._dbg_log.tag_config("tx",   foreground=C["log_tx"])
        self._dbg_log.tag_config("fsm",  foreground=C["log_rx"])
        self._dbg_log.tag_config("rx",   foreground=C["log_rx"])
        self._dbg_log.tag_config("qr",   foreground=C["log_qr"])
        self._dbg_log.tag_config("sys",  foreground=C["log_sys"])
        self._dbg_log.tag_config("warn", foreground=C["log_warn"])
        self._dbg_log.tag_config("err",  foreground=C["log_err"])
        self._dbg_log.config(bg=C["bg"], fg=C["text"],
                              insertbackground=C["accent"])

    # =========================================================================
    #  LOG UNIFICADO (roteia para a aba ativa)
    # =========================================================================
    def _write_log_to(self, tab, tag, msg):
        """
        Escreve no log da aba indicada E NO LOG DA ABA ATIVA.
        Se a aba do evento nao for a ativa, o evento e silenciado.
        """
        if tab != self._active_tab:
            return
        line = f"[{now_ts()}]  {msg}\n"
        if tab == "fsm":
            self._fsm_lines.append(line)
            self._write(self._fsm_log, line, tag)
        else:
            self._dbg_lines.append(line)
            self._write(self._dbg_log, line, tag)
        self._flash_activity()

    def _write(self, widget, line, tag):
        widget.config(state="normal")
        widget.insert("end", line, tag)
        widget.see("end")
        widget.config(state="disabled")

    def _clear_txt(self, w):
        w.config(state="normal")
        w.delete("1.0", "end")
        w.config(state="disabled")

    def _flash_activity(self):
        self._activity_led.config(fg=C["accent"])
        if self._activity_after_id:
            self.after_cancel(self._activity_after_id)
        self._activity_after_id = self.after(
            ACTIVITY_ON_MS,
            lambda: self._activity_led.config(fg=C["text_dim"]))

    # =========================================================================
    #  FSM DISPLAY (atualiza ambas as abas)
    # =========================================================================
    def _refresh_fsm_panels(self, sid):
        self._fsm_id = sid
        name, box_clr, txt_clr = FSM_STATE_DEFS.get(sid, ("?","fsm_idle","text_dim"))
        # Aba FSM
        if hasattr(self, "_fsm_lbl"):
            self._fsm_lbl.config(text=name, fg=C[txt_clr])
        for s, box in self._fsm_boxes.items():
            if s == sid:
                box.config(bg=C[box_clr], fg=C["state_box_active_fg"])
            else:
                box.config(bg=C["border"], fg=C["text_dim"])
        # Aba Debug
        if hasattr(self, "_dbg_lbl"):
            self._dbg_lbl.config(text=name, fg=C[txt_clr])
        for s, box in self._dbg_boxes.items():
            if s == sid:
                box.config(bg=C[box_clr], fg=C["state_box_active_fg"])
            else:
                box.config(bg=C["border"], fg=C["text_dim"])

    # =========================================================================
    #  CALLBACKS SERIAL
    # =========================================================================
    def _on_serial_event(self, ev, data):
        if ev == "rx":
            self.after(0, self._handle_rx, data)
        elif ev == "disconnected":
            self.after(0, self._on_disconnected)

    def _handle_rx(self, data):
        status, payload, extra = data
        name = RX_NAMES.get(payload, f"DESCONHECIDO (0x{payload:02X})")
        tab  = self._active_tab

        if status == CMD_ERR:
            self._error_count += 1
            self._error_var.set(f"ERR: {self._error_count}")
            self._write_log_to(tab, "err",
                f"<< RX  [0x91]  CMD_ERR")
            return

        # Telemetria sensores (3 bytes) -- so relevante no debug
        if payload == SENS_STATUS_MSG and extra is not None:
            self._write_log_to("dbg", "uart",
                f"<< RX  [0x90][0x{payload:02X}]  {name}  STATUS=0x{extra:02X}")
            for i in range(4):
                self._sens_flags[i].set(bool(extra & (1 << i)))
            self._refresh_sensors()
            return

        # Log UART na aba ativa
        self._write_log_to(tab, "uart",
            f"<< RX  [0x90][0x{payload:02X}]  {name}")

        # --- FSM principal ---
        if payload == SYS_INIT:
            self._write_log_to(tab, "fsm",
                "HANDSHAKE OK -- sistema inicializado")
            self._refresh_fsm_panels(0)
            if QR_AVAILABLE and not self._cam_active:
                self._toggle_camera()

        elif payload == OBJ_DETECTED:
            self._write_log_to(tab, "fsm",
                "Objeto detectado -- encaminhando para a camera")
            self._refresh_fsm_panels(1)
            if hasattr(self,"_canvas"):
                self._canvas.config(highlightbackground=C["cam_idle"])

        elif payload == CLSS_REQUEST:
            self._write_log_to(tab, "fsm",
                "CLSS_REQUEST -- objeto sob a camera, iniciando classificacao")
            self._refresh_fsm_panels(2)
            self._write_log_to("fsm", "qr",
                "Modo de classificacao ATIVO -- aguardando QRcode")
            if hasattr(self,"_canvas"):
                self._canvas.config(highlightbackground=C["cam_scan"])
                self._lbl_cam_st.config(text="CLASSIFICANDO...",fg=C["yellow"])
            self.classifier.start()

        elif payload in (ROUTE_A_FWD, ROUTE_B_FWD):
            route = "A" if payload == ROUTE_A_FWD else "B"
            total = self.classifier.stop()
            self._write_log_to("fsm", "qr",
                f"Classificacao encerrada -- {total} tentativa(s)")
            self._write_log_to(tab, "fsm",
                f"STM32 confirmou encaminhamento -> ROTA {route}")
            sid = 3 if route == "A" else 4
            self._refresh_fsm_panels(sid)
            self._gate.set("FECHADA" if route == "A" else "ABERTA")
            self._upd_hw_colors()
            if hasattr(self,"_canvas") and self._cam_active:
                self._canvas.config(highlightbackground=C["cam_idle"])
                self._lbl_cam_st.config(text="Ativa", fg=C["green"])

        elif payload in (ROUTE_A_OK, ROUTE_B_OK):
            route = "A" if payload == ROUTE_A_OK else "B"
            self._write_log_to(tab, "fsm",
                f"Entrega confirmada -- ROTA {route}  |  ciclo encerrado")
            self._refresh_fsm_panels(5 if route == "A" else 6)
            self.after(2000, lambda: self._refresh_fsm_panels(0))

        elif payload == MODE_FSM_MSG:
            self._op_mode.set("FSM"); self._upd_hw_colors()
            self._write_log_to(tab, "sys", "MODO CONFIRMADO -> FSM")

        elif payload == MODE_DEBUG_MSG:
            self._op_mode.set("DEBUG"); self._upd_hw_colors()
            self._write_log_to(tab, "sys", "MODO CONFIRMADO -> DEBUG")

        elif payload == SW_RESET_MSG:
            self._write_log_to(tab, "sys",
                "SW RESET confirmado -- STM32 reiniciando...")

        elif payload == DEBUG_TOGGLE:
            new = "DEBUG" if self._op_mode.get()=="FSM" else "FSM"
            self._op_mode.set(new); self._upd_hw_colors()
            self._write_log_to(tab, "sys", f"Modo alternado -> {new}")

        # Ecos assincronos
        elif payload == GATE_OPEN:
            self._gate.set("ABERTA");   self._upd_hw_colors()
        elif payload == GATE_CLOSE:
            self._gate.set("FECHADA");  self._upd_hw_colors()
        elif payload == LIGHT_EN:
            self._flash.set("ON");      self._upd_hw_colors()
        elif payload == LIGHT_DIS:
            self._flash.set("OFF");     self._upd_hw_colors()
        elif payload == STPR_EN:
            self._motor_st.set("ENGAJADO"); self._upd_hw_colors()
        elif payload == STPR_DIS:
            self._motor_st.set("LIVRE");    self._upd_hw_colors()
        elif payload == STPR_FORWARD:
            self._motor_dir.set("FRENTE"); self._upd_hw_colors()
        elif payload == STPR_BACKWARD:
            self._motor_dir.set("TRAS");   self._upd_hw_colors()

    def _on_disconnected(self):
        self._led.config(fg=C["red"])
        self._lbl_conn.config(text="CONEXAO PERDIDA", fg=C["red"])
        self._btn_conn.config(text="CONECTAR", fg=C["green"])
        self._btn_hs.config(state="disabled")
        self._btn_reset.config(state="disabled")
        self._write_log_to(self._active_tab, "err",
            "Conexao UART encerrada -- tentando reconectar...")
        self._start_reconnect()

    # =========================================================================
    #  RECONEXAO AUTOMATICA
    # =========================================================================
    def _start_reconnect(self):
        if self._reconnect_thread and self._reconnect_thread.is_alive():
            return
        self._reconnect_thread = threading.Thread(
            target=self._reconnect_loop, daemon=True)
        self._reconnect_thread.start()

    def _reconnect_loop(self):
        port = self.serial._last_port
        baud = self.serial._last_baud
        if not port:
            return
        while self._auto_reconnect and not self.serial.connected:
            time.sleep(RECONNECT_DELAY)
            if self.serial.connected:
                break
            result = self.serial.connect(port, baud)
            if result is True:
                self.after(0, self._on_reconnected, port, baud)
                break

    def _on_reconnected(self, port, baud):
        self._led.config(fg=C["green"])
        self._lbl_conn.config(text=f"RECONECTADO  {port}", fg=C["green"])
        self._btn_conn.config(text="DESCONECTAR", fg=C["red"])
        self._btn_hs.config(state="normal")
        self._btn_reset.config(state="normal")
        self._write_log_to(self._active_tab, "sys",
            f"Reconectado automaticamente em {port} @ {baud}")

    # =========================================================================
    #  ACOES UART
    # =========================================================================
    def _send(self, cmd, data=None):
        if not self.serial.connected:
            self._write_log_to(self._active_tab, "err",
                "Nao conectado -- comando nao enviado")
            return
        self.serial.send_frame(cmd, data)
        name  = TX_NAMES.get(cmd, f"0x{cmd:02X}")
        extra = f"  DATA=0x{data:02X}" if data is not None else ""
        self._write_log_to(self._active_tab, "uart",
            f">> TX  [0xAA][0x{cmd:02X}]  {name}{extra}")

    def _refresh_ports(self):
        ports = self.serial.list_ports()
        self._port_cb["values"] = ports
        if ports:
            self._port_var.set(ports[0])

    def _toggle_connect(self):
        if self.serial.connected:
            self._auto_reconnect = False
            self._stop_loop()
            self.serial.disconnect()
            self._auto_reconnect = True
            self._led.config(fg=C["text_dim"])
            self._lbl_conn.config(text="DESCONECTADO", fg=C["text_dim"])
            self._btn_conn.config(text="CONECTAR",     fg=C["green"])
            self._btn_hs.config(state="disabled")
            self._btn_reset.config(state="disabled")
            self._write_log_to(self._active_tab, "sys", "UART desconectada")
        else:
            port   = self._port_var.get()
            baud   = int(self._baud_var.get())
            result = self.serial.connect(port, baud)
            if result is True:
                self._led.config(fg=C["green"])
                self._lbl_conn.config(
                    text=f"CONECTADO  {port}", fg=C["green"])
                self._btn_conn.config(text="DESCONECTAR", fg=C["red"])
                self._btn_hs.config(state="normal")
                self._btn_reset.config(state="normal")
                self._write_log_to(self._active_tab, "sys",
                    f"UART conectada: {port} @ {baud}  --  "
                    "pressione HANDSHAKE para inicializar")
            else:
                messagebox.showerror("Erro de conexao", str(result))

    def _do_handshake(self):
        self._send(SYS_RDY)
        self._write_log_to(self._active_tab, "sys",
            "Handshake enviado -- aguardando SYS_INIT (0x01)...")

    def _do_sw_reset(self):
        if messagebox.askyesno("SW RESET",
                "Confirma reset por software do STM32?\n"
                "O sistema precisara de novo handshake apos o reset."):
            self._send(SW_RESET_MSG)
            self._refresh_fsm_panels(255)
            self._op_mode.set("FSM"); self._upd_hw_colors()

    # =========================================================================
    #  MOTOR DE PASSO
    # =========================================================================
    def _send_steps(self):
        steps = self._step_count.get()
        if self._loop_mode.get():
            self._start_loop(steps)
        else:
            self._send(STPR_TGT_STPS, steps)

    def _start_loop(self, steps):
        if self._step_loop_running:
            return
        self._step_loop_running = True
        self._btn_steps.config(state="disabled")
        self._btn_lstop.config(state="normal")
        self._lbl_loop.config(
            text=f"LOOP  {steps} passos  ({int(1/LOOP_INTERVAL)} cmd/s)")
        def loop():
            while self._step_loop_running:
                if self.serial.connected:
                    self.serial.send_frame(STPR_TGT_STPS, steps)
                time.sleep(LOOP_INTERVAL)
        self._step_loop_thread = threading.Thread(target=loop, daemon=True)
        self._step_loop_thread.start()

    def _stop_loop(self):
        self._step_loop_running = False
        if hasattr(self,"_btn_steps"): self._btn_steps.config(state="normal")
        if hasattr(self,"_btn_lstop"): self._btn_lstop.config(state="disabled")
        if hasattr(self,"_lbl_loop"):  self._lbl_loop.config(text="")

    # =========================================================================
    #  TEMA
    # =========================================================================
    def _toggle_theme(self):
        global C
        self._theme_name = "light" if self._theme_name == "dark" else "dark"
        C = dict(THEMES[self._theme_name])
        self.configure(bg=C["bg"])
        self._recolor_all()
        self._apply_fsm_log_tags()
        self._apply_dbg_log_tags()
        self._show_tab(self._active_tab)   # reaplica cores das tabs
        self._refresh_fsm_panels(self._fsm_id)
        self._upd_hw_colors()
        self._refresh_sensors()
        if hasattr(self,"_canvas"):
            bdr = C["cam_found"] if self._cam_active else C["cam_idle"]
            self._canvas.config(highlightbackground=bdr)
            if not self._cam_active:
                self._draw_placeholder()

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
        style.configure("Horizontal.TScrollbar",
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
                           activebackground=C["bg"], activeforeground=C[ck])
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
        for i, box in enumerate(self._sens_boxes):
            a = self._sens_flags[i].get()
            box.config(bg=C["sens_active"] if a else C["sens_inactive"],
                       fg=C["sens_active_fg"] if a else C["sens_inactive_fg"])
        for i, lbl in enumerate(self._sens_lbls):
            a = self._sens_flags[i].get()
            lbl.config(fg=C["red"] if a else C["text_dim"])
        if hasattr(self,"_err_lbl"):
            self._err_lbl.config(fg=C["log_err"])
        self._btn_theme.config(
            bg=C["border"], fg=C["text_dim"],
            activebackground=C["bg"], activeforeground=C["text_dim"])

    # =========================================================================
    #  HELPERS DE UI
    # =========================================================================
    def _panel(self, parent, label=""):
        f = tk.Frame(parent, bg=C["panel"], bd=1, relief="flat",
                      highlightbackground=C["border"], highlightthickness=1)
        self._tw_list.append((f,{"bg":"panel","highlightbackground":"border"}))
        if label:
            l = tk.Label(f, text=f" {label} ",
                          bg=C["border"], fg=C["text_dim"],
                          font=self.f_sm, padx=6, pady=2)
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

    def _style_ttk(self):
        s = ttk.Style()
        s.theme_use("clam")
        s.configure("TCombobox",
                     fieldbackground=C["border"], background=C["border"],
                     foreground=C["text_head"],
                     selectbackground=C["border"],
                     selectforeground=C["accent"])
        s.configure("Vertical.TScrollbar",
                     background=C["border"], troughcolor=C["bg"],
                     arrowcolor=C["text_dim"])
        s.configure("Horizontal.TScrollbar",
                     background=C["border"], troughcolor=C["bg"],
                     arrowcolor=C["text_dim"])

    # =========================================================================
    #  FECHAMENTO
    # =========================================================================
    def _on_close(self):
        # Oferece salvar logs pendentes
        for tab, lines, lbl in [
                ("fsm", self._fsm_lines, "FSM"),
                ("dbg", self._dbg_lines, "Debug")]:
            if lines:
                ans = messagebox.askyesno(
                    "Salvar log ao fechar",
                    f"O log da aba {lbl} possui {len(lines)} linha(s).\n"
                    "Deseja salvar antes de fechar?")
                if ans:
                    self._save_log(tab)
        self._auto_reconnect = False
        self._stop_loop()
        if self._cam_active and self.camera:
            self.camera.stop()
        if self.serial.connected:
            self.serial.disconnect()
        self.destroy()


# =============================================================================
#  ENTRY POINT
# =============================================================================
if __name__ == "__main__":
    if not QR_AVAILABLE:
        print("=" * 60)
        print("AVISO: opencv-python e/ou Pillow nao encontrados.")
        print("Instale: pip install opencv-python Pillow")
        print("A aba de aplicacao funcionara sem camera.")
        print("=" * 60)
    app = EsteiraApp()
    app.mainloop()