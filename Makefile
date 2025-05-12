CXX := $(shell yosys-config --cxx)
YOSYS_LD_FLAGS := $(shell yosys-config --ldflags --ldlibs)
CXX_FLAGS ?=
YOSYS_CXX_FLAGS := $(shell yosys-config --cxxflags)
YOSYS_INCLUDES ?= $(YOSYS_SRC)

OBJECTS = yosys_plugin.o yosys_debug.o snl_common.capnp.o snl_interface.capnp.o snl_implementation.capnp.o
LIBNAME = snl-yosys-plugin.so
# Default command substitution for yosys
YOSYS_DESTDIR ?= $(shell yosys-config --datdir)
INCDIRS = /opt/homebrew/include

all: $(LIBNAME)

$(LIBNAME): $(OBJECTS)
	yosys-config --build $@ $^ -L/opt/homebrew/lib -lcapnp -lkj -shared --ldflags

capnp/%.capnp.c++: capnp/%.capnp
	capnp compile -oc++ $<

%.capnp.o: capnp/%.capnp.c++
	$(CXX) $(CXXFLAGS) -isystem /opt/homebrew/include -std=gnu++20 -o $@ -c $<

yosys_%.o: src/yosys_%.cpp snl_common.capnp.o snl_interface.capnp.o snl_implementation.capnp.o
	#$(CXX) -std=c++20 -Wall -Wextra -ggdb -Icapnp -I$(INCDIRS) -I$(YOSYS) -MD -MP -DSNL_YOSYS_PLUGIN_DEBUG -D_YOSYS_ -fPIC -Os -c $< 
	#yosys-config --exec --cxx -c --cxxflags -std=c++14 -I $(INCDIRS) -I capnp $(CXX_FLAGS) -o $@ $<
	$(CXX) -c $(YOSYS_CXX_FLAGS) $(CXX_FLAGS) -I$(YOSYS_INCLUDES) -Icapnp -std=c++20 -o $@ $<


install: $(LIBNAME)
	mkdir -p $(YOSYS_DESTDIR)/share/plugins/
	cp $(LIBNAME) $(YOSYS_DESTDIR)/share/plugins/

clean:
	rm $(OBJECTS)
