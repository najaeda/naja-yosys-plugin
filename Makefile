CXX_FLAGS ?= -O2
LD_FLAGS ?= 
OBJECTS = yosys_plugin.o yosys_debug.o snl_common.capnp.o snl_interface.capnp.o snl_implementation.capnp.o
LIBNAME = snl-yosys-plugin.so
# Default command substitution for yosys
DESTDIR ?= --datdir
INCDIRS = /opt/homebrew/include

all: $(LIBNAME)

$(LIBNAME): $(OBJECTS)
	yosys-config --build $@ $^ -L/opt/homebrew/lib -lcapnp -lkj -shared --ldflags

capnp/%.capnp.c++: capnp/%.capnp
	capnp compile -oc++ $<

%.capnp.o: capnp/%.capnp.c++
	g++ $(CXXFLAGS) -isystem /opt/homebrew/include -std=gnu++20 -o $@ -c $<

yosys_%.o: src/yosys_%.cpp snl_common.capnp.o snl_interface.capnp.o snl_implementation.capnp.o
	g++ -std=c++20 -Wall -Wextra -ggdb -Icapnp -I$(INCDIRS) -I"/opt/homebrew/Cellar/yosys/0.31/share/yosys/include" -MD -MP -DSNL_YOSYS_PLUGIN_DEBUG -D_YOSYS_ -fPIC -I/opt/homebrew/Cellar/yosys/0.31/include -std=c++14 -Os -c $< 
	#yosys-config --exec --cxx -c --cxxflags -std=c++14 -I $(INCDIRS) -I capnp $(CXX_FLAGS) -o $@ $<

install: $(LIBNAME)
	yosys-config --exec mkdir -p $(DESTDIR)/plugins/
	yosys-config --exec cp $(LIBNAME) $(DESTDIR)/plugins/

clean:
	rm $(OBJECTS)
