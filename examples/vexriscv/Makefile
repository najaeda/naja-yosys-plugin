all: vexriscv.demo.GenFull_netlist.v

interface.txt:
	capnp convert --verbose packed:text ../../capnp/snl_interface.capnp DBInterface < snl/db_interface.snl > interface.txt

implementation.txt:
	capnp decode --packed ../../capnp/snl_implementation.capnp DBImplementation < snl/db_implementation.snl > implementation.txt

vexriscv.demo.GenFull_netlist.v: src/vexriscv.demo.GenFull.v
	${YOSYS} -m naja-if src/synth.ys

clean:
	-rm vexriscv.demo.GenFull_netlist.v vexriscv.demo.GenFull_snl.v
