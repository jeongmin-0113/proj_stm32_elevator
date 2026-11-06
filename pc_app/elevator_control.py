"""
Elevator UART Control
proj_stm32_elevator 펌웨어(STM32F411)용 PC 제어 프로그램

패킷: [0xAA][LEN][CMD][PAYLOAD...][CHK]
      LEN = 1 + len(payload),  CHK = (LEN + CMD + sum(payload)) & 0xFF

실행:  python elevator_control.py           (포트 선택 후 연결)
       python elevator_control.py --demo    (보드 없이 시뮬레이션)
"""
import argparse
import ctypes
import queue
import threading
import time
import tkinter as tk
from tkinter import ttk

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover
    serial = None

# ─────────────────────────────────────────── protocol ──
PKT_START = 0xAA
PKT_MAX_LEN = 32
REQ_FLOOR = {1: 0x10, 2: 0x11, 3: 0x12}
REQ_STATE = 0x20
RSP_BASE = 0x80
RSP_OK, RSP_ERROR = 0x00, 0x01
BAUDRATE = 115200
RSP_TIMEOUT = 0.4

FLOORS = (1, 2, 3)
DIR_IDLE, DIR_UP, DIR_DOWN = 0, 1, 2
DIR_TEXT = {DIR_IDLE: "STOP", DIR_UP: "UP", DIR_DOWN: "DOWN"}
DIR_ARROW = {DIR_IDLE: "", DIR_UP: "▲", DIR_DOWN: "▼"}
DEMO_PORT = "DEMO  ·  시뮬레이션"


def build_packet(cmd, payload=b""):
    ln = 1 + len(payload)
    chk = (ln + cmd + sum(payload)) & 0xFF
    return bytes([PKT_START, ln, cmd, *payload, chk])


class PacketParser:
    """바이트 스트림 → ('pkt', cmd, payload, raw) / ('bad', None, b'', raw)"""

    def __init__(self):
        self._reset()

    def _reset(self):
        self.state = "START"
        self.ln = 0
        self.data = bytearray()

    def feed(self, data):
        out = []
        for b in data:
            if self.state == "START":
                if b == PKT_START:
                    self.state = "LEN"
            elif self.state == "LEN":
                if b == 0 or b > PKT_MAX_LEN:
                    self._reset()
                else:
                    self.ln, self.data, self.state = b, bytearray(), "DATA"
            elif self.state == "DATA":
                self.data.append(b)
                if len(self.data) >= self.ln:
                    self.state = "CHK"
            else:  # CHK
                raw = bytes([PKT_START, self.ln, *self.data, b])
                if (self.ln + sum(self.data)) & 0xFF == b:
                    out.append(("pkt", self.data[0], bytes(self.data[1:]), raw))
                else:
                    out.append(("bad", None, b"", raw))
                self._reset()
        return out


def parse_state(payload):
    """0xA0 응답 payload(6B) → [(floor, dir, reqMask), (floor, dir, reqMask)]"""
    if len(payload) < 6:
        return None
    return [(payload[0], payload[1], payload[2]), (payload[3], payload[4], payload[5])]


def mask_floors(mask):
    return [f for f in FLOORS if mask & (1 << f)]


def describe_tx(cmd):
    for f, c in REQ_FLOOR.items():
        if c == cmd:
            return f"{f}F 호출"
    return "상태 요청" if cmd == REQ_STATE else f"CMD 0x{cmd:02X}"


def describe_rx(cmd, payload):
    if payload == bytes([RSP_ERROR]):
        return "ERROR · 체크섬 오류"
    if cmd - RSP_BASE in REQ_FLOOR.values():
        return f"{cmd - RSP_BASE - 0x0F}F 호출 · OK"
    if cmd == REQ_STATE + RSP_BASE:
        st = parse_state(payload)
        if st:
            return "  ".join(f"E{i + 1} {fl}F {DIR_TEXT.get(d, '?')}"
                             for i, (fl, d, _) in enumerate(st))
    return f"CMD 0x{cmd:02X}"


def hexs(raw):
    return " ".join(f"{b:02X}" for b in raw)


# ────────────────────────────────────────── transports ──
class SerialTransport:
    def __init__(self, port):
        if serial is None:
            raise RuntimeError("pyserial이 없습니다  →  pip install pyserial")
        self.port = port
        self.s = serial.Serial(port, BAUDRATE, timeout=0.05)

    def write(self, data):
        self.s.write(data)

    def read(self):
        return self.s.read(self.s.in_waiting or 1)

    def close(self):
        try:
            self.s.close()
        except Exception:
            pass


class _SimCar:
    def __init__(self):
        self.cur, self.req, self.forced = 1, 0, 0
        self.dir, self.arrived, self.pos = DIR_IDLE, False, 1.0


class DemoFirmware:
    """elevator.c / ap_main.c 로직을 그대로 옮긴 가상 보드"""
    SPEED = 0.4  # floor / s

    def __init__(self):
        self.port = "DEMO"
        self.cars = [_SimCar(), _SimCar()]
        self.lock = threading.Lock()
        self.cv = threading.Condition()
        self.out = bytearray()
        self.parser = PacketParser()
        self.alive = True
        threading.Thread(target=self._run, daemon=True).start()

    # -- transport API
    def write(self, data):
        for kind, cmd, _payload, raw in self.parser.feed(data):
            with self.lock:
                if kind == "bad":
                    rsp = build_packet(raw[2] + RSP_BASE, bytes([RSP_ERROR]))
                elif cmd in REQ_FLOOR.values():
                    self._select(cmd - 0x0F)
                    rsp = build_packet(cmd + RSP_BASE, bytes([RSP_OK]))
                elif cmd == REQ_STATE:
                    rsp = build_packet(cmd + RSP_BASE, self._state())
                else:
                    continue
            with self.cv:
                self.out += rsp
                self.cv.notify()

    def read(self):
        with self.cv:
            if not self.out:
                self.cv.wait(0.05)
            data = bytes(self.out)
            self.out.clear()
        return data

    def close(self):
        self.alive = False

    # -- firmware logic
    def _state(self):
        a, b = self.cars
        return bytes([a.cur, a.dir, a.req, b.cur, b.dir, b.req])

    @staticmethod
    def _cost(e, f):
        dist = abs(e.cur - f)
        if e.dir == DIR_IDLE or (e.dir == DIR_UP and f > e.cur) or (e.dir == DIR_DOWN and f < e.cur):
            return dist
        return dist + 3

    @staticmethod
    def _cancel(e, f):
        if e.arrived:
            return
        e.req &= ~(1 << f)
        if e.dir == DIR_IDLE:
            return
        above = e.req >> (e.cur + 1)
        below = e.req & ((1 << e.cur) - 1)
        if e.dir == DIR_UP and not above and e.cur < 3:
            e.forced |= 1 << (e.cur + 1)
        if e.dir == DIR_DOWN and not below and e.cur > 1:
            e.forced |= 1 << (e.cur - 1)

    def _select(self, f):
        bit = 1 << f
        for e in self.cars:
            if e.req & bit:
                self._cancel(e, f)
                return
        for e in self.cars:
            if e.forced & bit:
                e.req |= bit
                e.forced = 0
                return
        for e in self.cars:
            if e.dir == DIR_IDLE and e.cur == f:
                return
        c0, c1 = self._cost(self.cars[0], f), self._cost(self.cars[1], f)
        (self.cars[1] if c0 > c1 else self.cars[0]).req |= bit

    def _set_cur(self, e, f):
        e.cur = f
        if (e.req | e.forced) & (1 << f):
            e.arrived = True

    def _task(self, dt):
        for e in self.cars:
            if e.arrived:
                e.arrived = False
                e.req &= ~(1 << e.cur)
                e.forced &= ~(1 << e.cur)
                e.pos = float(e.cur)
            mask = e.req | e.forced
            above = mask >> (e.cur + 1)
            below = mask & ((1 << e.cur) - 1)
            if e.dir == DIR_UP and (e.req >> (e.cur + 1)):
                e.forced = 0
            if e.dir == DIR_DOWN and (e.req & ((1 << e.cur) - 1)):
                e.forced = 0
            if e.dir == DIR_UP and not above:
                e.dir = DIR_IDLE
            if e.dir == DIR_DOWN and not below:
                e.dir = DIR_IDLE
            if e.dir == DIR_IDLE:
                e.dir = DIR_UP if above else (DIR_DOWN if below else DIR_IDLE)
            if e.dir != DIR_IDLE:
                prev = e.pos
                e.pos += self.SPEED * dt * (1 if e.dir == DIR_UP else -1)
                e.pos = max(1.0, min(3.0, e.pos))
                for f in FLOORS:
                    if (e.dir == DIR_UP and prev < f <= e.pos) or (e.dir == DIR_DOWN and e.pos <= f < prev):
                        self._set_cur(e, f)

    def _run(self):
        last = time.monotonic()
        while self.alive:
            time.sleep(0.02)
            now = time.monotonic()
            with self.lock:
                self._task(now - last)
            last = now


# ──────────────────────────────────────────────── link ──
class Link:
    """요청 1개 → 응답 대기(타임아웃) → 다음 요청.
    펌웨어는 처리 중인 패킷이 있으면 새 패킷을 무시하므로 한 번에 하나씩 보낸다."""

    def __init__(self, transport, emit):
        self.t = transport
        self.emit = emit
        self.tx_q = queue.Queue()
        self.rsp_q = queue.Queue()
        self.parser = PacketParser()
        self.running = True
        self.pending = False
        threading.Thread(target=self._rx_loop, daemon=True).start()
        threading.Thread(target=self._tx_loop, daemon=True).start()

    def request(self, cmd, payload=b"", tag=None):
        self.tx_q.put((cmd, bytes(payload), tag))

    def busy(self):
        return self.pending or not self.tx_q.empty()

    def close(self):
        self.running = False
        self.t.close()

    def _rx_loop(self):
        while self.running:
            try:
                data = self.t.read()
            except Exception as ex:
                if self.running:
                    self.running = False
                    self.emit(("fatal", str(ex)))
                return
            for kind, cmd, payload, raw in self.parser.feed(data):
                if kind == "pkt":
                    self.rsp_q.put((cmd, payload, raw))
                else:
                    self.emit(("bad", raw))

    def _tx_loop(self):
        while self.running:
            try:
                cmd, payload, tag = self.tx_q.get(timeout=0.1)
            except queue.Empty:
                continue
            self.pending = True
            pkt = build_packet(cmd, payload)
            try:
                self.t.write(pkt)
            except Exception as ex:
                self.running = False
                self.emit(("fatal", str(ex)))
                return
            self.emit(("tx", cmd, pkt, tag))
            deadline = time.monotonic() + RSP_TIMEOUT
            ok = False
            while True:
                left = deadline - time.monotonic()
                if left <= 0:
                    break
                try:
                    rcmd, rpl, raw = self.rsp_q.get(timeout=left)
                except queue.Empty:
                    break
                matched = rcmd == cmd + RSP_BASE
                self.emit(("rx", rcmd, rpl, raw, tag if matched else None))
                if matched:
                    ok = True
                    break
            if not ok:
                self.emit(("timeout", cmd, tag))
            self.pending = False


# ─────────────────────────────────────────────── theme ──
BG = "#E4E5E7"        # 바탕 회색
CARD = "#F4F4F5"      # 카드
SURFACE = "#FFFFFF"
SHAFT = "#E3E4E7"
LINE = "#CDCFD3"
TEXT = "#1D1E21"
SUB = "#80838A"
MUTE = "#B4B7BC"
CAR = "#2A2C30"
CAR_LINE = "#3E4146"
RED = "#E3342F"
RED_DARK = "#C42722"
GHOST = "#E7E8EA"
GHOST_HOVER = "#D9DADD"
FONT = "Malgun Gothic"
NUM = "Segoe UI"
MONO = "Consolas"


def round_rect(c, x1, y1, x2, y2, r, **kw):
    pts = [x1 + r, y1, x2 - r, y1, x2, y1, x2, y1 + r, x2, y2 - r, x2, y2,
           x2 - r, y2, x1 + r, y2, x1, y2, x1, y2 - r, x1, y1 + r, x1, y1]
    return c.create_polygon(pts, smooth=True, **kw)


class FlatButton(tk.Label):
    def __init__(self, master, text, command, kind="ghost", bg=CARD, **kw):
        super().__init__(master, text=text, cursor="hand2", padx=16, pady=6,
                         font=(FONT, 9, "bold"), bd=0, **kw)
        self.command, self.kind, self.hover = command, kind, False
        self.bind("<Enter>", lambda e: self._set_hover(True))
        self.bind("<Leave>", lambda e: self._set_hover(False))
        self.bind("<ButtonRelease-1>", lambda e: self.command and self.command())
        self._paint()

    def set(self, text=None, kind=None):
        if text is not None:
            self.configure(text=text)
        if kind is not None:
            self.kind = kind
        self._paint()

    def _set_hover(self, v):
        self.hover = v
        self._paint()

    def _paint(self):
        if self.kind == "primary":
            self.configure(bg=RED_DARK if self.hover else RED, fg="white")
        else:
            self.configure(bg=GHOST_HOVER if self.hover else GHOST, fg=TEXT)


class Toggle(tk.Canvas):
    def __init__(self, master, value, command, s, bg=CARD):
        self.w, self.h = s(34), s(18)
        super().__init__(master, width=self.w, height=self.h, bg=bg,
                         highlightthickness=0, cursor="hand2")
        self.value, self.command = value, command
        self.bind("<Button-1>", self._click)
        self._draw()

    def _click(self, _e):
        self.value = not self.value
        self._draw()
        self.command(self.value)

    def _draw(self):
        self.delete("all")
        w, h = self.w, self.h
        col = RED if self.value else MUTE
        self.create_oval(0, 0, h, h, fill=col, outline=col)
        self.create_oval(w - h, 0, w, h, fill=col, outline=col)
        self.create_rectangle(h / 2, 0, w - h / 2, h, fill=col, outline=col)
        p = 3
        x = w - h + p if self.value else p
        self.create_oval(x, p, x + h - 2 * p, h - p, fill=SURFACE, outline=SURFACE)


def card(master, title, s):
    f = tk.Frame(master, bg=CARD, padx=s(20), pady=s(16))
    head = tk.Frame(f, bg=CARD)
    head.pack(fill="x", pady=(0, s(12)))
    tk.Label(head, text=title, bg=CARD, fg=SUB, font=(FONT, 8, "bold")).pack(side="left")
    return f, head


# ──────────────────────────────────────────── building ──
class BuildingView(tk.Canvas):
    """층 단면도: [층 표시] [E1 승강로] [호출 버튼] [E2 승강로]"""

    def __init__(self, master, app, s):
        self.s = s
        self.W, self.H = s(600), s(540)
        super().__init__(master, width=self.W, height=self.H, bg=CARD, highlightthickness=0)
        self.app = app
        self.top, self.fh = s(80), s(130)
        self.bottom = self.top + 3 * self.fh
        self.shafts = [(s(130), s(270)), (s(430), s(570))]
        self.call_x = s(350)
        self.call_r = s(27)
        self.hover = None
        self.bind("<Motion>", self._motion)
        self.bind("<Leave>", lambda e: self._set_hover(None))
        self.bind("<Button-1>", self._click)
        self._draw_static()

    def floor_y(self, pos):  # 해당 층 바닥 y
        return self.bottom - (pos - 1) * self.fh

    def band_cy(self, f):
        return self.floor_y(f) - self.fh / 2

    def _draw_static(self):
        s = self.s
        for f in FLOORS:
            y = self.floor_y(f)
            self.create_line(s(20), y, self.W - s(10), y, fill=LINE, width=2 if f == 1 else 1)
            self.create_text(s(30), self.band_cy(f), text=f"{f}", anchor="w",
                             fill=TEXT, font=(NUM, 26, "bold"))
            self.create_text(s(66), self.band_cy(f) + s(9), text="F", anchor="w",
                             fill=SUB, font=(NUM, 11, "bold"))
        for i, (x1, x2) in enumerate(self.shafts):
            self.create_rectangle(x1, self.top, x2, self.bottom, fill=SHAFT, outline="")
            self.create_text(x1, s(44), text=f"E{i + 1}", anchor="w", fill=TEXT, font=(NUM, 15, "bold"))
        self.create_text(self.call_x, s(44), text="HALL CALL", fill=SUB, font=(FONT, 8, "bold"))

    def _call_hit(self, x, y):
        for f in FLOORS:
            if (x - self.call_x) ** 2 + (y - self.band_cy(f)) ** 2 <= (self.call_r + 4) ** 2:
                return f
        return None

    def _motion(self, e):
        self._set_hover(self._call_hit(e.x, e.y))

    def _set_hover(self, f):
        if f != self.hover:
            self.hover = f
            self.configure(cursor="hand2" if f else "")

    def _click(self, e):
        f = self._call_hit(e.x, e.y)
        if f:
            self.app.call(f)

    def render(self, vis_pos, state, connected):
        s = self.s
        self.delete("dyn")
        calls = {}
        if state:
            for i, (_, _, mask) in enumerate(state):
                for f in mask_floors(mask):
                    calls.setdefault(f, []).append(i)

        # 엘리베이터 카
        for i, (x1, x2) in enumerate(self.shafts):
            fl, d, mask = state[i] if state else (1, DIR_IDLE, 0)
            yb = self.floor_y(vis_pos[i]) - s(8)
            yt = yb - (self.fh - s(26))
            cx1, cx2 = x1 + s(12), x2 - s(12)
            fill = CAR if connected else MUTE
            round_rect(self, cx1, yt, cx2, yb, s(8), fill=fill, outline="", tags="dyn")
            mid = (cx1 + cx2) / 2
            self.create_line(cx1 + s(14), yb - s(10), cx2 - s(14), yb - s(10),
                             fill=CAR_LINE if connected else "#A6A9AE", tags="dyn")
            self.create_text(mid, (yt + yb) / 2 + s(4), text=str(fl) if connected else "–",
                             fill=SURFACE, font=(NUM, 24, "bold"), tags="dyn")
            if connected and d != DIR_IDLE:
                self.create_text(mid, yt + s(16), text=DIR_ARROW[d], fill=RED,
                                 font=(NUM, 10, "bold"), tags="dyn")
            # 상단 방향 표시
            label = DIR_TEXT.get(d, "?") if connected else "OFFLINE"
            self.create_text(x2, s(44), text=label, anchor="e",
                             fill=RED if connected and d != DIR_IDLE else SUB,
                             font=(NUM, 9, "bold"), tags="dyn")
            # 배정된 층 마커 (호출 버튼 쪽 승강로 벽)
            edge = x2 if i == 0 else x1
            for f in mask_floors(mask) if connected else []:
                cy = self.band_cy(f)
                self.create_rectangle(edge - s(2), cy - s(14), edge + s(2), cy + s(14),
                                      fill=RED, outline="", tags="dyn")
            # 하단 상태 텍스트
            self.create_text((x1 + x2) / 2, self.bottom + s(34),
                             text=f"{label}  ·  {fl}F" if connected else "연결 안 됨",
                             fill=TEXT if connected else SUB, font=(FONT, 10, "bold"), tags="dyn")

        # 호출 버튼
        for f in FLOORS:
            cx, cy, r = self.call_x, self.band_cy(f), self.call_r
            active = connected and f in calls
            hov = connected and self.hover == f
            if active:
                fill, outline, fg = (RED_DARK if hov else RED), "", SURFACE
            else:
                fill, outline, fg = SURFACE, (TEXT if hov else LINE), (TEXT if connected else MUTE)
            self.create_oval(cx - r, cy - r, cx + r, cy + r, fill=fill, outline=outline,
                             width=s(2) if hov and not active else 1, tags="dyn")
            self.create_text(cx, cy, text=str(f), fill=fg, font=(NUM, 15, "bold"), tags="dyn")
            if active:
                cap = " · ".join(f"E{i + 1}" for i in calls[f]) + " 배정"
                self.create_text(cx, cy + r + s(14), text=cap, fill=RED,
                                 font=(FONT, 8, "bold"), tags="dyn")


# ──────────────────────────────────────────────── app ──
class App:
    POLL_MS = 200

    def __init__(self, root, demo=False):
        self.root = root
        self.S = root.winfo_fpixels("1i") / 96.0
        s = self.s = lambda v: int(round(v * self.S))

        root.title("Elevator UART Control")
        root.configure(bg=BG)
        root.geometry(f"{s(1080)}x{s(700)}")
        root.minsize(s(1000), s(660))

        self.link = None
        self.gen = 0
        self.events = queue.Queue()
        self.state = None
        self.vis = [1.0, 1.0]
        self.move_t = [time.monotonic()] * 2
        self.prev_key = [None, None]
        self.poll_on = True
        self.hide_poll = True
        self.timeouts = 0
        self.last_frame = time.monotonic()

        self._style()
        self._build_header()
        self._build_body()
        self.refresh_ports()
        if demo:
            self.port_var.set(DEMO_PORT)
            self.connect()

        for k in "123":
            root.bind(k, lambda e, f=int(k): self.call(f))
        root.bind("s", lambda e: self.request_state())
        root.protocol("WM_DELETE_WINDOW", self.on_close)

        self._pump()
        self._frame()
        self._poll()

    # -- ui
    def _style(self):
        st = ttk.Style()
        st.theme_use("clam")
        st.configure("Port.TCombobox", fieldbackground=SURFACE, background=SURFACE, foreground=TEXT,
                     bordercolor=LINE, lightcolor=SURFACE, darkcolor=SURFACE, arrowcolor=TEXT,
                     padding=self.s(5), arrowsize=self.s(12))
        st.map("Port.TCombobox", fieldbackground=[("readonly", SURFACE)],
               foreground=[("readonly", TEXT)], selectbackground=[("readonly", SURFACE)],
               selectforeground=[("readonly", TEXT)], bordercolor=[("focus", TEXT)])
        self.root.option_add("*TCombobox*Listbox.background", SURFACE)
        self.root.option_add("*TCombobox*Listbox.foreground", TEXT)
        self.root.option_add("*TCombobox*Listbox.selectBackground", RED)
        self.root.option_add("*TCombobox*Listbox.selectForeground", SURFACE)
        self.root.option_add("*TCombobox*Listbox.font", (FONT, 9))

    def _build_header(self):
        s = self.s
        h = tk.Frame(self.root, bg=CARD, padx=s(24), pady=s(14))
        h.pack(fill="x")
        tk.Frame(self.root, bg=LINE, height=1).pack(fill="x")

        left = tk.Frame(h, bg=CARD)
        left.pack(side="left")
        tk.Frame(left, bg=RED, width=s(4), height=s(26)).pack(side="left", padx=(0, s(12)))
        tk.Label(left, text="ELEVATOR", bg=CARD, fg=TEXT, font=(NUM, 15, "bold")).pack(side="left")
        tk.Label(left, text="UART CONTROL", bg=CARD, fg=SUB, font=(NUM, 10)).pack(side="left", padx=(s(8), 0), pady=(s(4), 0))

        right = tk.Frame(h, bg=CARD)
        right.pack(side="right")
        self.status_dot = tk.Canvas(right, width=s(10), height=s(10), bg=CARD, highlightthickness=0)
        self.status_dot.pack(side="left", padx=(0, s(6)))
        self.status_lbl = tk.Label(right, text="연결 안 됨", bg=CARD, fg=SUB, font=(FONT, 9))
        self.status_lbl.pack(side="left", padx=(0, s(18)))

        self.port_var = tk.StringVar()
        self.port_box = ttk.Combobox(right, textvariable=self.port_var, state="readonly",
                                     style="Port.TCombobox", width=34, font=(FONT, 9))
        self.port_box.pack(side="left", padx=(0, s(6)))
        FlatButton(right, "↻", self.refresh_ports).pack(side="left", padx=(0, s(6)))
        self.conn_btn = FlatButton(right, "연결", self.toggle_connect, kind="primary")
        self.conn_btn.pack(side="left")
        self._set_status(False, "연결 안 됨")

    def _build_body(self):
        s = self.s
        body = tk.Frame(self.root, bg=BG, padx=s(20), pady=s(20))
        body.pack(fill="both", expand=True)

        side = tk.Frame(body, bg=BG, width=s(350))
        side.pack(side="right", fill="y", padx=(s(20), 0))
        side.pack_propagate(False)

        bcard, _ = card(body, "ELEVATORS", s)
        bcard.pack(side="left", fill="both", expand=True)
        self.view = BuildingView(bcard, self, s)
        self.view.pack(expand=True)

        # status card
        sc, head = card(side, "STATUS", s)
        sc.pack(fill="x")
        Toggle(head, True, self._set_poll, s).pack(side="right")
        tk.Label(head, text=f"자동 갱신 {self.POLL_MS}ms", bg=CARD, fg=SUB,
                 font=(FONT, 8)).pack(side="right", padx=(0, s(8)))

        grid = tk.Frame(sc, bg=CARD)
        grid.pack(fill="x")
        for c, t in enumerate(("", "층", "방향", "배정 층")):
            tk.Label(grid, text=t, bg=CARD, fg=SUB, font=(FONT, 8)).grid(row=0, column=c, sticky="w", pady=(0, s(4)))
        self.st_cells = []
        for r in range(2):
            tk.Frame(grid, bg=LINE, height=1).grid(row=1 + r * 2, column=0, columnspan=4, sticky="ew")
            row = []
            tk.Label(grid, text=f"E{r + 1}", bg=CARD, fg=TEXT, font=(NUM, 11, "bold")).grid(
                row=2 + r * 2, column=0, sticky="w", pady=s(8))
            for c, (w, fnt) in enumerate(((6, (NUM, 16, "bold")), (8, (NUM, 10, "bold")), (9, (NUM, 10)))):
                lb = tk.Label(grid, text="–", bg=CARD, fg=TEXT, font=fnt, width=w, anchor="w")
                lb.grid(row=2 + r * 2, column=1 + c, sticky="w")
                row.append(lb)
            self.st_cells.append(row)
        grid.columnconfigure(0, minsize=s(44))

        btns = tk.Frame(sc, bg=CARD)
        btns.pack(fill="x", pady=(s(12), 0))
        FlatButton(btns, "상태 요청", self.request_state).pack(side="left")
        tk.Label(btns, text="단축키  1 · 2 · 3 호출   S 상태", bg=CARD, fg=MUTE,
                 font=(FONT, 8)).pack(side="right")

        # log card
        lc, lhead = card(side, "PACKET LOG", s)
        lc.pack(fill="both", expand=True, pady=(s(16), 0))
        FlatButton(lhead, "지우기", self.clear_log).pack(side="right")
        Toggle(lhead, True, self._set_hide_poll, s).pack(side="right", padx=(0, s(10)))
        tk.Label(lhead, text="폴링 숨김", bg=CARD, fg=SUB, font=(FONT, 8)).pack(side="right", padx=(0, s(8)))

        self.log = tk.Text(lc, bg=CARD, fg=TEXT, bd=0, highlightthickness=0, wrap="none",
                           font=(MONO, 9), state="disabled", cursor="arrow", padx=0, pady=0,
                           spacing1=s(2), spacing3=s(2))
        self.log.pack(fill="both", expand=True)
        self.log.tag_configure("time", foreground=MUTE)
        self.log.tag_configure("tx", foreground=SUB, font=(MONO, 9, "bold"))
        self.log.tag_configure("rx", foreground=TEXT, font=(MONO, 9, "bold"))
        self.log.tag_configure("hex", foreground=TEXT)
        self.log.tag_configure("desc", foreground=SUB, font=(FONT, 8))
        self.log.tag_configure("err", foreground=RED, font=(MONO, 9, "bold"))
        self.log.tag_configure("errdesc", foreground=RED, font=(FONT, 8))

    def _set_status(self, on, text, error=False):
        s = self.s
        self.status_dot.delete("all")
        col = RED if (on or error) else MUTE
        self.status_dot.create_oval(1, 1, s(9), s(9), fill=col, outline=col)
        self.status_lbl.configure(text=text, fg=RED if error else (TEXT if on else SUB))

    # -- log
    def _log(self, kind, raw_or_text, desc=""):
        self.log.configure(state="normal")
        t = time.strftime("%H:%M:%S") + f".{int(time.time() * 1000) % 1000:03d}"
        self.log.insert("end", t + "  ", "time")
        if kind in ("tx", "rx"):
            self.log.insert("end", kind.upper() + "  ", kind)
            self.log.insert("end", hexs(raw_or_text) + "\n", "hex")
            self.log.insert("end", " " * 14 + desc + "\n", "desc")
        else:
            self.log.insert("end", "!!  ", "err")
            self.log.insert("end", raw_or_text + "\n", "errdesc")
        lines = int(self.log.index("end-1c").split(".")[0])
        if lines > 600:
            self.log.delete("1.0", f"{lines - 600}.0")
        self.log.see("end")
        self.log.configure(state="disabled")

    def clear_log(self):
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    # -- connection
    def refresh_ports(self):
        items = []
        if serial is not None:
            ports = sorted(serial.tools.list_ports.comports(), key=lambda p: p.device)
            items = [f"{p.device}  ·  {p.description}" for p in ports]
        items.append(DEMO_PORT)
        self.port_box["values"] = items
        cur = self.port_var.get()
        if cur not in items:
            stlink = [i for i in items if "STLink" in i or "STMicro" in i]
            self.port_var.set(stlink[0] if stlink else items[0])

    def toggle_connect(self):
        self.disconnect() if self.link else self.connect()

    def connect(self):
        sel = self.port_var.get()
        try:
            if sel == DEMO_PORT:
                tr = DemoFirmware()
            else:
                tr = SerialTransport(sel.split()[0])
        except Exception as ex:
            self._set_status(False, "연결 실패", error=True)
            self._log("err", f"연결 실패 · {ex}")
            return
        self.gen += 1
        gen = self.gen
        self.link = Link(tr, lambda ev: self.events.put((gen, ev)))
        self.timeouts = 0
        self._set_status(True, f"연결됨 · {tr.port} · {BAUDRATE}")
        self.conn_btn.set("연결 해제", "ghost")
        self.port_box.configure(state="disabled")
        self.request_state()

    def disconnect(self, reason=None):
        if self.link:
            self.link.close()
        self.link = None
        self.gen += 1
        self.state = None
        if reason:
            self._set_status(False, "연결 끊김", error=True)
            self._log("err", reason)
        else:
            self._set_status(False, "연결 안 됨")
        self.conn_btn.set("연결", "primary")
        self.port_box.configure(state="readonly")
        for row in self.st_cells:
            for lb in row:
                lb.configure(text="–", fg=TEXT)

    def on_close(self):
        if self.link:
            self.link.close()
        self.root.destroy()

    # -- commands
    def call(self, f):
        if not self.link:
            self._flash_status()
            return
        self.link.request(REQ_FLOOR[f], tag="call")
        self.link.request(REQ_STATE, tag="state")

    def request_state(self):
        if self.link:
            self.link.request(REQ_STATE, tag="state")
        else:
            self._flash_status()

    def _flash_status(self):
        self._set_status(False, "먼저 포트를 연결하세요", error=True)
        self.root.after(1500, lambda: self.link or self._set_status(False, "연결 안 됨"))

    def _set_poll(self, v):
        self.poll_on = v

    def _set_hide_poll(self, v):
        self.hide_poll = v

    # -- loops
    def _poll(self):
        if self.link and self.poll_on and not self.link.busy():
            self.link.request(REQ_STATE, tag="poll")
        self.root.after(self.POLL_MS, self._poll)

    def _pump(self):
        try:
            while True:
                gen, ev = self.events.get_nowait()
                if gen == self.gen:
                    self._handle(ev)
        except queue.Empty:
            pass
        self.root.after(15, self._pump)

    def _handle(self, ev):
        kind = ev[0]
        quiet = lambda tag: self.hide_poll and tag == "poll"
        if kind == "tx":
            _, cmd, raw, tag = ev
            if not quiet(tag):
                self._log("tx", raw, describe_tx(cmd))
        elif kind == "rx":
            _, cmd, payload, raw, tag = ev
            if cmd == REQ_STATE + RSP_BASE:
                st = parse_state(payload)
                if st:
                    self._apply_state(st)
            if self.timeouts:
                self.timeouts = 0
                self._set_status(True, f"연결됨 · {self.link.t.port} · {BAUDRATE}")
            if not quiet(tag):
                self._log("rx", raw, describe_rx(cmd, payload))
        elif kind == "timeout":
            _, cmd, tag = ev
            self.timeouts += 1
            if self.timeouts == 3:
                self._set_status(True, "응답 없음 · 보드를 확인하세요", error=True)
            if not quiet(tag) or self.timeouts == 3:
                self._log("err", f"응답 없음 · {describe_tx(cmd)}")
        elif kind == "bad":
            self._log("err", f"체크섬 불일치 · {hexs(ev[1])}")
        elif kind == "fatal":
            self.disconnect(f"포트 오류 · {ev[1]}")

    def _apply_state(self, st):
        now = time.monotonic()
        for i, (fl, d, mask) in enumerate(st):
            key = (fl, d)
            if key != self.prev_key[i]:
                self.prev_key[i] = key
                self.move_t[i] = now
            cells = self.st_cells[i]
            cells[0].configure(text=f"{fl}F")
            cells[1].configure(text=(DIR_ARROW[d] + " " + DIR_TEXT[d]).strip() if d in DIR_TEXT else "?",
                               fg=RED if d != DIR_IDLE else TEXT)
            fl_list = mask_floors(mask)
            cells[2].configure(text=" · ".join(f"{f}F" for f in fl_list) if fl_list else "–",
                               fg=RED if fl_list else SUB)
        self.state = st

    def _frame(self):
        """보드는 '마지막 통과 층 + 방향'만 알려주므로, 이동 중에는 진행 방향으로 서서히 밀어 표현"""
        now = time.monotonic()
        dt = now - self.last_frame
        self.last_frame = now
        for i in range(2):
            if self.state:
                fl, d, _ = self.state[i]
                drift = min(0.8, (now - self.move_t[i]) * 0.38)
                target = fl + (drift if d == DIR_UP else -drift if d == DIR_DOWN else 0)
                target = max(1.0, min(3.0, target))
            else:
                target = 1.0
            self.vis[i] += (target - self.vis[i]) * min(1.0, dt * 8)
        self.view.render(self.vis, self.state, self.link is not None)
        self.root.after(33, self._frame)


def main():
    ap = argparse.ArgumentParser(description="Elevator UART Control")
    ap.add_argument("--demo", action="store_true", help="보드 없이 시뮬레이션으로 실행")
    args = ap.parse_args()
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except Exception:
        pass
    root = tk.Tk()
    App(root, demo=args.demo)
    root.mainloop()


if __name__ == "__main__":
    main()
