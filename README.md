# RGA_LEYSPEC

EPICS/asyn driver for the Leybold LEYSPEC view 200S Residual Gas Analyzer (RGA), initially developed for deployment and testing on MuVacAS.

## Scope

This repository implements the Leybold Ethernet communication protocol described in the LEYSPEC instruction manual, using the documented TCP interface. The first target is the LEYSPEC view 200S and the MUVACAS EPICS/Phoebus environment.

Architecture:

```text
LEYSPEC view 200S -- Ethernet/TCP :1024 --> EPICS IOC/asyn driver --> EPICS PVs --> Phoebus
```

The driver is intentionally split into a protocol layer and an EPICS/asyn layer so the protocol can be tested without EPICS or hardware.

## Current v1 scope

- TCP transport through EPICS asyn.
- Leybold command framing and checksum validation.
- Connection and extended control mode (`CN`, `CM2`).
- RGA condition/status (`CS`, `DS`, `ES`, `VR`).
- Filament control (`F0`, `F1`).
- Detector selection (`SF`, `SS`).
- Scan/trend/analog mode selection (`G0`, `G1`, `G2`).
- Scan mass range (`FM`, `LM`).
- Measurement speed (`SD`).
- Start/stop measurement (`ST`, `SP`).
- Measurement data acquisition (`DD`).
- Total pressure extraction from `DD` responses.
- EPICS spectrum and mass-axis arrays.
- Standalone Python simulator for MUVACAS IOC/Phoebus development.

## Hardware protocol notes

The Leybold manual explicitly lists LEYSPEC view 200S as compatible with the protocol. It documents TCP port 1024, a CR/LF terminated command format, and checksum-bearing responses. The manual also describes remote mode and requires `CM2` after power-up or PC disconnect.

The same manual describes its supported PC environment as Windows. This repository therefore treats Linux support as an implementation/integration target, not as a claim of official Leybold Linux support.

## Build

This is an EPICS Base + asyn support module. Set the paths in `configure/RELEASE` and build with:

```bash
make
```

## Simulator

The simulator listens on TCP port 1024 by default:

```bash
python3 test/leyrga_sim.py --host 0.0.0.0 --port 1024
```

It implements the subset required by the v1 IOC and generates synthetic peaks at common residual-gas masses.

## IOC test

Configure `iocBoot/iocleyrga/st.cmd` for the simulator or the real RGA IP address, then run the IOC. Do not connect the driver to a real instrument until the RGA has been placed in remote mode and the operational limits/interlocks have been checked.
