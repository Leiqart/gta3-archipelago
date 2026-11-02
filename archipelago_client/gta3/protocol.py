"""Local protocol between GTA3 Client and the ASI: versioned JSON Lines over loopback TCP, without AP imports."""
from __future__ import annotations

import json
import re
from typing import Any, Dict


LOCAL_HOST = "127.0.0.1"
LOCAL_PORT = 2727
PROTOCOL_VERSION = 1
GAME_ID = "gta3"
MAX_MESSAGE_BYTES = 16 * 1024

_LOCATION_RE = re.compile(r"^[a-z0-9_]+$")


class ProtocolError(ValueError):
    """Raised when a local peer sends an invalid protocol message."""


def encode_message(message_type: str, **fields: Any) -> bytes:
    """Encode one client-to-game JSON Lines message."""
    if not isinstance(message_type, str) or not message_type:
        raise ValueError("message_type must be a non-empty string")
    payload = {"type": message_type, **fields}
    encoded = (json.dumps(payload, ensure_ascii=True, separators=(",", ":")) + "\n").encode("ascii")
    if len(encoded) > MAX_MESSAGE_BYTES:
        raise ValueError("protocol message is too large")
    return encoded


def decode_game_message(raw_line: bytes) -> Dict[str, Any]:
    """Decode and validate one game-to-client JSON Lines message."""
    if not raw_line:
        raise ProtocolError("empty message")
    if len(raw_line) > MAX_MESSAGE_BYTES:
        raise ProtocolError("message is too large")
    try:
        payload = json.loads(raw_line.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProtocolError("message is not valid JSON") from exc
    if not isinstance(payload, dict):
        raise ProtocolError("message must be a JSON object")

    message_type = payload.get("type")
    if message_type == "hello":
        if payload.get("game") != GAME_ID:
            raise ProtocolError("wrong game identifier")
        if payload.get("version") != PROTOCOL_VERSION:
            raise ProtocolError("unsupported protocol version")
    elif message_type == "check":
        location = payload.get("location")
        if not isinstance(location, str) or not _LOCATION_RE.fullmatch(location):
            raise ProtocolError("invalid location identifier")
    elif message_type not in ("goal", "ping"):
        raise ProtocolError("unknown message type")

    return payload
