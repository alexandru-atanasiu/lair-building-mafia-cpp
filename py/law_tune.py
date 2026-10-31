"""Runs the auto players fast to measure the law's balance. See "Balance" in CLAUDE.md.

    python py/law_tune.py [--days 14] [--modes 1,2,3,4,5] [--turbo 20] [--repeat 1]

One game per mode at a time, with its own pipe, on the opt build. Each prints when it hit each
raid and how it ended. `turbo` runs extra simulation steps per frame: the rules are the same.
"""

import argparse
import os
import re
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = {1: "greedy missions", 2: "careful missions", 3: "smuggling to booze", 4: "smuggling+missions", 5: "contraband"}


def send(pipe, command):
    pipe.write((command + "\n").encode("utf-8"))
    reply = b""
    while not reply.endswith(b"\n"):
        chunk = pipe.read(65536)
        if not chunk:
            raise EOFError
        reply += chunk
    return reply.decode("utf-8", "replace").strip()


def field(reply, name):
    m = re.search(rf"\b{name}=([-0-9.e+]+)", reply)
    return float(m.group(1)) if m else 0.0


def run(mode, days, turbo, tag, results):
    pipe_name = f"capo_tune_{os.getpid()}_{tag}"
    exe = os.path.join(ROOT, "__generated", "bin", "opt", "game.exe")
    proc = subprocess.Popen([exe, "-windowed", "-pipe", pipe_name, "-nodevui", "-size", "640x360", "-novsync"],
                            cwd=os.path.join(ROOT, "__generated", "data"))
    deadline = time.time() + 60
    pipe = None
    while pipe is None:
        try:
            pipe = open(r"\\.\pipe" + "\\" + pipe_name, "r+b", buffering=0)
        except OSError:
            if time.time() > deadline:
                results[tag] = "no pipe"
                return
            time.sleep(0.2)
    for command in ["speed 3", f"city auto {mode}", "law autodismiss 1", f"law turbo {turbo}", "wander 1"]:
        send(pipe, command)

    samples = []
    bad = []
    next_sample = 1
    while True:
        time.sleep(0.5)
        state = send(pipe, "state")
        law = send(pipe, "law")
        check = send(pipe, "law check")
        if check.startswith("err") and "away" not in check:
            bad.append(check)
        day = int(field(state, "day"))
        hour = int(field(state, "hour"))
        if day >= next_sample:
            samples.append((day, int(field(state, "money")), int(field(state, "heat")), int(field(law, "evidence"))))
            next_sample = day + 1
        if "screen=busted" in law or day > days:
            break
    log = send(pipe, "law log")
    totals = send(pipe, "city totals")
    send(pipe, "quit")
    proc.wait(timeout=20)

    raids = [x.strip() for x in log.split("|") if x.strip().startswith(("raid", "escape", "busted"))]
    reports = sum(1 for x in log.split("|") if x.strip().startswith("report"))
    lines = [f"{NAMES.get(mode, mode)} [{tag}] ended day {day} {hour}h; reports {reports}; {totals}"]
    lines.append("  day:money/heat/evidence " + " ".join(f"{d}:{m}/{h}/{e}" for d, m, h, e in samples))
    lines += ["  " + x for x in raids]
    if bad:
        lines.append(f"  CHECK FAILED x{len(bad)}: {bad[0]}")
    results[tag] = "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--days", type=int, default=14)
    parser.add_argument("--modes", default="1,2,3,4,5")
    parser.add_argument("--turbo", type=int, default=20)
    parser.add_argument("--repeat", type=int, default=1)
    args = parser.parse_args()
    results = {}
    threads = []
    for mode in [int(m) for m in args.modes.split(",")]:
        for r in range(args.repeat):
            tag = f"m{mode}r{r}"
            t = threading.Thread(target=run, args=(mode, args.days, args.turbo, tag, results))
            t.start()
            threads.append(t)
    for t in threads:
        t.join()
    for tag in sorted(results):
        print(results[tag])
    return 0


if __name__ == "__main__":
    sys.exit(main())
