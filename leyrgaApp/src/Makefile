TOP=../..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += leyrga
leyrga_SRCS += leyrga.cpp
leyrga_SRCS += LeyboldProtocol.cpp
leyrga_LIBS += asyn
leyrga_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC += leyrgaIoc
leyrgaIoc_DBD += leyrgaIoc.dbd
leyrgaIoc_SRCS += leyrgaMain.cpp
leyrgaIoc_LIBS += leyrga
leyrgaIoc_LIBS += asyn
leyrgaIoc_LIBS += $(EPICS_BASE_IOC_LIBS)

DBD += leyrgaIoc.dbd

include $(TOP)/configure/RULES
