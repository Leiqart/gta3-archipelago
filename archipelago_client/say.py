#!/usr/bin/env python3
"""Pousse un toast directement dans le jeu en cours (sans serveur AP).

Usage:
    python say.py mon texte ici

Ecrit III.Archipelago.bridge.json avec un seq incremente ; le plugin lit ce
fichier toutes les ~20 frames et affiche le texte a l'ecran (une fois le
moteur SCM vivant, cf. le fix du crash 0x00438FB1). Utile pour verifier le
chemin d'affichage in-game independamment du chat serveur.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME_DIR = os.path.normpath(os.path.join(HERE, "..", "Grand Theft Auto 3 -moded"))
BRIDGE_FILE = os.path.join(GAME_DIR, "III.Archipelago.bridge.json")


def main():
    msg = " ".join(sys.argv[1:]).strip()
    if not msg:
        print("usage: python say.py <texte>")
        return 1
    try:
        with open(BRIDGE_FILE, "r", encoding="utf-8") as f:
            seq = int(json.load(f).get("seq", 0))
    except Exception:
        seq = 0
    seq += 1
    with open(BRIDGE_FILE, "w", encoding="utf-8") as f:
        json.dump({"seq": seq, "toast": msg[:80]}, f)
    print(f"toast envoye (seq={seq}): {msg[:80]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
