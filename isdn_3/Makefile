TOPDIR=..
include $(TOPDIR)/Make.config

PSOURCES := isdn_3.c $(addsuffix .c,$(PROTOCOLS))
ifneq ($(strip $(filter phone, $(PROTOCOLS))),)
ifneq ($(strip $(filter euro, $(SUBPROTOCOLS))),)
PSOURCES += prot_ETS.c
endif
ifneq ($(strip $(filter german, $(SUBPROTOCOLS))),)
PSOURCES += prot_1TR6_1.c
PSOURCES += prot_1TR6_common.c
endif
endif

EXTRA_CFLAGS=$(addsuffix _,$(addprefix -D_,$(PROTOCOLS) $(SUBPROTOCOLS)))

LIB = ../isdn_3.a

include $(TOPDIR)/Make.rules
