YOSYS ?= ${YOSYS_SRC}/yosys
SNL_X2Y ?= /Users/xtof/WORK/naja/build/src/apps/x2y/naja_x2y

all: vexriscv.demo.GenFull_snl.v 

interface.txt:
	capnp convert --verbose packed:text ../../capnp/snl_interface.capnp DBInterface < snl/db_interface.snl > interface.txt

implementation.txt:
	capnp decode --packed ../../capnp/snl_implementation.capnp DBImplementation < snl/db_implementation.snl > implementation.txt

vexriscv.demo.GenFull_netlist.v: src/vexriscv.demo.GenFull.v
	${YOSYS} -m snl-yosys-plugin src/synth.ys

vexriscv.demo.GenFull_snl.v: vexriscv.demo.GenFull_netlist.v
	${SNL_X2Y} -a snl -b verilog -i snl -o vexriscv.demo.GenFull_snl.v #>& toto

clean:
	-rm vexriscv.demo.GenFull_netlist.v vexriscv.demo.GenFull_snl.v
