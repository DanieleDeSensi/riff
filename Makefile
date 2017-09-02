export CC                    = gcc
export CXX                   = g++
export OPTIMIZE_FLAGS        = -O3
export CXXFLAGS              = $(COVERAGE_FLAGS) -Wall -pedantic --std=c++11
export LDLIBS                = $(COVERAGE_LIBS) -lm -pthread -lrt -L $(realpath .)/src/ -lknarr -lanl

.PHONY: all demo test cppcheck clean cleanall

all:
	$(MAKE) -C src
demo:
	$(MAKE) -C demo
test:
	$(MAKE) -C test
cppcheck:
	cppcheck --xml --xml-version=2 --enable=warning,performance,information,style --error-exitcode=1 -UNN_EXPORT . -isrc/external -itest 2> cppcheck-report.xml
clean:
	$(MAKE) -C src clean
	$(MAKE) -C demo clean
	$(MAKE) -C test clean
cleanall:
	$(MAKE) -C src cleanall
	$(MAKE) -C demo cleanall
	$(MAKE) -C test cleanall

