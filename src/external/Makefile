TARGET = all

all:
ifeq ("$(wildcard nanomsg/build/libnanomsg.a)","")
	cd nanomsg && mkdir -p build && cd build && cmake  .. && cmake --build .
endif
