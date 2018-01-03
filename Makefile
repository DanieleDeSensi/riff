export CC                    = gcc
export CXX                   = g++
export OPTIMIZE_FLAGS        = -O3
export CXXFLAGS              = $(RIFF_COVERAGE_FLAGS) -Wall -pedantic --std=c++11
export LDLIBS                = $(RIFF_COVERAGE_LIBS) -lm -pthread -lrt -L $(realpath .)/src/ -lriff -lanl

.PHONY: all demo test cppcheck clean cleanall develcheck cppcheck gcov

all:
	$(MAKE) -C src
demo:
	$(MAKE) -C demo
test:
	$(MAKE) -C test -j
cppcheck:
	cppcheck --xml --xml-version=2 --enable=warning,performance,style --error-exitcode=1 --suppressions-list=./test/cppcheck/suppressions-list.txt -UNN_EXPORT . -isrc/external -itest 2> cppcheck-report.xml || (cat cppcheck-report.xml; exit 2) 
gcov:
	./test/gcov/gcov.sh
develcheck:
	$(MAKE) "RIFF_COVERAGE_FLAGS=-fprofile-arcs -ftest-coverage" && $(MAKE) cppcheck && \
	$(MAKE) "RIFF_COVERAGE_FLAGS=-DTOLERANCE=0.1 -fprofile-arcs -ftest-coverage" RIFF_COVERAGE_LIBS=-lgcov test && $(MAKE) gcov
clean:
	$(MAKE) -C src clean
	$(MAKE) -C demo clean
	$(MAKE) -C test clean
cleanall:
	$(MAKE) -C src cleanall
	$(MAKE) -C demo cleanall
	$(MAKE) -C test cleanall

