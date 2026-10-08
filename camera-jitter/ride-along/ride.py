r"""ride.py -- the BeG0nE Camera Jitter rider: sits in the background, notices when a game is running in VR, and files
the mod's jitter log the moment the VR session ends. Idle, it costs one process look every few seconds.

    python ride.py install        make it start with Windows for this user (a shortcut in the Startup folder; no admin), start it now
    python ride.py uninstall      remove that, and stop it
    python ride.py start | stop | status
    python ride.py run            the loop itself, in the foreground (what the task runs)
    python ride.py check <file.jl> [--game <project>]     file one log by hand
    python ride.py label <run> jittery|smooth [note]      say how that session FELT; the knowledge base learns from it

HOW IT KNOWS THE GAME IS IN VR, three ways, because one is never enough (claude-memory PREFERENCES.md, "input
automation" -- the same reasoning):
  1. the mod says so: jitterlog.h / jitterlog.lua only write a .jl file between jl_vr_on() and jl_vr_off(), so a
     growing .jl in %LOCALAPPDATA%\BeG0nE\jitter\ IS a VR session, and names the game and the runtime;
  2. SteamVR is up: vrserver.exe / vrcompositor.exe / vrmonitor.exe are running;
  3. a windowed process has openxr_loader.dll or openvr_api.dll loaded (the OpenXR simulator runs inside the game's
     own process, so on the dev PC this is the only process-level sign).
Only 1 produces data; 2 and 3 go in the status line so a session can see "VR is up but no mod is logging" and know
the mod is not yet wired to jitterlog.h.

WHAT IT FILES: when a .jl stops growing (the session ended, or the game died) it runs jitter_log_check.py on it and
puts summary.json, report.md and chart.png in camera-jitter/data/<game>/<stamp>_<runtime>/, adds a line to
camera-jitter/data/INDEX.csv, and moves the .jl to %LOCALAPPDATA%\BeG0nE\jitter\done\. It never commits; the session does.
"""
import csv
import ctypes
import ctypes.wintypes as W
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

DATA = os.path.join(os.path.dirname(HERE), "data")
INDEX = os.path.join(DATA, "INDEX.csv")
INDEX_FIELDS = ["run", "game", "runtime", "date", "time", "seconds", "pictures_per_s", "pose_reads_per_s",
                "camera_writes_per_s", "turning_s", "jitter_s", "quality", "verdict"]
LOCAL = os.path.join(os.path.expandvars("%LOCALAPPDATA%"), "BeG0nE")
LOG_DIR = os.path.join(LOCAL, "jitter")
DONE_DIR = os.path.join(LOG_DIR, "done")
STATE = os.path.join(LOCAL, "ride-state.json")
PID_FILE = os.path.join(LOCAL, "ride.pid")
RIDE_LOG = os.path.join(LOCAL, "ride.log")
POLL_S = 5                      # how often the rider looks
SETTLE_S = 10                   # a .jl that has not grown for this long is a finished session
PROCESS_LOOK_EVERY_S = 15       # how often the (dearer) process check runs
STARTUP_CMD = os.path.join(os.path.expandvars("%APPDATA%"), r"Microsoft\Windows\Start Menu\Programs\Startup",
                           "BeG0nE-jitter-rider.cmd")      # per-user Startup folder: needs no admin, unlike a logon task
STEAMVR_EXES = ("vrserver.exe", "vrcompositor.exe", "vrmonitor.exe")
VR_DLLS = ("openxr_loader.dll", "openvr_api.dll")
MIN_LOG_BYTES = 200             # a .jl smaller than this never got past its header
DETACHED_PROCESS = 0x00000008   # Windows: the rider gets its own console-less session so closing the terminal does not end it


def say(msg):
    line = f"{time.strftime('%Y-%m-%d %H:%M:%S')} {msg}"
    print(line, flush=True)
    try:
        with open(RIDE_LOG, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except OSError:
        pass


# ----- VR signals -----
def windowed_pids():
    u = ctypes.windll.user32
    pids = set()

    @ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    def cb(h, _):
        if u.IsWindowVisible(h):
            pid = W.DWORD()
            u.GetWindowThreadProcessId(h, ctypes.byref(pid))
            pids.add(pid.value)
        return True
    u.EnumWindows(cb, 0)
    pids.discard(os.getpid())
    return pids


def vr_processes():
    """Returns (steamvr_up, [names of windowed processes with a VR dll loaded])."""
    try:
        import psutil
    except ImportError:
        return False, []
    steam = False
    loaded = []
    wp = windowed_pids()
    for p in psutil.process_iter(["pid", "name"]):
        name = (p.info["name"] or "").lower()
        if name in STEAMVR_EXES:
            steam = True
        if p.info["pid"] in wp:
            try:
                for m in p.memory_maps():
                    if os.path.basename(m.path).lower() in VR_DLLS:
                        loaded.append(p.info["name"]); break
            except (psutil.AccessDenied, psutil.NoSuchProcess, OSError):
                pass
    return steam, loaded


def logs_in_progress():
    out = []
    if not os.path.isdir(LOG_DIR):
        return out
    for f in os.listdir(LOG_DIR):
        p = os.path.join(LOG_DIR, f)
        if f.endswith(".jl") and os.path.isfile(p):
            out.append((p, os.path.getsize(p), os.path.getmtime(p)))
    return out


def log_head(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            first = f.readline()
        return dict(kv.split("=", 1) for kv in first.split()[1:] if "=" in kv)
    except OSError:
        return {}


def log_ended(path):
    try:
        with open(path, "rb") as f:
            f.seek(max(0, os.path.getsize(path) - 16))
            return b"#end" in f.read()
    except OSError:
        return False


# ----- filing -----
def file_log(path, game=None):
    import jitter_log_check
    head = log_head(path)
    game = game or head.get("game", "unknown")
    runtime = head.get("runtime", "unknown")
    stamp = time.strftime("%Y-%m-%d_%H-%M-%S", time.localtime(os.path.getmtime(path)))
    run = f"{stamp}_{runtime}"
    out = os.path.join(DATA, game, run)
    s = jitter_log_check.check(path, out)
    rows = []
    if os.path.exists(INDEX):
        with open(INDEX, newline="", encoding="utf-8") as f:
            rows = [r for r in csv.DictReader(f) if r.get("run") != run]
    rows.append({"run": run, "game": game, "runtime": runtime, "date": stamp[:10], "time": stamp[11:],
                 "seconds": s.get("seconds", 0), "pictures_per_s": s.get("pictures_per_s", 0),
                 "pose_reads_per_s": s.get("pose_reads_per_s", 0), "camera_writes_per_s": s.get("camera_writes_per_s", 0),
                 "turning_s": s.get("seconds_turning", 0), "jitter_s": s.get("seconds_jitter", 0),
                 "quality": s["quality"], "verdict": s["verdict"]})
    os.makedirs(DATA, exist_ok=True)
    with open(INDEX, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=INDEX_FIELDS)
        w.writeheader(); w.writerows(rows)
    say(f"FILED {game}/{run}: {s['verdict']}")
    try:
        import knowledge
        fid, status = knowledge.ingest(out)
        knowledge.report()
        say(f"KNOWLEDGE: pattern {fid} {status}")
    except Exception as e:  # the base must never stop a filing
        say(f"knowledge base not updated: {e}")
    return out


def write_state(**kw):
    os.makedirs(LOCAL, exist_ok=True)
    kw["at"] = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(STATE, "w", encoding="utf-8") as f:
        json.dump(kw, f, indent=1)


# ----- the loop -----
def run():
    os.makedirs(DONE_DIR, exist_ok=True)
    with open(PID_FILE, "w") as f:
        f.write(str(os.getpid()))
    say("rider up")
    seen = {}            # path -> (size, first seen)
    last_proc_look = 0
    steam, loaded = False, []
    announced = set()
    while True:
        now = time.time()
        if now - last_proc_look >= PROCESS_LOOK_EVERY_S:
            steam, loaded = vr_processes()
            last_proc_look = now
        live = []
        for p, size, mtime in logs_in_progress():
            head = log_head(p)
            game = head.get("game", "?")
            if p not in announced and size >= MIN_LOG_BYTES:
                say(f"VR SESSION: {game} ({head.get('runtime', '?')}) is logging"); announced.add(p)
            if size < MIN_LOG_BYTES and now - mtime > SETTLE_S:
                os.remove(p); continue          # never got going
            if log_ended(p) or now - mtime > SETTLE_S:
                try:
                    file_log(p, game)
                except Exception as e:  # one bad log must not stop the rider
                    say(f"could not file {os.path.basename(p)}: {e}")
                shutil.move(p, os.path.join(DONE_DIR, os.path.basename(p)))
                announced.discard(p)
            else:
                live.append(game)
        write_state(vr=bool(live or steam or loaded), logging=live, steamvr=steam, vr_dll_in=loaded)
        time.sleep(POLL_S)


# ----- start / stop / install -----
def running_pid():
    try:
        with open(PID_FILE) as f:
            pid = int(f.read().strip())
    except (OSError, ValueError):
        return None
    try:
        import psutil
        p = psutil.Process(pid)
        return pid if "ride.py" in " ".join(p.cmdline()) else None
    except Exception:
        return None


def start():
    if running_pid():
        print("already running"); return
    os.makedirs(LOCAL, exist_ok=True)
    subprocess.Popen([sys.executable, os.path.abspath(__file__), "run"], stdout=subprocess.DEVNULL,
                     stderr=subprocess.DEVNULL, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0) | DETACHED_PROCESS)
    time.sleep(1)
    print("rider started" if running_pid() else "rider did not start; see " + RIDE_LOG)


def stop():
    pid = running_pid()
    if not pid:
        print("not running"); return
    subprocess.run(["taskkill", "/PID", str(pid), "/F"], capture_output=True)
    try:
        os.remove(PID_FILE)
    except OSError:
        pass
    print("rider stopped")


def status():
    pid = running_pid()
    print("rider:", f"running (pid {pid})" if pid else "not running")
    try:
        with open(STATE, encoding="utf-8") as f:
            st = json.load(f)
        print("VR now:", "YES" if st.get("vr") else "no", "| mods logging:", st.get("logging") or "none",
              "| SteamVR:", "up" if st.get("steamvr") else "down", "| VR dll in:", st.get("vr_dll_in") or "none",
              "| at", st.get("at"))
    except (OSError, ValueError):
        print("no state yet")
    print("starts with Windows:", "yes" if os.path.exists(STARTUP_CMD) else "no")


def install():
    pyw = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")
    if not os.path.exists(pyw):
        pyw = sys.executable
    os.makedirs(os.path.dirname(STARTUP_CMD), exist_ok=True)
    with open(STARTUP_CMD, "w", encoding="utf-8") as f:
        f.write(f'@echo off\r\nstart "" "{pyw}" "{os.path.abspath(__file__)}" run\r\n')
    print("starts with Windows: yes (" + STARTUP_CMD + ")")
    start()


def uninstall():
    try:
        os.remove(STARTUP_CMD)
    except OSError:
        pass
    stop()
    print("startup entry removed")


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    cmd = a[0]
    if cmd == "run": run()
    elif cmd == "start": start()
    elif cmd == "stop": stop()
    elif cmd == "status": status()
    elif cmd == "install": install()
    elif cmd == "uninstall": uninstall()
    elif cmd == "label" and len(a) >= 3:
        import knowledge
        knowledge.label(a[1], a[2], " ".join(a[3:]))
    elif cmd == "check" and len(a) >= 2:
        game = a[a.index("--game") + 1] if "--game" in a else None
        print(file_log(a[1], game))
    else:
        sys.exit(__doc__)
