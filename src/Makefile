.PHONY = all lib
OBJS = knarr.o

all: 
	$(MAKE) -C external
	$(MAKE) lib

%.o: %.cpp *.hpp
	$(CXX) $(CXXFLAGS) $(INCS) $(OPTIMIZE_FLAGS) -c -o $@ $<
lib: $(OBJS)
	ar rs libknarr.a $(OBJS)
	ar -M < buildlib.ar 
clean: 
	-rm -fr *.o *~
cleanall:
	-rm -fr *.o *~ *.a
