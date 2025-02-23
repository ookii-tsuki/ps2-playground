PROGRAMS = hello graphics

ifdef program
all:
	$(MAKE) -C src/$(program)

clean:
	$(MAKE) -C src/$(program) clean
else
all:
	@for prog in $(PROGRAMS); do \
		$(MAKE) -C src/$$prog; \
	done

clean:
	@for prog in $(PROGRAMS); do \
		$(MAKE) -C src/$$prog clean; \
	done
endif