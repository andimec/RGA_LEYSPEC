#!/usr/bin/env python3
"""Small Leybold view-series protocol simulator for IOC/Phoebus testing."""
import argparse, math, socket, threading, time

PEAKS = {2: 1.2e-8, 18: 3.5e-7, 28: 2.2e-6, 32: 8.0e-8, 40: 2.5e-8, 44: 1.5e-7}

def checksum(payload: bytes) -> str:
    return f"{sum(payload) & 0xFFFF:04X}"

def response(address, command, parameter=""):
    body = f"#{address:02d}{command}{parameter}".encode("ascii")
    return body + checksum(body).encode("ascii") + b"\r\n"

def ack(address, error=0):
    body = bytes([ord('#'), ord('0') + address // 10, ord('0') + address % 10, 0x06]) + f"{error:02d}".encode()
    return body + checksum(body).encode() + b"\r\n"

def nak(address, error=3):
    body = bytes([ord('#'), ord('0') + address // 10, ord('0') + address % 10, 0x15]) + f"{error:02d}".encode()
    return body + checksum(body).encode() + b"\r\n"

class Simulator:
    def __init__(self, host, port):
        self.host, self.port = host, port
        self.measurement, self.mode = False, 0
        self.first_mass, self.last_mass, self.speed = 1, 50, 3
        self.filament, self.detector, self.address = 0, 0, 1
        self.start = time.monotonic()

    def handle(self, conn):
        buf = b""
        with conn:
            while True:
                data = conn.recv(4096)
                if not data: return
                buf += data
                while b"\r\n" in buf:
                    frame, buf = buf.split(b"\r\n", 1)
                    if not frame: continue
                    try:
                        text = frame.decode("ascii", errors="replace")
                        address, cmd, param = int(text[1:3]), text[3:5], text[5:]
                    except Exception: continue
                    conn.sendall(self.command(address, cmd, param))

    def command(self, address, cmd, param):
        if cmd in ("CN", "DC", "CM"): return ack(address)
        if cmd == "CS": return response(address, "CS", "02111")
        if cmd == "DS": return response(address, "DS", f"E0F{self.filament}S{1 if self.detector == 0 else 0}R1D{self.detector}A{0 if self.measurement else 1}")
        if cmd == "ES": return response(address, "ES", "Er00000")
        if cmd == "VR": return response(address, "VR", "VER101")
        if cmd == "F0": self.filament = 0; return ack(address)
        if cmd == "F1": self.filament = 1; return ack(address)
        if cmd == "SF": self.detector = 1; return ack(address)
        if cmd == "SS": self.detector = 0; return ack(address)
        if cmd == "G0": self.mode = 0; return ack(address)
        if cmd == "G1": self.mode = 1; return ack(address)
        if cmd == "G2": self.mode = 2; return ack(address)
        if cmd == "FM": self.first_mass = int(param); return ack(address)
        if cmd == "LM": self.last_mass = int(param); return ack(address)
        if cmd == "SD": self.speed = int(param); return ack(address)
        if cmd == "ST": self.measurement = True; return ack(address)
        if cmd == "SP": self.measurement = False; return ack(address)
        if cmd == "DD": return self.dd(address)
        return nak(address, 1)

    def signal(self, mass):
        baseline = 1.0e-14
        peak = PEAKS.get(mass, 0.0)
        modulation = 1.0 + 0.05 * math.sin(time.monotonic() - self.start)
        return baseline + peak * modulation

    def dd(self, address):
        if self.mode == 1:
            masses = [2, 18, 28, 32, 40, 44] + [0] * 14
            values = [self.signal(m) if m else 1.0e-14 for m in masses]
        elif self.mode == 2:
            values = [self.signal(self.first_mass + i) for i in range(min(20, self.last_mass - self.first_mass + 1))]
        else:
            values = [self.signal(m) for m in range(self.first_mass, self.last_mass + 1)]
        fields = [f"{v:.3e}" for v in values]
        fields += ["3.000e-6", "0.00", "0.00", "0", "0"]
        return response(address, "DD", ",".join(fields))

    def run(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((self.host, self.port)); server.listen(5)
            print(f"Leybold simulator listening on {self.host}:{self.port}")
            while True:
                conn, _ = server.accept()
                threading.Thread(target=self.handle, args=(conn,), daemon=True).start()

if __name__ == "__main__":
    p = argparse.ArgumentParser(); p.add_argument("--host", default="127.0.0.1"); p.add_argument("--port", type=int, default=1024)
    a = p.parse_args(); Simulator(a.host, a.port).run()
