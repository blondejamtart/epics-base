#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# This is a Makefile fragment, see src/libCom/Makefile.

SRC_DIRS += $(LIBCOM)/env

vpath %.pl $(USR_VPATH) $(SRC_DIRS)

PERL_SCRIPTS += bldEnvData.pl

INC += envDefs.h

Com_SRCS += envSubr.c
Com_SRCS += envData.c

CLEANS += envData.c
