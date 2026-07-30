import sys
import os

# ── Wayland workaround ─────────────────────────────────────
# On Wayland, compositors (KWin) ignore client window-position requests.
# Force the X11 backend via XWayland, which allows free window placement.
# This must be set BEFORE QApplication is constructed.
if os.environ.get("QT_QPA_PLATFORM") is None:
    os.environ["QT_QPA_PLATFORM"] = "xcb"

import random
from datetime import datetime
from pathlib import Path
from PyQt6.QtWidgets import QApplication, QWidget, QLabel, QMenu
from PyQt6.QtGui import QPixmap, QAction
from PyQt6.QtCore import Qt, QTimer, QPoint, QElapsedTimer

# ── global log ──────────────────────────────────────────────
LOG_PATH = Path(__file__).with_suffix(".log")
_LOG_FILE = None


def _log_init() -> None:
    global _LOG_FILE
    _LOG_FILE = open(LOG_PATH, "a", encoding="utf-8")
    _LOG_FILE.write(f"\n{'='*60}\n")
    _LOG_FILE.write(f"Started {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
    _LOG_FILE.flush()


def _log(msg: str) -> None:
    stamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    line = f"[{stamp}] {msg}"
    print(line, flush=True)
    if _LOG_FILE:
        _LOG_FILE.write(line + "\n")
        _LOG_FILE.flush()


class DesktopPet(QWidget):
    """Desktop virtual pet — Wayland-compatible via Dialog window type.

    On Wayland, normal top-level windows cannot set their own position.
    ``Qt.WindowType.Dialog`` uses the xdg-toplevel protocol which some
    compositors (KDE KWin in particular) allow to be repositioned.
    """

    # ── constants ──────────────────────────────────────────
    PET_SIZE: int = 180
    ANIM_FPS: int = 60
    WALK_DURATION_MIN: int = 800
    WALK_DURATION_MAX: int = 2000
    WALK_FRAME_MS: int = 150      # ms between walk animation frames (4 frames per cycle)
    BEHAVIOR_INTERVAL: int = 4000
    INITIAL_WALK_DELAY: int = 1500
    HAPPY_TIMEOUT: int = 1000
    CLICKED_TIMEOUT: int = 800

    STATE_FILES: dict[str, str] = {
        "idle":    "pet_longhair_transparent_idle.png",
        "happy":   "pet_longhair_transparent_happy.png",
        "clicked": "pet_longhair_transparent_clicked.png",
        "sleep":   "pet_longhair_transparent_sleep.png",
    }

    # 4-frame walk animation cycles (left / right)
    WALK_LEFT_FILES: tuple[str, ...] = (
        "图库/jdd_walk_left_1.png",
        "图库/jdd_walk_left_2.png",
        "图库/jdd_walk_left_3.png",
        "图库/jdd_walk_left_4.png",
    )
    WALK_RIGHT_FILES: tuple[str, ...] = (
        "图库/jdd_walk_right_1.png",
        "图库/jdd_walk_right_2.png",
        "图库/jdd_walk_right_3.png",
        "图库/jdd_walk_right_4.png",
    )

    # ── init ───────────────────────────────────────────────
    def __init__(self) -> None:
        super().__init__()
        _log("─ 桌面宠物启动 ─")

        # Use Dialog instead of Tool — some Wayland compositors (KWin) allow
        # Dialog windows to set their position via xdg-toplevel configure.
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Dialog
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.resize(self.PET_SIZE, self.PET_SIZE)

        self.label = QLabel(self)
        self.label.resize(self.PET_SIZE, self.PET_SIZE)

        # ── detect platform for logging ─────────────────────
        plat = QApplication.instance().platformName() if QApplication.instance() else "?"
        _log(f"Qt 平台: {plat}")

        # ── state ───────────────────────────────────────────
        self.state: str = "idle"
        self._last_pixmap_state: str | None = None
        self.drag_position: QPoint | None = None

        # walk-animation state
        self._target_pos: QPoint | None = None
        self._walk_start_pos: QPoint | None = None
        self._walk_clock: QElapsedTimer = QElapsedTimer()
        self._walk_duration_ms: int = 0
        self._walk_tick_count: int = 0
        self._walk_id: int = 0
        self._walk_direction: str = "right"  # face right by default
        self._walk_frames: dict[str, list[QPixmap]] = {}  # {"left": [...], "right": [...]}

        self._app_clock = QElapsedTimer()
        self._app_clock.start()

        self.load_images()

        # ── initial position ────────────────────────────────
        geo = QApplication.primaryScreen().availableGeometry()
        _log(f"屏幕可用区域: left={geo.x()} top={geo.y()} {geo.width()}x{geo.height()}")
        cx = geo.x() + (geo.width()  - self.PET_SIZE) // 2
        cy = geo.y() + (geo.height() - self.PET_SIZE) // 2
        self.move(max(geo.x(), cx), max(geo.y(), cy))
        # Log whether move was respected
        actual = self.pos()
        _log(f"请求位置: ({max(geo.x(), cx)},{max(geo.y(), cy)})  实际位置: ({actual.x()},{actual.y()})")
        if actual.x() != max(geo.x(), cx) or actual.y() != max(geo.y(), cy):
            _log("!! 警告: Wayland compositor 忽略了窗口位置请求 !!")
            _log("!! 尝试设置环境变量: QT_QPA_PLATFORM=xcb python3 desktop_pet.py")

        _log(f"state={self.state}")

        # ── timers ─────────────────────────────────────────
        self._anim_timer = QTimer(self)
        self._anim_timer.timeout.connect(self._animation_tick)
        self._anim_timer.start(1000 // self.ANIM_FPS)
        _log(f"动画计时器已启动  interval={1000//self.ANIM_FPS}ms  (目标 {self.ANIM_FPS}fps)")

        self._behavior_timer = QTimer(self)
        self._behavior_timer.timeout.connect(self._decide_behavior)
        self._behavior_timer.start(self.BEHAVIOR_INTERVAL)
        _log(f"行为计时器已启动  interval={self.BEHAVIOR_INTERVAL}ms")
        _log(f"首轮行为将在 {self.INITIAL_WALK_DELAY}ms 后触发")

        QTimer.singleShot(self.INITIAL_WALK_DELAY, self._decide_behavior)

        self.show()
        _log(f"窗口已显示  pos=({self.x()},{self.y()})  state={self.state}")
        _log(f"日志文件: {LOG_PATH}")

    # ── image loading ──────────────────────────────────────
    def load_images(self) -> None:
        base = os.path.dirname(os.path.abspath(__file__))
        self.images: dict[str, QPixmap] = {}
        missing = []

        for state, filename in self.STATE_FILES.items():
            path = os.path.join(base, filename)
            if os.path.exists(path):
                self.images[state] = QPixmap(path).scaled(
                    self.PET_SIZE, self.PET_SIZE,
                    Qt.AspectRatioMode.KeepAspectRatio,
                    Qt.TransformationMode.SmoothTransformation,
                )
                _log(f"  已加载: {filename}")
            else:
                missing.append(filename)
                _log(f"  !! 缺失: {filename}")

        if not self.images:
            raise FileNotFoundError(
                f"No pet images found under {base} – expected: "
                + ", ".join(self.STATE_FILES.values())
            )
        if missing:
            _log(f"  !! 警告: {len(missing)} 张图片缺失")
        _log(f"共加载 {len(self.images)}/{len(self.STATE_FILES)} 张静态图片")

        # ── load walk animation frames ─────────────────────
        for direction, files in (("left", self.WALK_LEFT_FILES),
                                  ("right", self.WALK_RIGHT_FILES)):
            frames: list[QPixmap] = []
            for filename in files:
                path = os.path.join(base, filename)
                if os.path.exists(path):
                    frames.append(QPixmap(path).scaled(
                        self.PET_SIZE, self.PET_SIZE,
                        Qt.AspectRatioMode.KeepAspectRatio,
                        Qt.TransformationMode.SmoothTransformation,
                    ))
                else:
                    _log(f"  !! 缺失走动帧: {filename}")
            self._walk_frames[direction] = frames
            _log(f"  走动帧 {direction}: {len(frames)}/{len(files)} 张")

        self._set_pixmap("idle")

    # ── helpers ────────────────────────────────────────────
    def _log_elapsed(self) -> int:
        return self._app_clock.elapsed()

    def _set_pixmap(self, state: str) -> None:
        if state in self.images and state != self._last_pixmap_state:
            self.label.setPixmap(self.images[state])
            prev = self._last_pixmap_state
            self._last_pixmap_state = state
            if prev is not None:
                _log(f"  pixmap: {prev} -> {state}")

    def _clamp_to_screen(self, pos: QPoint) -> QPoint:
        geo = QApplication.primaryScreen().availableGeometry()
        x = max(geo.x(), min(pos.x(), geo.x() + geo.width()  - self.PET_SIZE))
        y = max(geo.y(), min(pos.y(), geo.y() + geo.height() - self.PET_SIZE))
        return QPoint(x, y)

    @staticmethod
    def _smoothstep(t: float) -> float:
        return t * t * (3.0 - 2.0 * t)

    # ── state machine ──────────────────────────────────────
    def set_state(self, state: str) -> None:
        if state != "walking" and state not in self.images:
            _log(f"  set_state: 无效状态 '{state}' — 忽略")
            return
        old = self.state
        self.state = state
        self._target_pos = None
        self._sync_pixmap()
        if old != state:
            _log(f"[{self._log_elapsed()}ms] 状态变更: {old} -> {state}")

    def _sync_pixmap(self) -> None:
        if self.state == "walking" and self._walk_frames:
            # Cycle through 4 walk frames based on elapsed time
            frames = self._walk_frames.get(self._walk_direction, [])
            if frames:
                elapsed = self._walk_clock.elapsed()
                frame_idx = (elapsed // self.WALK_FRAME_MS) % len(frames)
                self.label.setPixmap(frames[frame_idx])
                self._last_pixmap_state = None  # always refresh during walking
        else:
            wanted = self.state if self.state in self.images else "idle"
            self._set_pixmap(wanted)

    # ── walking ────────────────────────────────────────────
    def _start_walk(self, target: QPoint) -> None:
        self._walk_id += 1
        wid = self._walk_id
        self._target_pos = target
        self._walk_start_pos = self.pos()
        self._walk_clock.start()
        self._walk_duration_ms = random.randint(
            self.WALK_DURATION_MIN, self.WALK_DURATION_MAX
        )
        self._walk_tick_count = 0

        dist = ((target.x() - self.x()) ** 2 + (target.y() - self.y()) ** 2) ** 0.5

        if self.state not in ("clicked", "happy", "sleep"):
            self.state = "walking"
            # Determine walk direction from target vs current position
            if target.x() > self.x():
                self._walk_direction = "right"
            elif target.x() < self.x():
                self._walk_direction = "left"
            # if equal, keep previous direction

        _log(
            f"[{self._log_elapsed()}ms] 走动 #{wid} 开始: "
            f"({self._walk_start_pos.x()},{self._walk_start_pos.y()}) "
            f"-> ({target.x()},{target.y()})  "
            f"距离={dist:.0f}px  时长={self._walk_duration_ms}ms  "
            f"当前state={self.state}"
        )

    def walk_to(self, x: int, y: int) -> None:
        self._start_walk(self._clamp_to_screen(QPoint(x, y)))

    # ── behavior ───────────────────────────────────────────
    def _decide_behavior(self) -> None:
        _log(
            f"[{self._log_elapsed()}ms] 行为决策触发  "
            f"当前state={self.state}  pos=({self.x()},{self.y()})"
        )
        if self.state in ("sleep", "clicked", "happy", "walking"):
            _log(f"  -> 跳过 (state={self.state} 不可打断)")
            return

        r = random.random()
        if r < 0.50:
            _log(f"  -> 选择: 远距离走动 (r={r:.3f})")
            self._walk_to_random()
        elif r < 0.85:
            _log(f"  -> 选择: 近距离溜达 (r={r:.3f})")
            self._walk_nearby()
        else:
            _log(f"  -> 选择: 发呆不动 (r={r:.3f})")

    def _walk_to_random(self) -> None:
        geo = QApplication.primaryScreen().availableGeometry()
        target = QPoint(
            random.randint(geo.x(), geo.x() + max(0, geo.width()  - self.PET_SIZE)),
            random.randint(geo.y(), geo.y() + max(0, geo.height() - self.PET_SIZE)),
        )
        self._start_walk(target)

    def _walk_nearby(self) -> None:
        target = self._clamp_to_screen(QPoint(
            self.x() + random.randint(-100, 100),
            self.y() + random.randint(-80,  80),
        ))
        self._start_walk(target)

    # ── animation tick ─────────────────────────────────────
    def _animation_tick(self) -> None:
        if self.state == "walking" and self._target_pos is not None:
            self._walk_tick_count += 1
            elapsed = self._walk_clock.elapsed()
            t = min(1.0, elapsed / max(1, self._walk_duration_ms))
            eased = self._smoothstep(t)

            cur = QPoint(
                int(self._walk_start_pos.x()
                    + (self._target_pos.x() - self._walk_start_pos.x()) * eased),
                int(self._walk_start_pos.y()
                    + (self._target_pos.y() - self._walk_start_pos.y()) * eased),
            )
            self.move(cur)

            if (
                self._walk_tick_count == 1
                or self._walk_tick_count % 15 == 0
                or t >= 1.0
            ):
                actual = self.pos()
                delta = (actual.x() - cur.x(), actual.y() - cur.y())
                _log(
                    f"  walk_tick #{self._walk_tick_count:4d}  "
                    f"elapsed={elapsed:5d}ms  t={t:.3f}  "
                    f"期望=({cur.x()},{cur.y()})  实际=({actual.x()},{actual.y()})"
                    + (f"  偏移=({delta[0]},{delta[1]})" if delta != (0, 0) else "")
                )

            if t >= 1.0:
                self.move(self._target_pos)
                actual = self.pos()
                self._target_pos = None
                self.state = "idle"
                _log(
                    f"[{self._log_elapsed()}ms] 走动 #{self._walk_id} 完成 "
                    f"-> ({actual.x()},{actual.y()})  "
                    f"共 {self._walk_tick_count} 帧  "
                    f"state -> idle"
                )

        self._sync_pixmap()

    # ── mouse events ───────────────────────────────────────
    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.MouseButton.LeftButton:
            self.drag_position = (
                event.globalPosition().toPoint()
                - self.frameGeometry().topLeft()
            )
            _log(f"[{self._log_elapsed()}ms] 鼠标按下 (左键) — 开始拖拽")
            self.set_state("clicked")
        elif event.button() == Qt.MouseButton.RightButton:
            _log(f"[{self._log_elapsed()}ms] 鼠标按下 (右键) — 弹出菜单")
            self._show_context_menu(event.globalPosition().toPoint())

    def mouseMoveEvent(self, event) -> None:
        if self.drag_position is not None and event.buttons() & Qt.MouseButton.LeftButton:
            raw = event.globalPosition().toPoint() - self.drag_position
            clamped = self._clamp_to_screen(raw)
            self.move(clamped)

    def mouseReleaseEvent(self, event) -> None:
        if event.button() == Qt.MouseButton.LeftButton:
            self.drag_position = None
            _log(f"[{self._log_elapsed()}ms] 鼠标释放 — 拖拽结束 -> happy")
            self.set_state("happy")
            QTimer.singleShot(self.HAPPY_TIMEOUT, lambda: self.set_state("idle"))

    # ── context menu ───────────────────────────────────────
    def _show_context_menu(self, pos: QPoint) -> None:
        menu = QMenu(self)

        a_happy = QAction("逗我开心", self)
        a_happy.triggered.connect(lambda: self.set_state("happy"))
        menu.addAction(a_happy)

        a_sleep = QAction("睡觉", self)
        a_sleep.triggered.connect(lambda: self.set_state("sleep"))
        menu.addAction(a_sleep)

        a_idle = QAction("叫醒", self)
        a_idle.triggered.connect(lambda: self.set_state("idle"))
        menu.addAction(a_idle)

        menu.addSeparator()

        a_quit = QAction("退出", self)
        a_quit.triggered.connect(self.close)
        menu.addAction(a_quit)

        menu.exec(pos)


if __name__ == "__main__":
    _log_init()
    app = QApplication(sys.argv)
    pet = DesktopPet()
    sys.exit(app.exec())
