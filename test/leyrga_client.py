#!/usr/bin/env python3
"""Command-line smoke client for a Leybold view-series RGA or the simulator."""
import argparse
import socket


def checksum(payload: bytes) -> str:
    return f"{sum(payload) & 0xFFFF:04X}"


def command(address: int, cmd: str, parameter: str = "") -> bytes:
    return f"#{address:02d}{cmd}{parameter}\r\n".encode("ascii")


def parse(frame: bytes):
    data = frame.rstrip(b"\r\n")
    if not data.startswith(b"#") or len(data) < 8:
        raise RuntimeError(f"invalid frame: {frame!r}")
    body, supplied = data[:-4], data[-4:].decode("ascii")
    expected = checksum(body)
    if supplied.upper() != expected:
        raise RuntimeError(f"checksum mismatch: supplied={supplied} expected={expected}")
    if data[3] in (0x06, 0x15):
        return {"type": "ACK" if data[3] == 0x06 else "NAK", "code": data[4:6].decode()}
    return {"type": "DATA", "command": data[3:5].decode(), "parameter": body[5:].decode()}


def exchange(sock, address, cmd, parameter=""):
    tx = command(address, cmd, parameter)
    print("TX", tx.rstrip().decode("ascii"))
    sock.sendall(tx)
    rx = b""
    while b"\r\n" not in rx:
        chunk = sock.recv(16384)
        if not chunk:
            raise RuntimeError("connection closed")
        rx += chunk
    result = parse(rx)
    print("RX", result)
    if result["type"] == "NAK":
        raise RuntimeError(f"NAK {result['code']}")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("--port", type=int, default=1024)
    parser.add_argument("--address", type=int, default=1)
    args = parser.parse_args()

    with socket.create_connection((args.host, args.port), timeout=2.0) as sock:
        exchange(sock, args.address, "CN")
        exchange(sock, args.address, "CM", "2")
        exchange(sock, args.address, "CS")
        exchange(sock, args.address, "DS")
        exchange(sock, args.address, "ES")
        exchange(sock, args.address, "VR")
        exchange(sock, args.address, "G1")
        for channel, mass in enumerate([2, 18, 28, 32, 40, 44]):
            exchange(sock, args.address, "MC", f"{channel:02d}")
            exchange(sock, args.address, "MM", f"{mass:03d}")
        exchange(sock, args.address, "ST")
        exchange(sock, args.address, "DD")
        exchange(sock, args.address, "SP")


if __name__ == "__main__":
    main()
