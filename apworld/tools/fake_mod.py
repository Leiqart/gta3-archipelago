import socket
import threading
import time


# Prints every line the GTA3 Client sends until it closes the connection.
def read_lines(sock: socket.socket) -> None:
    buffer = b""
    while True:
        try:
            data = sock.recv(4096)
        except OSError:
            return
        if not data:
            return
        buffer += data
        while b"\n" in buffer:
            line, buffer = buffer.split(b"\n", 1)
            print("Received:", line.decode("utf-8"), flush=True)


class FakeMod:
    """Pretends to be the GTA III mod: stays connected to the GTA3 Client like the mod does, and sends the lines you type."""

    def __init__(self) -> None:
        self.sock: socket.socket | None = None

    # Runs in the background: connects to the GTA3 Client, prints its lines, and retries every 2 seconds while it is missing.
    def run(self) -> None:
        waiting_printed = False
        while True:
            try:
                with socket.create_connection(("127.0.0.1", 2727)) as sock:
                    self.sock = sock
                    waiting_printed = False
                    print("Connected to the GTA3 Client", flush=True)
                    read_lines(sock)
                self.sock = None
                print("The GTA3 Client closed the connection", flush=True)
            except OSError:
                self.sock = None
                if not waiting_printed:
                    print("GTA3 Client not running, retrying every 2 seconds", flush=True)
                waiting_printed = True
            time.sleep(2)

    # Sends one typed line to the GTA3 Client, like the mod's "check 1".
    def send(self, line: str) -> None:
        sock = self.sock
        if sock is None:
            print("Not sent: not connected to the GTA3 Client", flush=True)
            return
        try:
            sock.sendall((line + "\n").encode("utf-8"))
        except OSError:
            print("Not sent: the connection was lost", flush=True)


# Starts the connection in the background, then sends every line typed in this window; Ctrl+C stops it.
def main() -> None:
    fake = FakeMod()
    threading.Thread(target=fake.run, daemon=True).start()
    print("Type a line and press Enter to send it, for example: check 1", flush=True)
    while True:
        line = input().strip()
        if line:
            fake.send(line)


if __name__ == "__main__":
    try:
        main()
    except (KeyboardInterrupt, EOFError):
        print("Fake mod stopped", flush=True)
