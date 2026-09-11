TOP=../..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += leyrga
leyrga_SRCS += leyrga.cpp
leyrga_SRCS += LeyboldProtocol.cpp
leyrga_LIBS += asyn
leyrga_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC += leyrgaIoc
DBD += leyrgaIoc.dbd
leyrgaIoc_DBD += base.dbd
leyrgaIoc_DBD += asyn.dbd
leyrgaIoc_DBD += drvAsynIPPort.dbd
leyrgaIoc_DBD += leyrga.dbd
leyrgaIoc_SRCS += leyrgaMain.cpp
leyrgaIoc_SRCS += leyrgaIoc_registerRecordDeviceDriver.cpp
leyrgaIoc_LIBS += leyrga
leyrgaIoc_LIBS += asyn
leyrgaIoc_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
