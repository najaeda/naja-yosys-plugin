YOSYS ?= yosys
SNL_X2Y ?= ../../build/src/snl/snippets/cpp/snl_vrl_snippet

all: vexriscv.demo.GenFull_snl.v 

vexriscv.demo.GenFull_netlist.v: src/vexriscv.demo.GenFull.v
	${YOSYS} -m snl-yosys-plugin src/synth.ys

vexriscv.demo.GenFull_snl.v: vexriscv.demo.GenFull_netlist.v
	${SNL_X2Y} -a snl -b verilog -i snl -o vexriscv.demo.GenFull_snl.v #>& toto

clean:
	-rm vexriscv.demo.GenFull_netlist.v vexriscv.demo.GenFull_snl.v
