"""Drives a running dbg/opt game through the dev driver's named pipe. See "Dev driver" in CLAUDE.md.

    python py/dev_client.py --launch dbg "view lair" "wait 2" "screenshot shots/lair.png" state quit

Each argument is one command; it runs after the previous one finished, and its reply ("ok ..." or
"err ...") is printed. The exit code is 1 if a command failed. Relative screenshot paths are made
absolute here, since the game's working directory is the data folder.

  --launch <dbg|opt>  start the game first (windowed, with -pipe), then wait for it to answer
  --args="<args>"     extra game arguments for --launch (e.g. --args="-view lair -nodevui -size 1280x720")
  --pipe <name>       pipe name (default capo_dev)
"""

import argparse
import os
import shlex
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def connect(pipe_path, timeout):
    deadline = time.time() + timeout
    while True:
        try:
            return open(pipe_path, "r+b", buffering=0)
        except OSError:
            if time.time() > deadline:
                raise
            time.sleep(0.1)


def send(pipe, command):
    pipe.write((command + "\n").encode("utf-8"))
    reply = b""
    while not reply.endswith(b"\n"):
        chunk = pipe.read(4096)
        if not chunk:
            raise EOFError("the game closed the pipe")
        reply += chunk
    return reply.decode("utf-8").strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pipe", default="capo_dev")
    parser.add_argument("--launch", choices=["dbg", "opt"])
    parser.add_argument("--args", default="")
    parser.add_argument("commands", nargs="*")
    args = parser.parse_args()

    if args.launch:
        exe = os.path.join(ROOT, "__generated", "bin", args.launch, "game.exe")
        subprocess.Popen([exe, "-windowed", "-pipe", args.pipe] + shlex.split(args.args),
                         cwd=os.path.join(ROOT, "__generated", "data"))

    failed = False
    pipe = connect(r"\\.\pipe" + "\\" + args.pipe, 60.0 if args.launch else 5.0)
    try:
        for command in args.commands:
            word, _, rest = command.partition(" ")
            if word == "screenshot":
                os.makedirs(os.path.dirname(os.path.abspath(rest.strip())), exist_ok=True)
                command = "screenshot " + os.path.abspath(rest.strip())
            reply = send(pipe, command)
            print(f"{command} -> {reply}")
            failed = failed or reply.startswith("err")
    except EOFError:
        if not args.commands[-1:] == ["quit"]:
            raise
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
