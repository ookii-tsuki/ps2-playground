EE_BIN = graphics.elf
EE_OBJS = graphics.o
EE_LIBS = -ldmakit -lgskit -ldebug -lkernel

# Add PS2SDK includes
EE_INCS += -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2DEV)/gsKit/include

# Add PS2DEV library paths
EE_LDFLAGS += -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ee/lib

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

clean:
	rm -f $(EE_BIN) $(EE_OBJS)