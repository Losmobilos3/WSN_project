CONTIKI_PROJECT = main
all: $(CONTIKI_PROJECT)

# Enable NULLNET
MAKE_NET = MAKE_NET_NULLNET

CONTIKI = ../..
include $(CONTIKI)/Makefile.include
