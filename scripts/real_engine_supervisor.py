#!/usr/bin/env python3
import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path


def kill_group(pid: int, sig: int) -> None:
    try:
        os.killpg(pid, sig)
    except ProcessLookupError:
        pass
    except PermissionError:
        try:
            os.kill(pid, sig)
        except ProcessLookupError:
            pass


def process_alive(proc: subprocess.Popen | None) -> bool:
    return proc is not None and proc.poll() is None


def terminate_process(proc: subprocess.Popen | None, grace: float = 1.5) -> int:
    if proc is None:
        return 0
    if process_alive(proc):
        kill_group(proc.pid, signal.SIGTERM)
        deadline = time.monotonic() + grace
        while process_alive(proc) and time.monotonic() < deadline:
            time.sleep(0.05)
    if process_alive(proc):
        kill_group(proc.pid, signal.SIGKILL)
    try:
        return proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        try:
            os.kill(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        try:
            return proc.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            return 137


def read_probe(path: Path) -> list[str]:
    try:
        return path.read_text(errors="replace").splitlines()
    except FileNotFoundError:
        return []


def lifecycle_complete(lines: list[str], bsp: bool) -> bool:
    line_set = set(lines)
    required = {
        "CreateInterface_OK ServerGameDLL006",
        "DLLInit_OK",
        "GetPlayerLimits min=1 max=64 default=32",
        "PostInit_OK",
    }
    if not required.issubset(line_set):
        return False
    if not bsp:
        return True
    if "GameInit_OK" not in line_set:
        return False
    if "CreateNetworkStringTables_OK" not in line_set:
        return False
    if "ServerActivate_OK" not in line_set:
        return False
    if not any(line.startswith("LevelInit_OK ") for line in lines):
        return False
    put_count = sum(line.startswith("ClientPutInServer_OK ") for line in lines)
    active_count = sum(line.startswith("ClientActive_OK ") for line in lines)
    return put_count >= 2 and active_count >= 2


def append_text(path: Path, text: str) -> None:
    with path.open("a", encoding="utf-8", errors="replace") as out:
        out.write(text)
        if text and not text.endswith("\n"):
            out.write("\n")


def bounded_command(command: list[str], out_path: Path, timeout: float, title: str) -> None:
    append_text(out_path, f"\n===== {title} =====\n")
    try:
        with out_path.open("ab") as out:
            proc = subprocess.Popen(
                command,
                stdout=out,
                stderr=subprocess.STDOUT,
                stdin=subprocess.DEVNULL,
                start_new_session=True,
                close_fds=True,
            )
            try:
                proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                kill_group(proc.pid, signal.SIGKILL)
                try:
                    proc.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    pass
                append_text(out_path, f"<{title} timed out after {timeout:.1f}s>")
    except FileNotFoundError as exc:
        append_text(out_path, f"<{title} unavailable: {exc}>")
    except Exception as exc:
        append_text(out_path, f"<{title} failed: {exc}>")


def proc_snapshot(pid: int, out_path: Path) -> None:
    append_text(out_path, f"\n===== PROC SNAPSHOT pid={pid} =====\n")
    for rel in ("status", "wchan", "syscall", "maps"):
        path = Path(f"/proc/{pid}/{rel}")
        append_text(out_path, f"\n--- {path} ---\n")
        try:
            text = path.read_text(errors="replace")
            if rel == "maps":
                lines = text.splitlines()
                interesting = [
                    line for line in lines
                    if any(token in line.lower() for token in (
                        "engine", "server_i486", "soundemitter", "scenefilecache",
                        "filesystem", "materialsystem", "vphysics", "dedicated",
                        "tier0", "vstdlib", "steam"
                    ))
                ]
                append_text(out_path, "\n".join(interesting[-300:]))
            else:
                append_text(out_path, text)
        except Exception as exc:
            append_text(out_path, f"<unavailable: {exc}>")

    task_root = Path(f"/proc/{pid}/task")
    try:
        tids = sorted(task_root.iterdir(), key=lambda p: int(p.name))
    except Exception as exc:
        append_text(out_path, f"\n--- tasks unavailable: {exc} ---\n")
        tids = []
    for task in tids:
        append_text(out_path, f"\n--- task {task.name} ---\n")
        for rel in ("status", "wchan", "syscall", "stack"):
            path = task / rel
            try:
                text = path.read_text(errors="replace")
                if rel == "status":
                    keep = []
                    for line in text.splitlines():
                        if line.startswith(("Name:", "State:", "Pid:", "Tgid:", "PPid:", "TracerPid:")):
                            keep.append(line)
                    text = "\n".join(keep) + "\n"
                append_text(out_path, f"[{rel}]\n{text}")
            except Exception as exc:
                append_text(out_path, f"[{rel}] <unavailable: {exc}>")

    bounded_command(
        ["ps", "-eLo", "pid,ppid,pgid,sid,lwp,stat,wchan:28,comm,args"],
        out_path,
        2.0,
        "PROCESS TREE",
    )


def gdb_snapshot(pid: int, out_path: Path) -> None:
    bounded_command(
        [
            "gdb", "-q", "-batch",
            "-ex", "set pagination off",
            "-ex", "set confirm off",
            "-ex", "set print thread-events off",
            "-ex", "thread apply all bt",
            "-ex", "detach",
            "-p", str(pid),
        ],
        out_path,
        7.0,
        f"GDB BACKTRACE pid={pid}",
    )
    # A killed/failed debugger must never leave srcds stopped.
    try:
        os.kill(pid, signal.SIGCONT)
    except ProcessLookupError:
        pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    parser.add_argument("--log", required=True)
    parser.add_argument("--diagnostic", required=True)
    parser.add_argument("--pid-file", required=True)
    parser.add_argument("--cwd", required=True)
    parser.add_argument("--timeout", type=int, required=True)
    parser.add_argument("--bsp", action="store_true")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command and args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        parser.error("missing command after --")
    if args.timeout <= 0:
        parser.error("timeout must be positive")

    probe = Path(args.probe)
    log = Path(args.log)
    diagnostic = Path(args.diagnostic)
    pid_file = Path(args.pid_file)
    probe.parent.mkdir(parents=True, exist_ok=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    probe.write_text("")
    diagnostic.write_text("")
    pid_file.write_text("")

    proc: subprocess.Popen | None = None
    stopping = False

    def on_signal(signum, _frame):
        nonlocal stopping
        if stopping:
            if proc is not None:
                kill_group(proc.pid, signal.SIGKILL)
            raise KeyboardInterrupt
        stopping = True
        print(f"REAL_ENGINE_SUPERVISOR_SIGNAL signal={signum} pid={proc.pid if proc else -1}", flush=True)
        terminate_process(proc, grace=0.25)
        raise KeyboardInterrupt

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGHUP, on_signal)

    print(
        f"REAL_ENGINE_SUPERVISOR_PREPARE mode={'BSP' if args.bsp else 'NO_BSP'} "
        f"deadline={args.timeout}s command={args.command[0]}",
        flush=True,
    )

    env = os.environ.copy()
    try:
        with log.open("wb") as log_fp:
            # Do not use preexec_fn here.  Python documents it as unsafe around
            # fork/exec and it can deadlock before Popen returns.  start_new_session
            # performs the needed setsid in the safe subprocess path.
            proc = subprocess.Popen(
                args.command,
                cwd=args.cwd,
                env=env,
                stdin=subprocess.DEVNULL,
                stdout=log_fp,
                stderr=subprocess.STDOUT,
                start_new_session=True,
                close_fds=True,
            )
            pid_file.write_text(f"{proc.pid}\n")

            print(
                f"REAL_ENGINE_START pid={proc.pid} pgid={proc.pid} "
                f"mode={'BSP' if args.bsp else 'NO_BSP'} deadline={args.timeout}s",
                flush=True,
            )

            last_count = 0
            last_event = "<none>"
            deadline = time.monotonic() + args.timeout
            while True:
                lines = read_probe(probe)
                if len(lines) > last_count:
                    for event in lines[last_count:]:
                        if event:
                            last_event = event
                            print(f"REAL_ENGINE_EVENT {event}", flush=True)
                    last_count = len(lines)

                if lifecycle_complete(lines, args.bsp):
                    rc = terminate_process(proc)
                    print(
                        f"REAL_ENGINE_LIFECYCLE_COMPLETE last_event={last_event} stop_rc={rc}",
                        flush=True,
                    )
                    return 0

                rc = proc.poll()
                if rc is not None:
                    print(f"REAL_ENGINE_PROCESS_EXIT rc={rc} last_event={last_event}", flush=True)
                    return rc if rc != 0 else 3

                if time.monotonic() >= deadline:
                    print(
                        f"REAL_ENGINE_WATCHDOG_TIMEOUT last_event={last_event} deadline={args.timeout}s",
                        flush=True,
                    )
                    proc_snapshot(proc.pid, diagnostic)
                    gdb_snapshot(proc.pid, diagnostic)
                    terminate_process(proc)
                    return 124

                time.sleep(0.10)
    except KeyboardInterrupt:
        terminate_process(proc, grace=0.25)
        return 130
    finally:
        if process_alive(proc):
            terminate_process(proc, grace=0.25)
        pid_file.write_text("")


if __name__ == "__main__":
    sys.exit(main())
