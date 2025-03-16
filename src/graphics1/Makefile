# PS2 SDK tools
EE_BIN = graphics1.elf
EE_OBJS = graphics1.o model.o
EE_LIBS = -ldma -lgraph -ldraw -ldebug -lkernel -lpacket -lmath3d

# Add PS2SDK includes
EE_INCS += -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include

# Use default rules from PS2SDK
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

# Clean target
clean:
	rm -f $(EE_BIN) $(EE_OBJS)