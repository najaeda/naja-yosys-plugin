CXX_FLAGS ?= -O2
LD_FLAGS ?= 
OBJECTS = yosys_plugin.o snl_common.capnp.o snl_interface.capnp.o snl_implementation.capnp.o
LIBNAME = snl-yosys-plugin.so
# Default command substitution for yosys
DESTDIR ?= --datdir
INCDIRS = /opt/homebrew/include

all: $(LIBNAME)

capnp:
	capnp compile -oc++ capnp/snl_common.capnp capnp/snl_interface.capnp capnp/snl_implementation.capnp

$(LIBNAME): $(OBJECTS)
	yosys-config --build $@ $^ -L/opt/homebrew/lib -lcapnp -lkj -shared --ldflags


%.capnp.o: capnp/%.capnp.c++
	yosys-config --exec --cxxflags -std=c++2a -I $(DESTDIR)/include -I $(INCDIRS) $(CXX_FLAGS) -o $@ $<

yosys_plugin.o: src/yosys_plugin.cpp
	g++ -std=c++20 -Wall -Wextra -ggdb -Icapnp -I$(INCDIRS) -I"/opt/homebrew/Cellar/yosys/0.30/share/yosys/include" -MD -MP -D_YOSYS_ -fPIC -I/opt/homebrew/Cellar/yosys/0.30/include -std=c++14 -Os -c $< 
	#yosys-config --exec --cxx -c --cxxflags -std=c++14 -I $(INCDIRS) -I capnp $(CXX_FLAGS) -o $@ $<

install: $(LIBNAME)
	yosys-config --exec mkdir -p $(DESTDIR)/plugins/
	yosys-config --exec cp $(LIBNAME) $(DESTDIR)/plugins/

clean:
	rm $(OBJECTS)
