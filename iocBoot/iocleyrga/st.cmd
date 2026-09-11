#!../../bin/${EPICS_HOST_ARCH}/leyrgaIoc

< envPaths

# Generic deployment macros. Override these from the environment or copy this
# startup file into the target project's IOC repository.
epicsEnvSet("RGA_PREFIX", "RGA01:")
epicsEnvSet("RGA_IP", "127.0.0.1")
epicsEnvSet("RGA_ADDRESS", "1")
epicsEnvSet("RGA_PORT", "RGA_TCP")
epicsEnvSet("RGA_TREND_MASSES", "2,18,28,32,40,44")

dbLoadDatabase("$(TOP)/dbd/leyrgaIoc.dbd")
leyrgaIoc_registerRecordDeviceDriver(pdbbase)

drvAsynIPPortConfigure("$(RGA_PORT)", "$(RGA_IP):1024 TCP", 0, 0, 0)
asynOctetSetInputEos("$(RGA_PORT)", 0, "\\r\\n")
asynOctetSetOutputEos("$(RGA_PORT)", 0, "")
asynSetOption("$(RGA_PORT)", 0, "disconnectOnReadTimeout", "Y")

leyrgaConfigure("RGA", "$(RGA_PORT)", $(RGA_ADDRESS), "$(RGA_TREND_MASSES)")

dbLoadRecords("$(TOP)/db/leyrga.db", "P=$(RGA_PREFIX),PORT=RGA,ADDR=0")

iocInit

# Useful diagnostics from the IOC shell:
# asynReport RGA_TCP 1
# dbgrep RGA01:
