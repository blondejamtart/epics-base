#*************************************************************************
# Copyright (c) 2002 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=..

include $(TOP)/configure/CONFIG

# Bootstrap resolution: tools not installed yet
TOOLS = $(TOP)/src/tools

CONFIGS += $(subst ../,,$(wildcard ../CONFIG*))
CONFIGS += $(subst ../,,$(wildcard ../os/CONFIG*))

CONFIGS += $(subst ../,,$(wildcard ../RELEASE*))
CONFIGS += $(subst ../,,$(wildcard ../RULES*))

CFG += CONFIG_LIBCOM_MODULE
CFG += CONFIG_LIBCOM_VERSION

CFG += CONFIG_CA_MODULE
CFG += CONFIG_CA_VERSION

CFG += CONFIG_DATABASE_MODULE
CFG += CONFIG_DATABASE_VERSION

include $(TOP)/configure/RULES

