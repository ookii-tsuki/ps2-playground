EE_BIN = graphics.elf
EE_OBJS = graphics.o
EE_LIBS = -ldmakit -lgskit -ldebug -lkernel

# Add PS2SDK includes
EE_INCS += -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2DEV)/gsKit/include


include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

clean:
	rm -f $(EE_BIN) $(EE_OBJS)