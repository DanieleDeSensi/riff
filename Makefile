export CC                    = gcc
export CXX                   = g++
export OPTIMIZE_FLAGS        = -O3
export CXXFLAGS              = -Wall -pedantic --std=c++11
export LDLIBS                = -lm -pthread -lrt -L $(realpath .)/src/ -lknarr -lanl

.PHONY: all demo test clean cleanall

all:
	$(MAKE) -C src
demo:
	$(MAKE) -C demo
test:
	$(MAKE) -C test
clean:
	$(MAKE) -C src clean
	$(MAKE) -C demo clean
cleanall:
	$(MAKE) -C src cleanall
	$(MAKE) -C demo cleanall

