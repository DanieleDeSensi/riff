.PHONY = all lib
OBJS = knarr.o

all: 
	$(MAKE) -C external
	$(MAKE) lib

%.o: %.cpp *.hpp
	$(CXX) $(CXXFLAGS) $(INCS) $(OPTIMIZE_FLAGS) -c -o $@ $<
lib: $(OBJS)
ifeq ("$(wildcard libknarr.a)","")
	ar rs libknarr.a $(OBJS)
	ar -M < buildlib.ar 
endif
clean: 
	-rm -fr *.o *~
cleanall:
	-rm -fr *.o *~ *.a
