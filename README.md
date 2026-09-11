# RGA_LEYSPEC

EPICS/asyn driver for the Leybold LEYSPEC view 200S Residual Gas Analyzer (RGA).

The repository is intended to be reusable across accelerator and vacuum control projects. The EPICS record prefix is supplied at deployment time, so the same database can be used in MUVACAS, LIPAc, IFMIF-DONES, or another EPICS installation without changing the driver source.

## V1 architecture

```text
LEYSPEC view 200S
       |
       | Ethernet TCP/IP :1024
       v
asyn drvAsynIPPort
       |
       v
LEYSPEC EPICS/asyn driver
       |
       +--> generic EPICS PVs
       |
       +--> spectrum / mass-axis waveforms
       |
       +--> status / alarms / communication diagnostics
       |
       v
Phoebus or another EPICS client
```

Phoebus display files are deliberately not part of V1. They will be added in the next development step.

## V1 functionality

### 1. Protocol layer

The driver implements the documented Leybold TCP protocol:

- CR/LF terminated commands.
- RGA address 01 to 16.
- ACK/NAK handling.
- Error codes 01, 02 and 03.
- Response checksum validation.
- `CN` and `CM2` initialization.
- `CS`, `DS`, `ES` and `VR` status queries.

### 2. Measurement configuration

Supported commands include:

- `G0`, `G1`, `G2` for Scan, Trend and Analog modes.
- `FM`, `LM` for scan/analog mass range.
- `MC`, `MM` for Trend mass channels.
- `SD` for measurement speed.
- `F0`, `F1`, `FA`, `FB` for filament operation.
- `SF`, `SS` for detector selection.
- `ST`, `SP` for measurement start/stop.
- `DD` for measurement data acquisition.
- `UC2` to keep the driver readback in mbar.

### 3. DD data parser

The driver handles all three documented DD response families:

- Scan mode: variable number of mass channels.
- Trend mode: 20 channels.
- Analog mode: 20 DAC/value pairs.

For Scan and Trend, the driver publishes total pressure and the two analog inputs. For Analog mode, the mass axis is derived from the DAC value using the documented 20 points per amu representation.

### 4. Communication robustness

V1 includes:

- Transaction serialization so polling and EPICS writes cannot interleave protocol frames.
- TCP timeout handling.
- Communication error counter.
- Automatic reconnect and reinitialization after communication loss.
- Response address validation.
- Protocol checksum validation.
- Separation of instrument error and communication diagnostics at the PV level.

### 5. Generic EPICS PV namespace

The database uses the macro `P` for the project/device prefix. Example deployments:

```text
MUVACAS:RGA01:
LIPAC:RGA01:
IFMIF:DONES:RGA01:
```

The driver itself has no project-specific PV names.

Main PV groups:

```text
$(P)CONNECTED
$(P)ERROR
$(P)ERROR:CODE
$(P)ERROR:RAW
$(P)STATUS
$(P)VERSION
$(P)MODEL
$(P)MASS:RANGE

$(P)FILAMENT
$(P)FILAMENT:CMD
$(P)FILAMENT:SELECT
$(P)DETECTOR
$(P)DETECTOR:CMD

$(P)MODE
$(P)MODE:CMD
$(P)MASS:FIRST
$(P)MASS:FIRST:SET
$(P)MASS:LAST
$(P)MASS:LAST:SET
$(P)SPEED:SET

$(P)MEASUREMENT:START
$(P)MEASUREMENT:STOP
$(P)MEASURING

$(P)PRESSURE:TOTAL
$(P)PRESSURE:SETPOINT:STATUS
$(P)ANALOG:INPUT1
$(P)ANALOG:INPUT2

$(P)SPECTRUM
$(P)MASS:AXIS

$(P)COMM:COUNT
$(P)COMM:ERROR
```

## Building

Set the EPICS Base and asyn paths in `configure/RELEASE`, or copy `configure/RELEASE.local.example` to `configure/RELEASE.local` and edit it for the deployment.

```bash
make
```

The build produces the `leyrga` library and the standalone `leyrgaIoc` soft IOC.

## Standalone simulator

Start the simulated LEYSPEC on TCP port 1024:

```bash
python3 test/leyrga_sim.py --host 127.0.0.1 --port 1024
```

Then run the protocol client:

```bash
python3 test/leyrga_client.py 127.0.0.1
```

The simulator supports the V1 command subset, including Scan, Trend and Analog DD responses.

## Protocol unit test

```bash
g++ -std=c++11 -IleyrgaApp/src test/test_protocol.cpp \
    leyrgaApp/src/LeyboldProtocol.cpp -o test_protocol
./test_protocol
```

## IOC test configuration

The generic startup script is in `iocBoot/iocleyrga/st.cmd`. The default configuration points to the local simulator.

For a real RGA, change the IP address and ensure that the LEYSPEC is configured for remote operation before enabling external control. The startup script also configures the TCP end-of-message handling and the automatic reconnect behavior.

Example project deployment:

```text
Project IOC
  |
  +-- LEYSPEC driver source from this repository
  |
  +-- P=PROJECT:RGA01:
  |
  +-- PORT=RGA_TCP
  |
  +-- RGA_TCP -> <RGA-IP>:1024 TCP
```

## Hardware support status

The Leybold manual explicitly lists the LEYSPEC view 200S among the models compatible with the communication protocol. The protocol documentation specifies TCP port 1024 and socket-based command/response communication.

The manual also describes Windows as the supported PC environment. This project implements the documented protocol on Linux through EPICS/asyn. It therefore does not claim official Leybold Linux support. Production deployment should be validated against the actual instrument and, where required, confirmed with Leybold.

## Development status

V1 covers steps 1 to 6 of the development plan:

1. Protocol test client.
2. Protocol library.
3. EPICS/asyn driver.
4. EPICS database.
5. Generic IOC configuration.
6. Simulator and end-to-end IOC test infrastructure.

Phoebus OPI development is the next step and is intentionally outside V1.
