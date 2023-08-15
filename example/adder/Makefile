YOSYS ?= yosys
SNL_X2Y ?= /Users/xtof/WORK/naja/build/src/apps/x2y/naja_x2y
PRIMITIVES ?= ../../primitives/xilinx.py 

all: adder_netlist.v adder_snl.v #primitives.v verilator

adder_netlist.v: src/adder.v
	${YOSYS} -m snl-yosys-plugin src/synth.ys

adder_snl.v primitives.v: adder_netlist.v
	${SNL_X2Y} -a snl -b verilog -i snl -o adder_snl.v #>& toto

clean:
	-rm adder_snl.v primitives.v adder_netlist.v 
