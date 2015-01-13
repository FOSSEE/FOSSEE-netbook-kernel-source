##########################################
# Makefile for ISDBT Device Drivers.
##########################################
#ccflags-y += -Idrivers/misc/isdbt
#ccflags-y += -v

#EXTRA_CFLAGS += $(extra-cflags-y) $(extra-cflags-m)

#obj-$(ISDBT_VENDOR_RAONTECH) := mtv231.o

# MTV231
#ccflags-$(CONFIG_ISDBT_MTV23x) += -Idrivers/misc/isdbt/mtv23x
#obj-$(CONFIG_ISDBT_MTV23x) +=../../src/

# core
MTVOBJ := mtv23x.o mtv23x_demod.o mtv23x_rf.o mtv23x_port.o isdbt_isr.o isdbt.o

# SPI
#obj-y += isdbt_spi.o
#obj-y += isdbt_isr.o

# SPI(Control) + TSIF(Data)
MTVOBJ += isdbt_spi.o

# TSIF or spi slave
MTVOBJ += isdbt_i2c.o

# CAMIF
#obj-y += isdbt_i2c.o

mtv231-objs := $(MTVOBJ)

obj-$(CONFIG_ISDBT_RAONTECH_MTV) += mtv231.o
