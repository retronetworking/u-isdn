TOPDIR=..
include $(TOPDIR)/Make.config

PSOURCES := isdn_3.c $(addsuffix .c,$(PROTOCOLS))
ifneq ($(strip $(filter capi, $(PROTOCOLS))),)
TCOMMON=1
ECOMMON=1
endif
ifneq ($(strip $(filter phone, $(PROTOCOLS))),)
ifneq ($(strip $(filter euro, $(SUBPROTOCOLS))),)
PSOURCES += prot_ETS.c
ECOMMON=1
endif
ifneq ($(strip $(filter german, $(SUBPROTOCOLS))),)
PSOURCES += prot_1TR6_1.c
TCOMMON=1
endif
endif
ifeq ($(ECOMMON),1)
PSOURCES += prot_ETS_common.c
endif
ifeq ($(TCOMMON),1)
PSOURCES += prot_1TR6_common.c
endif

EXTRA_CFLAGS=$(addsuffix _,$(addprefix -D_,$(PROTOCOLS) $(SUBPROTOCOLS)))

LIB = ../isdn_3.a

include $(TOPDIR)/Make.rules
