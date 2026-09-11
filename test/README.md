# V1 tests

## 1. Protocol unit test

This test validates frame generation/parsing, checksum handling, and the DD parser for Scan, Trend and Analog modes.

```bash
g++ -std=c++11 -I../leyrgaApp/src test_protocol.cpp \
    ../leyrgaApp/src/LeyboldProtocol.cpp -o test_protocol
./test_protocol
```

## 2. Start the LEYSPEC simulator

```bash
python3 leyrga_sim.py --host 127.0.0.1 --port 1024
```

The simulator implements the V1 command subset and generates synthetic peaks at m/z 2, 18, 28, 32, 40 and 44.

## 3. Run the standalone TCP client

From the repository root:

```bash
python3 test/leyrga_client.py 127.0.0.1
```

The client performs the basic connection and control sequence and requests a Trend-mode `DD` response.

## 4. IOC test

Build the EPICS application with:

```bash
make
```

Start the simulator first, then launch the generated `leyrgaIoc` using `iocBoot/iocleyrga/st.cmd`.

The default startup configuration uses:

```text
RGA IP       = 127.0.0.1
RGA TCP port = 1024
RGA address  = 1
PV prefix    = RGA01:
```

For a project deployment, change only the IOC macros or copy the startup configuration into the target IOC repository. The database itself remains generic.
