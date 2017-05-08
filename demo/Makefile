TARGET = demo

.PHONY: all clean cleanall

all: $(TARGET)
	
%: %.cpp ../src/libknarr.a
	$(CXX) $(CXXFLAGS) $(OPTIMIZE_FLAGS) -o $@ $< $(INCS) $(LDFLAGS) $(LDLIBS)
clean: 
	-rm -fr *.o *~
cleanall:
	-rm -fr *.o *~ 
	-rm -fr $(TARGET)
