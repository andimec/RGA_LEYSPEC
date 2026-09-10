TOP=../..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += leyrga

leyrga_SRCS += leyrga.cpp
leyrga_SRCS += LeyboldProtocol.cpp

leyrga_LIBS += asyn
leyrga_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
