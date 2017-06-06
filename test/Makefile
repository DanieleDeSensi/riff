TARGET = test1 

.PHONY: all clean cleanall

all: $(TARGET)
	@./runtests.sh
%: %.cpp ../src/libknarr.a
	$(CXX) -coverage $(CXXFLAGS) $(OPTIMIZE_FLAGS) -o $@ $< $(INCS) $(LDFLAGS) $(LDLIBS) -fopenmp
clean: 
	-rm -fr *.o *~
cleanall:
	-rm -fr *.o *~ 
	-rm -fr $(TARGET)
