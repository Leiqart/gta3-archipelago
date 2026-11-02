"""Build versioned alpha release files from explicit inputs, without game assets."""
import argparse
import ast
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_VERSION = "0.3.0-alpha.1"
WORLD_FILES = ("__init__.py", "data.py", "Options.py", "Client.py", "protocol.py", "archipelago.json")


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, encoding="utf-8").strip()


def build_world():
    source = ROOT / "archipelago_client/gta3"
    metadata = json.loads((source / "archipelago.json").read_text(encoding="utf-8"))
    if metadata["authors"] != ["Leiqart"]:
        raise ValueError("Project author metadata must name Leiqart only")
    for name in WORLD_FILES:
        if name.endswith(".py"):
            ast.parse((source / name).read_text(encoding="utf-8"), filename=name)
    output = ROOT / "dist/gta3.apworld"
    output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(output, "w", ZIP_DEFLATED) as archive:
        for name in WORLD_FILES:
            archive.write(source / name, "gta3/" + name)
    with ZipFile(output) as archive:
        if archive.testzip() is not None or set(archive.namelist()) != {"gta3/" + n for n in WORLD_FILES}:
            raise ValueError("Invalid apworld archive")
    return output, metadata


def verify_asi(path):
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError("ASI is not a Windows binary")
    pe = struct.unpack_from("<I", data, 60)[0]
    if data[pe:pe + 4] != b"PE\0\0" or struct.unpack_from("<H", data, pe + 4)[0] != 0x14C:
        raise ValueError("ASI must target x86")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default=DEFAULT_VERSION)
    args = parser.parse_args()
    if not re.fullmatch(r"\d+\.\d+\.\d+-alpha\.\d+", args.version):
        parser.error("Use an alpha version, for example 0.3.0-alpha.1")
    subprocess.run([sys.executable, str(ROOT / "gta_3_mod/tools/generate_runtime_data.py"), "--check"], check=True)
    asi = ROOT / "gta_3_mod/output/Release/GTA3Archipelago.asi"
    verify_asi(asi)
    world, metadata = build_world()
    if args.version.split("-", 1)[0] != metadata["world_version"]:
        raise ValueError("Release and apworld versions disagree")
    output = ROOT / "dist/releases" / args.version
    output.mkdir(parents=True, exist_ok=True)
    provenance = {
        "version": args.version, "author": "Leiqart",
        "built_at_utc": datetime.now(timezone.utc).isoformat(),
        "source_commit": git("rev-parse", "HEAD"),
        "uncommitted_changes": bool(git("status", "--porcelain")),
        "validation": "Unit and integration tests do not establish complete live-game compatibility.",
    }
    inputs = {
        "README.md": ROOT / "README.md",
        "gta3_game_files/scripts/GTA3Archipelago.asi": asi,
        "archipelago/gta3.apworld": world,
        "archipelago/GTA3.yaml": ROOT / "archipelago_client/players/GTA3.yaml",
        "licenses/MinHook.txt": ROOT / "gta_3_mod/third_party/minhook/LICENSE.txt",
        "licenses/nlohmann-json.txt": ROOT / "gta_3_mod/third_party/nlohmann/LICENSE.MIT",
    }
    archive_path = output / f"GTA3-Archipelago-{args.version}.zip"
    with ZipFile(archive_path, "w", ZIP_DEFLATED) as archive:
        for destination, source in inputs.items():
            archive.write(source, destination)
        archive.writestr("BUILD.json", json.dumps(provenance, indent=2) + "\n")
    with ZipFile(archive_path) as archive:
        if archive.testzip() is not None or set(archive.namelist()) != set(inputs) | {"BUILD.json"}:
            raise ValueError("Release archive verification failed")
        for destination, source in inputs.items():
            if archive.read(destination) != source.read_bytes():
                raise ValueError(f"Release input mismatch: {destination}")
    shutil.copy2(world, output / "gta3.apworld")
    shutil.copy2(inputs["archipelago/GTA3.yaml"], output / "GTA3.yaml")
    artifacts = (archive_path, output / "gta3.apworld", output / "GTA3.yaml")
    shutil.copy2(archive_path, ROOT / "dist/GTA3-Archipelago-development.zip")
    for path in artifacts:
        print(path)


if __name__ == "__main__":
    main()
