# Tests

## Protocol parser smoke test

```bash
g++ -std=c++11 -IleyrgaApp/src test/test_protocol.cpp leyrgaApp/src/LeyboldProtocol.cpp -o test_protocol
./test_protocol
```

## Simulator

```bash
python3 test/leyrga_sim.py --host 127.0.0.1 --port 1024
```

The simulator implements the protocol subset used by the v1 IOC and generates synthetic residual-gas peaks.
