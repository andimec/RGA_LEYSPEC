#!../../bin/linux-x86_64/leyrga

< envPaths

cd ${TOP}

# Simulator default. Replace 127.0.0.1:1024 with the real LEYSPEC IP for MUVACAS.
drvAsynIPPortConfigure("RGAIP", "127.0.0.1:1024", "", 0, 0, 0)
leyrgaConfigure("RGA01", "RGAIP", 1)

dbLoadRecords("db/leyrga.db", "P=MUVACAS:RGA01:")

iocInit

# caget MUVACAS:RGA01:CONNECTED
# caget MUVACAS:RGA01:STATUS
# caget MUVACAS:RGA01:PRESSURE:TOTAL
# caput MUVACAS:RGA01:MEASUREMENT:START 1
