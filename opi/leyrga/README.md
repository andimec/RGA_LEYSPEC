# LEYSPEC Phoebus OPI

## V0.1 diagnostic display

`leyrga_diagnostics_v0.1.bob` is a generic Phoebus Display Builder screen for the LEYSPEC EPICS/asyn driver.

The display uses the deployment macro `P` for the EPICS prefix. The same display can therefore be reused with prefixes such as:

```text
MUVACAS:RGA01:
IFMIF:DONES:RGA01:
LIPAC:RGA01:
TEST:RGA01:
```

Launch the display with the Phoebus `resource` mechanism and provide the `P` macro for the target IOC. Phoebus supports simple `$(macro)` expansion in display files and allows macro values to be supplied when launching a display.

The V0.1 screen is intentionally diagnostic only. It displays:

- Communication connection and counters.
- Instrument model, version, mass range, mode and speed.
- Measurement state and total pressure.
- Pressure setpoint status.
- Analog inputs.
- Filament and detector state.
- Instrument and driver error information.
- Mass spectrum using `$(P)SPECTRUM` and `$(P)MASS:AXIS`.

No advanced configuration controls are included in V0.1. Operator controls and a more complete operational interface will be developed separately after the driver and diagnostic screen have been validated with the simulator and the real LEYSPEC hardware.
