# PS2 SDK tools
EE_BIN = hello.elf
EE_OBJS = hello.o
EE_LIBS = -ldebug -lkernel

# Add PS2SDK includes
EE_INCS += -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include

# Use default rules from PS2SDK
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

# Clean target
clean:
	rm -f $(EE_BIN) $(EE_OBJS)