"""Plays a test run in the real game with nothing to start by hand: generates a room from apworld/players, hosts it on port 38300, opens GTA3 Client and a dev build of GTA III,
waits for the run to end, then stops everything and sums up the game's log.

Usage: python apworld/tools/autotest.py [--shop | --money]
  (default)  the dev build's autopilot plays the test missions with the settings of GTA3Archipelago-dev.ini, until it has passed all it can or is stuck.
  --shop     the shop check (shop_test=1, nobody plays): the player walks onto the Uzi of Portland's Ammu-Nation without its license, the room sends it, then again with it.
  --money    two sessions, nobody plays: the room sends $10000 in the first; the second, with a new client and a new game, must start with that money from the client's save file,
             skip that item, and add the next $10000.
The game is the one mod/build.cmd installs into (GTA3_GAME, else dist/live-game-test/game), with a dev build (mod/build.cmd dev); Archipelago is ARCHIPELAGO_DIR, else C:/ProgramData/Archipelago.
GTA3 Client must be closed first: the mod reaches the client on port 2727. The run takes the screen; do not touch the game until it ends.
The dev settings file is changed for the run (run_in_background=1, or the check's settings) and put back afterwards, whatever happens."""
import argparse
import os
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ARCHIPELAGO = Path(os.environ.get("ARCHIPELAGO_DIR") or r"C:\ProgramData\Archipelago")
GAME = Path(os.environ.get("GTA3_GAME") or REPO / "dist" / "live-game-test" / "game")
OUT = REPO / "apworld" / "output" / "autotest"
INI = GAME / "scripts" / "GTA3Archipelago-dev.ini"
LOG = GAME / "scripts" / "GTA3Archipelago.log"
# Not host.cmd's 38281, so a room it hosts can stay open.
PORT = "38300"
# The longest a run may take: a whole run of the autopilot, and each check, which ends once its rounds are done.
LONGEST = {"run": 3 * 3600, "shop": 10 * 60, "money": 15 * 60}
# The checks' settings: nobody plays, and the game goes on without the focus.
CHECK_SETTINGS = "play=0\nrun_in_background=1\nmute=1\nquit_on_stuck=0\nquit_when_done=0\n"
SHOP_RESULTS = ("the Uzi was bought", "no Uzi bought")


# The slot name of the GTA III player in apworld/players, which the room is generated from.
def slot_name() -> str:
    for yaml in sorted((REPO / "apworld" / "players").glob("*.yaml")):
        fields = dict(line.split(":", 1) for line in yaml.read_text(encoding="utf-8").splitlines() if ":" in line and not line.startswith((" ", "#")))
        if fields.get("game", "").strip() == "Grand Theft Auto III":
            return fields["name"].strip()
    raise SystemExit("No GTA III player in apworld/players")


# True while another program holds this port of this PC; found by trying to take it, so that a GTA3 Client listening there sees no connection.
def port_in_use(port: int) -> bool:
    with socket.socket() as sock:
        try:
            sock.bind(("127.0.0.1", port))
        except OSError:
            return True
    return False


def log_lines() -> list[str]:
    try:
        return LOG.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []


# Waits until a line of the game's log from this index on contains one of the texts; returns its index, or -1 once the deadline has passed or the game has closed.
def wait_for(texts: tuple[str, ...], after: int, deadline: float, gta: subprocess.Popen) -> int:
    while True:
        lines = log_lines()
        for index in range(after, len(lines)):
            if any(text in lines[index] for text in texts):
                return index
        if time.time() >= deadline or gta.poll() is not None:
            return -1
        time.sleep(1)


# Generates a room from apworld/players into a fresh folder; returns its .zip.
def generate_room() -> Path:
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)
    generated = subprocess.run([str(ARCHIPELAGO / "ArchipelagoGenerate.exe"), "--player_files_path", str(REPO / "apworld" / "players"), "--outputpath", str(OUT), "--spoiler", "2"],
                               stdin=subprocess.DEVNULL, capture_output=True, text=True)
    rooms = sorted(OUT.glob("AP_*.zip"))
    if generated.returncode != 0 or not rooms:
        raise SystemExit("Generation failed:\n" + generated.stdout[-2000:] + generated.stderr[-2000:])
    return rooms[-1]


class Run:
    """The programs of a run: the room's server, then a GTA3 Client and the game for each session; stop() ends them all."""

    def __init__(self, slot: str) -> None:
        self.slot = slot
        self.processes: list[subprocess.Popen] = []
        self.server: subprocess.Popen | None = None

    def host(self, room: Path) -> None:
        self.server = subprocess.Popen([str(ARCHIPELAGO / "ArchipelagoServer.exe"), "--host", "127.0.0.1", "--port", PORT, str(room)],
                                       stdin=subprocess.PIPE, stdout=open(OUT / "server.log", "w"), stderr=subprocess.STDOUT, cwd=OUT)
        self.processes.append(self.server)
        time.sleep(5)

    # Opens a GTA3 Client on the room, then the game, which the dev build starts a new game in once the client is connected; returns the game.
    def open_session(self) -> tuple[subprocess.Popen, subprocess.Popen]:
        client = subprocess.Popen([str(ARCHIPELAGO / "ArchipelagoLauncher.exe"), "GTA3 Client", "--", "--connect", f"localhost:{PORT}", "--name", self.slot])
        self.processes.append(client)
        time.sleep(8)
        gta = subprocess.Popen([str(GAME / "gta3.exe")], cwd=GAME)
        self.processes.append(gta)
        return client, gta

    # Types a command in the room's console, like "/send <slot> $10000".
    def console(self, command: str) -> None:
        self.server.stdin.write((command + "\n").encode("utf-8"))
        self.server.stdin.flush()

    @staticmethod
    def end(*processes: subprocess.Popen) -> None:
        for process in processes:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True)

    def stop(self) -> None:
        self.end(*reversed(self.processes))


# The shop check's two rounds: once the first is done, the room's console sends the Uzi License, and the second shows what the license changed.
def shop_check(run: Run, deadline: float) -> int | None:
    _, gta = run.open_session()
    first = wait_for(tuple("Shop test 1: " + result for result in SHOP_RESULTS), 0, deadline, gta)
    if first < 0:
        print("The first round never ended")
        return gta.poll()
    run.console(f"/send {run.slot} Uzi License")
    if wait_for(tuple("Shop test 2: " + result for result in SHOP_RESULTS), first + 1, deadline, gta) < 0:
        print("The second round never ended")
    return gta.poll()


# The money check: a first session gets $10000 from the room's console and reports it; a second one, with a new client and a new game, must start with it, then gets $10000 more.
def money_check(run: Run, deadline: float) -> int | None:
    sends = f"/send {run.slot} $10000"
    for session in (1, 2):
        client, gta = run.open_session()
        started = wait_for(("Money: the game starts with",), 0, deadline, gta)
        if started < 0:
            print(f"Session {session}: the game never took the room's money")
            return gta.poll()
        # The items the client sends after the money come within a few frames.
        time.sleep(5)
        run.console(sends)
        if wait_for(("Money: $",), started + 1, deadline, gta) < 0:
            print(f"Session {session}: the money the room sent never reached the client")
        # Time for the client to keep it in the room before both close.
        time.sleep(3)
        run.end(gta, client)
        shutil.copy2(LOG, OUT / f"GTA3Archipelago-session{session}.log")
    return 0


# What the game's log says of the run: why it ended, the missions passed, the items received, and the checks' lines.
def sum_up(lines: list[str], exit_code: int | None, mode: str) -> None:
    if mode == "run":
        if exit_code == 0:
            # A window closed by hand also exits with 0: the log tells it from quit_when_done.
            why = "the window was closed by hand" if any("the window was asked to close" in line for line in lines) else "every test mission it could play is passed"
        else:
            why = {None: "stopped by this script", 3: "the autopilot was stuck", 4: "no game frame for 60 s"}.get(exit_code, "the game crashed or was stopped")
        print(f"The game's exit code: {exit_code} ({why})")
    passed = [line.split("(")[-1].rstrip(")") for line in lines if line.startswith("Mission passed:")]
    print(f"{len(passed)} missions passed:", ", ".join(passed) or "none")
    for line in lines:
        if line.startswith(("Shop test", "Shops:", "Money:", "Items:", "Received item")) or any(word in line for word in ("stuck", "crashed", "all test missions")):
            print("  ", line)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    choice = parser.add_mutually_exclusive_group()
    choice.add_argument("--shop", action="store_true", help="check that a shop sells a weapon only with its license")
    choice.add_argument("--money", action="store_true", help="check that the room keeps the money from one session to the next")
    arguments = parser.parse_args()
    mode = "shop" if arguments.shop else "money" if arguments.money else "run"
    if port_in_use(2727):
        raise SystemExit("Port 2727 is in use: close GTA3 Client first")
    if not (GAME / "gta3.exe").exists() or not INI.exists():
        raise SystemExit(f"No game with a dev build's settings in {GAME}: run mod/build.cmd dev")
    run = Run(slot_name())
    room = generate_room()
    print("Room:", room)
    backup = OUT / "GTA3Archipelago-dev.ini.yours"
    shutil.copy2(INI, backup)
    started = time.time()
    exit_code = None
    try:
        if mode == "run":
            settings = [line for line in INI.read_text(encoding="ascii").splitlines() if line and not line.startswith("run_in_background=")]
            INI.write_text("\n".join(settings + ["run_in_background=1"]) + "\n", encoding="ascii")
        else:
            INI.write_text(CHECK_SETTINGS + ("shop_test=1\n" if mode == "shop" else ""), encoding="ascii")
        run.host(room)
        deadline = time.time() + LONGEST[mode]
        if mode == "shop":
            exit_code = shop_check(run, deadline)
        elif mode == "money":
            exit_code = money_check(run, deadline)
        else:
            _, gta = run.open_session()
            try:
                exit_code = gta.wait(timeout=LONGEST[mode])
            except subprocess.TimeoutExpired:
                print(f"The game still ran after {LONGEST[mode] // 3600} hours")
    finally:
        run.stop()
        shutil.copy2(backup, INI)
        if LOG.exists():
            shutil.copy2(LOG, OUT / "GTA3Archipelago.log")
    print(f"Ran {int(time.time() - started) // 60} min")
    if mode == "money":
        for session in (1, 2):
            path = OUT / f"GTA3Archipelago-session{session}.log"
            print(f"--- session {session}")
            sum_up(path.read_text(encoding="utf-8", errors="replace").splitlines() if path.exists() else [], exit_code, mode)
        # The client's save file of this room: its name starts with the room's seed, the number in the room's file name.
        seed = room.stem.removeprefix("AP_")
        for save in sorted((ARCHIPELAGO / "gta3_saves").glob(f"{seed}_*.json")):
            print(f"--- the client's save file {save}:", save.read_text(encoding="utf-8"))
    else:
        sum_up(log_lines(), exit_code, mode)
    print("Dev settings put back; the run's files are in", OUT)


if __name__ == "__main__":
    sys.exit(main())
