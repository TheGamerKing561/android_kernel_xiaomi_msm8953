ifeq ($(CONFIG_MACH_XIAOMI_TITANIUM),y)
obj-$(CONFIG_INPUT_TOUCHSCREEN) += touchscreen/
else
ccflags-y := -Wno-unused-function
obj-y := stub.o
endif