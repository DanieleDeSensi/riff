.PHONY = all lib
OBJS = riff.o

all:
	$(CXX) $(CXXFLAGS) $(OPTIMIZE_FLAGS) -o ticksPerNs ticksPerNs.cpp
	./build_archdata.sh
	$(MAKE) -C external
	$(MAKE) lib
%.o: %.cpp *.hpp
	$(CXX) $(CXXFLAGS) $(INCS) $(OPTIMIZE_FLAGS) -c -o $@ $<
lib: $(OBJS)
	rm -rf libriff.a
	ar rs libriff.a $(OBJS)
	ar -M < buildlib.ar 
clean: 
	-rm -fr *.o *~ ticksPerNs
cleanall:
	-rm -fr *.o *~ ticksPerNs *.a
