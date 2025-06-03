YOSYS_CONFIG := /Users/noamcohen/dev/OR/OpenROAD-flow-scripts/tools/yosys/yosys-config
YOSYS_SRC := /Users/noamcohen/dev/OR/OpenROAD-flow-scripts/tools/yosys
CXX := $(shell $(YOSYS_CONFIG) --cxx)
YOSYS_LD_FLAGS := $(shell $(YOSYS_CONFIG) --ldflags --ldlibs)
CXX_FLAGS ?= -I/opt/homebrew/Cellar/capnp/1.1.0_1/include -I$(PWD)/capnp -DDEBUG -g
YOSYS_CXX_FLAGS := $(shell $(YOSYS_CONFIG) --cxxflags)
YOSYS_INCLUDES ?= $(YOSYS_SRC)

OBJECTS = yosys_plugin.o yosys_debug.o snl_common.capnp.o snl_interface.capnp.o snl_implementation.capnp.o
LIBNAME = snl-yosys-plugin.so
# Default command substitution for yosys
YOSYS_DESTDIR ?= $(shell $(YOSYS_CONFIG) --datdir)
INCDIRS = /opt/homebrew/Cellar/capnp/1.1.0_1/include 

all: $(LIBNAME)

$(LIBNAME): $(OBJECTS)
	$(YOSYS_CONFIG) --build $@ $^ -fPIC -std=c++17 -stdlib=libc++ -DDEBUG -g -O2 -fno-omit-frame-pointer -L/opt/homebrew/lib -lc++abi -lcapnp -lkj -shared --ldflags  -L/opt/homebrew/Cellar/llvm/20.1.6/lib/c++

capnp/%.capnp.c++: capnp/%.capnp
	capnp compile -oc++ $<

%.capnp.o: capnp/%.capnp.c++
	$(CXX) $(CXXFLAGS) -isystem /opt/homebrew/Cellar/capnp/1.1.0_1/include -std=gnu++17 -o $@ -c $<

yosys_%.o: src/yosys_%.cpp snl_common.capnp.o snl_interface.capnp.o snl_implementation.capnp.o
	#$(CXX)  -L/opt/homebrew/Cellar/llvm/20.1.6/lib/c++ -g -DDEBUG -O2  -fno-omit-frame-pointer -std=c++17 -Wall -Wextra -ggdb -Icapnp -I$(INCDIRS) -I$(YOSYS) -MD -MP -DSNL_YOSYS_PLUGIN_DEBUG -D_YOSYS_ -fPIC -Os -c $< 
	#$(YOSYS_CONFIG) --exec --cxx -c --cxxflags -std=c++17 -I $(INCDIRS) -I capnp $(CXX_FLAGS) -o $@ $<
	$(CXX) -c $(YOSYS_CXX_FLAGS) $(CXX_FLAGS) -I$(YOSYS_INCLUDES) -Icapnp -std=c++17 -o $@ $<


install: $(LIBNAME)
	mkdir -p $(YOSYS_DESTDIR)/share/plugins/
	cp $(LIBNAME) $(YOSYS_DESTDIR)/share/plugins/

clean:
	rm $(OBJECTS)
