TARGET = test1 test2 test3

.PHONY: all clean cleanall

all: $(TARGET)
	@./runtests.sh
%: %.cpp ../src/libknarr.a
	$(CXX) $(CXXFLAGS) $(OPTIMIZE_FLAGS) -o $@ $< $(INCS) $(LDFLAGS) $(LDLIBS) -fopenmp
clean: 
	-rm -fr *.o *~
cleanall:
	-rm -fr *.o *~ 
	-rm -fr $(TARGET)
