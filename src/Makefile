.PHONY = all lib
OBJS = knarr.o ticksPerNs.o

all: 
	$(MAKE) ticksPerNs
	./build_archdata.sh
	$(MAKE) -C external
	$(MAKE) lib

%: %.cpp 
	$(CXX) $(CXXFLAGS) $(OPTIMIZE_FLAGS) -o $@ $< 
%.o: %.cpp *.hpp
	$(CXX) $(CXXFLAGS) $(INCS) $(OPTIMIZE_FLAGS) -c -o $@ $<
lib: $(OBJS)
	ar rs libknarr.a $(OBJS)
	ar -M < buildlib.ar 
clean: 
	-rm -fr *.o *~
cleanall:
	-rm -fr *.o *~ *.a
