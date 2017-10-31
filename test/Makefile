TARGET = test1 test2 test3 test4

.PHONY: all clean cleanall

all: $(TARGET)
	@./runtests.sh
%: %.cpp ../src/libriff.a
	$(CXX) $(CXXFLAGS) $(OPTIMIZE_FLAGS) -o $@ $< $(INCS) $(LDFLAGS) $(LDLIBS) -fopenmp
clean: 
	-rm -fr *.o *~
cleanall:
	-rm -fr *.o *~ 
	-rm -fr $(TARGET)
