"""What the test tools of apworld/tools share: where the repository and Archipelago are, how a room is generated, how a log is read, and how a check is reported."""
import os
import shutil
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ARCHIPELAGO = Path(os.environ.get("ARCHIPELAGO_DIR") or r"C:\ProgramData\Archipelago")


def generate(folder: Path, players: Path, seed: int | None = None) -> tuple[int, Path | None, str]:
    """Generates a room from the YAML files of players into a fresh folder, with its spoiler; returns the generator's exit code, the room's .zip (None if there is none)
    and what the generator wrote. The apworld the generator uses is the one installed in Archipelago's custom_worlds."""
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    command = [str(ARCHIPELAGO / "ArchipelagoGenerate.exe"), "--player_files_path", str(players), "--outputpath", str(folder), "--spoiler", "2"]
    if seed is not None:
        command += ["--seed", str(seed)]
    run = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, text=True, encoding="utf-8", errors="replace")
    rooms = sorted(folder.glob("AP_*.zip"))
    return run.returncode if rooms else (run.returncode or 1), rooms[-1] if rooms else None, run.stdout + run.stderr


def read_lines(path: Path) -> list[str]:
    """The lines of a text file, such as the game's log; none if it cannot be read."""
    try:
        return path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []


class Checks:
    """Prints each check as "ok" or "FAIL" with what went wrong, and counts the failures."""

    def __init__(self) -> None:
        self.failures: list[str] = []

    def check(self, what: str, ok: bool, detail: object = "") -> None:
        print("ok  " if ok else "FAIL", what, "" if ok or detail == "" else f"-> {detail}")
        if not ok:
            self.failures.append(what)

    def sum_up(self) -> int:
        """Prints the count of failures; returns the exit code for the script, 1 if any check failed."""
        print(f"--- {len(self.failures)} failure(s)" + (": " + ", ".join(self.failures) if self.failures else ""))
        return 1 if self.failures else 0
