#include "kernel/yosys.h"
#include "kernel/sigtools.h"

#include <filesystem>
#include <fcntl.h>
#include <unistd.h>

#include "snl_interface.capnp.h"
#include "snl_implementation.capnp.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN


namespace fs = std::__fs::filesystem;

namespace {

DBInterface::LibraryInterface::DesignInterface::Direction YosysToCapnPDirection(const RTLIL::Wire* wire) {
  auto direction = DBInterface::LibraryInterface::DesignInterface::Direction::INOUT;
	if (!wire->port_output) {
    direction = DBInterface::LibraryInterface::DesignInterface::Direction::INPUT;
  } else if (!wire->port_input) {
    direction = DBInterface::LibraryInterface::DesignInterface::Direction::OUTPUT;
  }
  return direction;
}

void dumpScalarTerm(
  DBInterface::LibraryInterface::DesignInterface::Term::Builder& term,
  const RTLIL::Wire* wire,
  size_t id) {
  auto scalarTermBuilder = term.initScalarTerm();
  scalarTermBuilder.setId(id);
  scalarTermBuilder.setName(wire->name.str());
  scalarTermBuilder.setDirection(YosysToCapnPDirection(wire));
}

void dumpBusTerm(
  DBInterface::LibraryInterface::DesignInterface::Term::Builder& term,
  const RTLIL::Wire* wire,
  size_t id) {
  auto busTermBuilder = term.initBusTerm();
  busTermBuilder.setId(id);
  busTermBuilder.setName(wire->name.str());
  busTermBuilder.setDirection(YosysToCapnPDirection(wire));
  auto start = wire->start_offset;
  auto end = wire->start_offset + wire->width - 1;
  auto msb = (wire->upto) ? start : end;
  auto lsb = (wire->upto) ? end : start;
  busTermBuilder.setMsb(msb);
  busTermBuilder.setLsb(lsb);
}

void dumpPorts(
  DBInterface::LibraryInterface::DesignInterface::Builder& design,
  RTLIL::Module* module) {
  using Ports = std::vector<RTLIL::Wire*>;
  Ports ports;
  for (auto wire : module->wires()) {
    if (wire->port_id == 0) {
      continue;
    }
    ports.push_back(wire);
  }
  if (not ports.empty()) {
    auto terms = design.initTerms(ports.size());
    size_t portID = 0;
    for (auto wire : module->wires()) {
      if (wire->port_id == 0) {
        continue;
      }
      auto term = terms[portID];
      if (wire->width == 1) {
        dumpScalarTerm(term, wire, portID);
      } else {
        dumpBusTerm(term, wire, portID);
      }
      ++portID;
    }
  }
}

void dumpBusNet(
  DBImplementation::LibraryImplementation::DesignImplementation::Net::Builder& net,
  const RTLIL::Wire* wire,
  size_t id) {
  auto busNetBuilder = net.initBusNet();
  busNetBuilder.setId(id);
  busNetBuilder.setName(wire->name.str());
  auto start = wire->start_offset;
  auto end = wire->start_offset + wire->width - 1;
  auto msb = (wire->upto) ? start : end;
  auto lsb = (wire->upto) ? end : start;
  busNetBuilder.setMsb(msb);
  busNetBuilder.setLsb(lsb);
}

void dumpScalarNet(
  DBImplementation::LibraryImplementation::DesignImplementation::Net::Builder& net,
  const RTLIL::Wire* wire,
  size_t id) {
  auto scalarNetBuilder = net.initScalarNet();
  scalarNetBuilder.setId(id);
  scalarNetBuilder.setName(wire->name.str());
}

void dumpWire(
  const RTLIL::Wire* wire,
  DBImplementation::LibraryImplementation::DesignImplementation::Net::Builder& net,
  size_t netID) {
  if (wire->width != 1) {
    dumpBusNet(net, wire, netID);
  } else {
    dumpScalarNet(net, wire, netID);
  }
}

//-1 means bbox
using Models = std::map<std::string, int>;
using Modules = std::vector<RTLIL::Module*>;

void dumpInterface(
  const Modules& primitiveModules,
  const Modules& userModules,
  const fs::path& interfacePath, Models& models) {
  ::capnp::MallocMessageBuilder message;

  DBInterface::Builder db = message.initRoot<DBInterface>();
  db.setId(1);
  auto libraries = db.initLibraryInterfaces(2);
  auto primitivesLibrary = libraries[0];
  primitivesLibrary.setId(0); 
  primitivesLibrary.setType(DBInterface::LibraryType::PRIMITIVES);
  auto designsLibrary = libraries[1];
  designsLibrary.setId(1);

  auto primitives = primitivesLibrary.initDesignInterfaces(primitiveModules.size());
  size_t primitiveID = 0;
  for (auto primitiveModule: primitiveModules) {
    auto primitive = primitives[primitiveID];
    primitive.setId(primitiveID);
    models[primitiveModule->name.str()] = primitiveID;
    primitive.setName(primitiveModule->name.str());
    primitive.setType(DBInterface::LibraryInterface::DesignType::PRIMITIVE);
    //collect ports
    dumpPorts(primitive, primitiveModule);
    ++primitiveID;
  }

  auto designs = designsLibrary.initDesignInterfaces(userModules.size());
  size_t designID = 0;
  int topDesignID = -1;
  for (auto userModule: userModules) {

    if (userModule->get_bool_attribute(ID::top)) {
      topDesignID = designID;
    }
    auto design = designs[designID];
    design.setId(designID);
    design.setName(userModule->name.str());

    //collect ports
    dumpPorts(design, userModule);

    ++designID;
  }

  if (topDesignID > 0) {
    auto designReferenceBuilder = db.initTopDesignReference();
    designReferenceBuilder.setDbID(1);
    designReferenceBuilder.setLibraryID(1);
    designReferenceBuilder.setDesignID(topDesignID);
  }



#if 0
  size_t designsSize = design->modules().size();
  auto designs = library.initDesignInterfaces(designsSize);
  size_t id = 0;
  int topModuleID = -1;
  for (auto module : design->modules()) {
    if (module->get_blackbox_attribute()) {
      models[module->name.str()] = -1;
      continue;
    }

    std::cerr << "Dumping module: " << log_id(module->name) << std::endl;
	  
    auto design = designs[id];
    design.setId(id);
    design.setName(module->name.str());
    models[module->name.str()] = id;
    
    }
    ++id;
  }
#endif
  int fd = open(
    interfacePath.c_str(),
    O_CREAT | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  writePackedMessageToFd(fd, message);
  close(fd);
}

void dumpImplementation(
  const Modules& userModules,
  const fs::path& implementationPath, const Models& models) {
  ::capnp::MallocMessageBuilder message;

  DBImplementation::Builder db = message.initRoot<DBImplementation>();
  db.setId(1);
  auto libraries = db.initLibraryImplementations(1);
  auto library = libraries[0];
  library.setId(1); 

  auto designs = library.initDesignImplementations(userModules.size());
  size_t designID = 0;
  for (auto userModule: userModules) {
    SigMap sigmap(userModule);
    auto design = designs[designID]; 
    design.setId(designID++);
    size_t netsSize = userModule->wires().size();
    if (netsSize > 0) {
      auto nets = design.initNets(netsSize);
      size_t netID = 0;
      for (auto wire : userModule->wires()) {
        auto net = nets[netID];
        dumpWire(wire, net, netID++); 
      }
    }
    size_t instancesSize = userModule->cells().size();
    if (instancesSize > 0) {
      auto instances = design.initInstances(instancesSize);
      size_t instanceID = 0;
      for (auto cell : userModule->cells()) {
        std::cerr << "Dumping cell/instance implementation: " << log_id(cell->name) << std::endl;
        auto instance = instances[instanceID];
        instance.setId(instanceID++);
        instance.setName(cell->name.str());
        auto modelReferenceBuilder = instance.initModelReference();
        modelReferenceBuilder.setDbID(1);
        modelReferenceBuilder.setLibraryID(0);
        auto modelIt = models.find(cell->type.str());
        if (modelIt == models.end()) {
          log_error("Model type %s not found in map for cell %s", log_id(cell->type), log_id(cell->name));
        }
        auto modelID = modelIt->second;
        modelReferenceBuilder.setDesignID(modelID);
        bool first_arg = true;
        std::set<RTLIL::IdString> numbered_ports;
        for (int i = 1; true; i++) {
		      char str[16];
		      snprintf(str, 16, "$%d", i);
		      for (auto it = cell->connections().begin(); it != cell->connections().end(); ++it) {
			      if (it->first != str) {
				      continue;
            }
			      if (!first_arg) {
              std::cerr << stringf(",");
            }
			      first_arg = false;
            std::cerr << stringf("\n%s  ", indent.c_str());
			      //dump_sigspec(f, it->second);
			      numbered_ports.insert(it->first);
			      //goto found_numbered_port;
          }
        }
        for (auto &c : cell->connections()) {
          std::cerr << "Dumping cell connection: " << c.first.str() << std::endl;
          if (cell->output(c.first)) {
						sigmap.add(c.second);
          }
        }
      }
    }
  }
  int fd = open(
    implementationPath.c_str(),
    O_CREAT | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  writePackedMessageToFd(fd, message);
  close(fd);
}

#if 0
    

  }

#endif

void dumpManifest(const fs::path& dir) {
  fs::path manifestPath(dir/"snl.mf");
  std::ofstream stream;
  stream.open(manifestPath, std::ofstream::out);
  stream << "V"
    << " " << 0
    << " " << 0
    << " " << 1
    << std::endl;
}

}

struct SNLBackend: public Backend {
  SNLBackend() : Backend("snl", "write design to Naja SNL netlist file") {}
	void execute(std::ostream *&f, std::string filename, std::vector<std::string> args, RTLIL::Design *design) override {
    log_header(design, "Executing Naja SNL backend.\n");

    fs::path dir("snl");
    fs::create_directory(dir);
    dumpManifest(dir);
    //SNLDumpManifest::dump(path);

    //First collect primitives
    Modules primitiveModules;
    Modules userModules;
    for (auto module : design->modules()) {
      if (module->get_blackbox_attribute()) {
        primitiveModules.push_back(module);
      } else {
        userModules.push_back(module);
      }
    }
    
    Models models;
    dumpInterface(primitiveModules, userModules, dir/"db_interface.snl", models);
    dumpImplementation(userModules, dir/"db_implementation.snl", models);
  }

	void help() override
	{
		log("\n");
		log("    snl_backend [options]\n");
		log("\n");
	}

} SNLBackend;

PRIVATE_NAMESPACE_END
