CONTIKI_PROJECT = simple_packet
all: $(CONTIKI_PROJECT)

CONTIKI = $(HOME)/contiki-ng


MAKE_MAC ?= MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_NULLNET
PROJECT_SOURCEFILES+=my_sensor.c
PROJECT_SOURCEFILES+=my_functions.c
include $(CONTIKI)/Makefile.include
