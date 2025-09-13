CONTIKI_PROJECT = sensor_data_send
all: $(CONTIKI_PROJECT)

PROJECT_SOURCEFILES += my_sensor.c
PROJECT_SOURCEFILES += my_function.c

CONTIKI = $(HOME)/contiki-ng

MAKE_MAC ?= MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_NULLNET

include $(CONTIKI)/Makefile.include
