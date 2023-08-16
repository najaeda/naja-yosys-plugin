#include "kernel/yosys.h"
#include "kernel/sigtools.h"

#include <filesystem>
#include <fcntl.h>
#include <unistd.h>

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

#include "snl_interface.capnp.h"
#include "snl_implementation.capnp.h"
#include "yosys_debug.h"


USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

namespace fs = std::__fs::filesystem;

namespace {

std::string getName(const RTLIL::IdString& yosysName) {
  return RTLIL::unescape_id(yosysName);
}

DBInterface::LibraryInterface::DesignInterface::Direction YosysToCapnPDirection(const RTLIL::Wire* wire) {
  auto direction = DBInterface::LibraryInterface::DesignInterface::Direction::INOUT;
	if (!wire->port_output) {
    direction = DBInterface::LibraryInterface::DesignInterface::Direction::INPUT;
  } else if (!wire->port_input) {
    direction = DBInterface::LibraryInterface::DesignInterface::Direction::OUTPUT;
  }
  return direction;
}

int getSize(int msb, int lsb) {
  return std::abs(lsb - msb) + 1;
}

struct Component {
  int instanceID_ = 0;
  bool isTerm_ = false;
  bool isBus_ = false;
  int termID_ = 0;
  int bit_ = 0;
  Component(int termID, bool isBus, int bit): isTerm_(true), isBus_(isBus), termID_(termID), bit_(bit) {}
  Component(int instanceID, int termID, bool isBus, int bit): instanceID_(instanceID), isBus_(isBus), termID_(termID), bit_(bit) {}
};

struct Bit {
  using Components = std::vector<Component>;
  Bit() = default;

  Components  components_ {};
  int         bit_        {0};
};

struct Net {
  using Bits = std::vector<Bit>;
  bool  isBus_  {false};
  int msb_ {0};
  int lsb_ {0};
  Bits  bits_   {};
  Net(): bits_(1) {}
  Net(int msb, int lsb): isBus_(true), msb_(msb), lsb_(lsb), bits_(getSize(msb, lsb)) {
    int incr = (msb<lsb)?+1:-1;
    int i=0;
    for (auto& bit: bits_) {
      bit.bit_ = msb_ + i*incr; 
      ++i;
    }
  }
};

using Nets = std::map<const RTLIL::Wire*, Net>;
using Terms = std::map<int, int>; //port_id, termid

struct Model {
  int   libraryID_  {0};
  int   designID_   {0};
  Terms terms_      {};
  Model(int libraryID, int designID): libraryID_(libraryID), designID_(designID) {}
};
using Models = std::map<std::string, Model>;

void dumpInstTermReference(
    DBImplementation::LibraryImplementation::DesignImplementation::NetComponentReference::Builder& dumpComponent,
    const Component& component) {
  auto instTermRefenceBuilder = dumpComponent.initInstTermReference();
  instTermRefenceBuilder.setInstanceID(component.instanceID_);
  instTermRefenceBuilder.setTermID(component.termID_);
  if (component.isBus_) {
    instTermRefenceBuilder.setBit(component.bit_);
  }
#if SNL_YOSYS_PLUGIN_DEBUG
  std::cerr << "Dumping inst term reference: " << component.instanceID_ << ":" << component.termID_;
  if (component.isBus_) {
    std::cerr << "[" << component.bit_ << "]";
  }
  std::cerr << std::endl;
#endif
}

void dumpTermReference(
    DBImplementation::LibraryImplementation::DesignImplementation::NetComponentReference::Builder& dumpComponent,
    const Component& component) {
  auto termRefenceBuilder = dumpComponent.initTermReference();
  termRefenceBuilder.setTermID(component.termID_);
  if (component.isBus_) {
    termRefenceBuilder.setBit(component.bit_);
  }
#if SNL_YOSYS_PLUGIN_DEBUG
  std::cerr << "Dumping term reference: " << component.termID_;
  if (component.isBus_) {
    std::cerr << "[" << component.bit_ << "]";
  }
  std::cerr << std::endl;
#endif
}

void dumpNetComponentReference(
    DBImplementation::LibraryImplementation::DesignImplementation::NetComponentReference::Builder& dumpComponent,
    const Component& component) {
  if (component.isTerm_) {
    dumpTermReference(dumpComponent, component);
  } else {
    dumpInstTermReference(dumpComponent, component);
  }
}

void dumpScalarNet(
    DBImplementation::LibraryImplementation::DesignImplementation::Net::Builder& dumpNet,
    const std::string& name,
    const Net& net,
    size_t id) {
  auto scalarNetBuilder = dumpNet.initScalarNet();
  scalarNetBuilder.setId(id);
  scalarNetBuilder.setName(name);
#if SNL_YOSYS_PLUGIN_DEBUG
  std::cerr << "Dumping scalar net: " << name << " with ID: " << id << std::endl;
#endif
  assert(net.bits_.size() == 1);
  auto bit = net.bits_[0];
  size_t componentsSize = bit.components_.size();
  if (componentsSize > 0) {
    auto components = scalarNetBuilder.initComponents(componentsSize);
    size_t componentID = 0;
    for (auto component: bit.components_) {
      auto componentRefBuilder = components[componentID++];
      dumpNetComponentReference(componentRefBuilder, component);
    }
  }
}

void dumpBusNetBit(
  DBImplementation::LibraryImplementation::DesignImplementation::BusNetBit::Builder& dumpBit,
  const Bit& bit) {
  dumpBit.setBit(bit.bit_);
#if SNL_YOSYS_PLUGIN_DEBUG
  std::cerr << "Dumping bus net bit: " << bit.bit_ << std::endl;
#endif
  size_t componentsSize = bit.components_.size();
  if (componentsSize > 0) {
    auto components = dumpBit.initComponents(componentsSize);
    size_t id = 0;
    for (auto component: bit.components_) {
      auto componentRefBuilder = components[id++];
      dumpNetComponentReference(componentRefBuilder, component);
    }
  }
}

void dumpBusNet(
    DBImplementation::LibraryImplementation::DesignImplementation::Net::Builder& dumpNet,
    const std::string& name,
    const Net& net,
    size_t id) {
  auto busNetBuilder = dumpNet.initBusNet();
  busNetBuilder.setId(id);
  busNetBuilder.setName(name);
  busNetBuilder.setMsb(net.msb_);
  busNetBuilder.setLsb(net.lsb_);
#if SNL_YOSYS_PLUGIN_DEBUG
  std::cerr << "Dumping bus net: " << name << "[" << net.msb_ << ":" << net.lsb_ << "]" << std::endl;
#endif
  auto bits = busNetBuilder.initBits(getSize(net.msb_, net.lsb_));
  size_t bid = 0;
  for (auto bit: net.bits_) {
    auto bitBuilder = bits[bid++];
    dumpBusNetBit(bitBuilder, bit);
  }
}

void dumpInstParameter(
  DBImplementation::LibraryImplementation::DesignImplementation::Instance::InstParameter::Builder& instParameter,
  const std::string& name) {
  instParameter.setName(name);
  //instParameter.setValue(snlInstParameter->getValue());
}

void dumpInstanceParameters(
  DBImplementation::LibraryImplementation::DesignImplementation::Instance::Builder& instance,
  const RTLIL::Cell* cell) {
  size_t instParametersSize = cell->parameters.size();
  if (instParametersSize > 0) { 
    auto instParameters = instance.initInstParameters(instParametersSize);
    size_t id = 0;
    for (auto it = cell->parameters.begin(); it != cell->parameters.end(); ++it) {
      auto instParameterBuilder = instParameters[id++];
      dumpInstParameter(instParameterBuilder, getName(it->first.c_str()));
    }
  }
}

void dumpParameter(
  DBInterface::LibraryInterface::DesignInterface::Parameter::Builder& parameter,
  const std::string& name) {
  parameter.setName(name);
  //parameter.setType(SNLtoCapNpParameterType(snlParameter->getType()));
  //parameter.setValue(snlParameter->getValue());
}

void dumpParameters(
  DBInterface::LibraryInterface::DesignInterface::Builder& design,
  RTLIL::Module* module) {
#if SNL_YOSYS_PLUGIN_DEBUG
  std::cerr << "Dumping parameters: " << module->avail_parameters.size() << std::endl;
  for (auto parameter: module->avail_parameters) {
    std::cerr << "Dumping parameter: " << parameter.c_str() << std::endl;

  }
#endif
  size_t parametersSize = module->avail_parameters.size();
  if (parametersSize > 0) {
    size_t id = 0;
    auto parameters = design.initParameters(parametersSize);
    for (auto parameter: module->avail_parameters) {
      auto parameterBuilder = parameters[id++];
      dumpParameter(parameterBuilder, getName(parameter));
    }
  }
}

void dumpScalarTerm(
  DBInterface::LibraryInterface::DesignInterface::Term::Builder& term,
  const RTLIL::Wire* wire,
  size_t id,
  Model& model) {
  auto scalarTermBuilder = term.initScalarTerm();
  scalarTermBuilder.setId(id);
  auto termName = getName(wire->name);
  model.terms_[wire->port_id] = id;
  scalarTermBuilder.setName(termName);
  scalarTermBuilder.setDirection(YosysToCapnPDirection(wire));
  std::cerr << "Dumping scalar term: " << termName << std::endl;
}

void dumpBusTerm(
  DBInterface::LibraryInterface::DesignInterface::Term::Builder& term,
  const RTLIL::Wire* wire,
  size_t id,
  Model& model) {
  auto busTermBuilder = term.initBusTerm();
  auto termName = getName(wire->name);
  model.terms_[wire->port_id] = id;
  busTermBuilder.setId(id);
  busTermBuilder.setName(termName);
  busTermBuilder.setDirection(YosysToCapnPDirection(wire));
  auto start = wire->start_offset;
  auto end = wire->start_offset + wire->width - 1;
  auto msb = (wire->upto) ? start : end;
  auto lsb = (wire->upto) ? end : start;
  busTermBuilder.setMsb(msb);
  busTermBuilder.setLsb(lsb);
  std::cerr << "Dumping bus: " << termName << "[" << msb << "," << lsb << "]" << std::endl;
}

void dumpPorts(
  DBInterface::LibraryInterface::DesignInterface::Builder& design,
  RTLIL::Module* module,
  Model& model) {
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
        dumpScalarTerm(term, wire, portID, model);
      } else {
        dumpBusTerm(term, wire, portID, model);
      }
      ++portID;
    }
  }
}

void collectBusNet(
  const RTLIL::Wire* wire,
  const Terms& terms,
  Nets& nets) {
  auto start = wire->start_offset;
  auto end = wire->start_offset + wire->width - 1;
  auto msb = (wire->upto) ? start : end;
  auto lsb = (wire->upto) ? end : start;
#if SNL_YOSYS_PLUGIN_DEBUG
  std::cerr << "Collect bus net: " << getName(wire->name) << "[" << msb << "," << lsb << "]" << std::endl;
#endif
  auto insertion = nets.insert(std::make_pair(wire, Net(msb, lsb)));
  assert(insertion.second);
  if (wire->port_id != 0) {
    auto it = insertion.first;
    auto& net = it->second;
    auto portIt = terms.find(wire->port_id);
    size_t id = 0;
    int incr = (wire->upto) ? +1 : -1;
    for (auto bit=msb; (wire->upto)?bit<=lsb:bit>=lsb; bit+=incr) {
      net.bits_[id].components_.emplace_back(Component(portIt->second, true, bit));
#if SNL_YOSYS_PLUGIN_DEBUG
      std::cerr << "Connect bus term bit: " << getName(wire->name) << "[" << bit << "], @" << id << std::endl;
#endif
      ++id;
    }
  }
}

void collectScalarNet(
  const RTLIL::Wire* wire,
  const Terms& terms,
  Nets& nets) {
  auto insertion = nets.insert(std::make_pair(wire, Net()));
  assert(insertion.second);
  //collect port
  if (wire->port_id != 0) {
    auto it = insertion.first;
    auto& net = it->second;
    auto portIt = terms.find(wire->port_id);
    assert(portIt != terms.end());
    assert(net.bits_.size() == 1);
    net.bits_.back().components_.emplace_back(Component(portIt->second, false, 0));
  }
}

void collectWire(
  const RTLIL::Wire* wire,
  const Terms& terms,
  Nets& nets) {
  //Will construct all nets and also collect terminals
#if SNL_YOSYS_PLUGIN_DEBUG
  YosysDebug::print(wire, 0);
#endif
  if (wire->width != 1) {
    collectBusNet(wire, terms, nets);
  } else {
    collectScalarNet(wire, terms, nets);
  }
}

using Modules = std::set<RTLIL::Module*>;

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
    auto name = getName(primitiveModule->name); 
    std::cerr << "Dumping primitive: " << name << std::endl;
    if (models.find(name) != models.end()) {
      log_error("Model %s already in map", log_id(name));
    }
    auto it = models.insert(std::pair<std::string, Model>(name, Model(0, primitiveID)));
    assert(it.second);
    auto& model = it.first->second;
    primitive.setName(name);
    primitive.setType(DBInterface::LibraryInterface::DesignType::PRIMITIVE);
    //dump parameters
    dumpParameters(primitive, primitiveModule);
    //dump ports
    dumpPorts(primitive, primitiveModule, model);
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
    auto name = getName(userModule->name);
    std::cerr << "Dumping module: " << name << std::endl;
    design.setName(name);
    auto it = models.insert(std::pair<std::string, Model>(name, Model(1, designID)));
    assert(it.second);
    auto& model = it.first->second;

    //collect ports
    dumpPorts(design, userModule, model);

    ++designID;
  }

  if (topDesignID != -1) {
    auto designReferenceBuilder = db.initTopDesignReference();
    designReferenceBuilder.setDbID(1);
    designReferenceBuilder.setLibraryID(1);
    designReferenceBuilder.setDesignID(topDesignID);
  }

  int fd = open(
    interfacePath.c_str(),
    O_CREAT | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  writePackedMessageToFd(fd, message);
  close(fd);
}

void dumpImplementation(
  const RTLIL::Design* ydesign,
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
  for (auto& userModule: userModules) {
    auto mit = models.find(getName(userModule->name));
    assert(mit != models.end());
    const Terms& terms = mit->second.terms_;
    auto design = designs[designID]; 
    design.setId(designID++);
    //collect all nets: bus and scalar
    //collect terminals at the same time
    Nets nets;
    for (auto wire: userModule->wires()) {
      collectWire(wire, terms, nets); 
    }
    size_t instancesSize = userModule->cells().size();
    if (instancesSize > 0) {
      auto instances = design.initInstances(instancesSize);
      size_t instanceID = 0;
      size_t autoNameID = 0;
      for (auto cell: userModule->cells()) {
        std::string name;
        //rename instance
        auto yosysName = cell->name.c_str();
        if (*yosysName == '$') {
          name = '_' + std::to_string(autoNameID++) + '_';
        } else {
          name = getName(cell->name);
        }
        //
#if SNL_YOSYS_PLUGIN_DEBUG
        std::cerr << "Dumping cell/instance implementation: " << getName(cell->name) << std::endl;
        std::cerr << "Renamed to: " << name << std::endl;
#endif
        YosysDebug::print(cell, 0);
        auto instance = instances[instanceID];
        instance.setId(instanceID);
        instance.setName(name);
        auto modelReferenceBuilder = instance.initModelReference();
        modelReferenceBuilder.setDbID(1);
        auto modelIt = models.find(getName(cell->type));
        if (modelIt == models.end()) {
          log_error("Model type %s not found in map for cell %s", cell->type.c_str(), cell->name.c_str());
        }
        const auto& model = modelIt->second;
        auto libraryID = model.libraryID_;
        auto modelID = model.designID_;
        modelReferenceBuilder.setLibraryID(libraryID);
        modelReferenceBuilder.setDesignID(modelID);
        dumpInstanceParameters(instance, cell);
        auto module = ydesign->module(cell->type);

        for (auto& conn: cell->connections()) {
#if SNL_YOSYS_PLUGIN_DEBUG
          YosysDebug::print(conn.first, conn.second, 0);
#endif
          //Find net
          auto ss = conn.second;
          if (ss.size() != 1) {
            //FIXME !!
            continue;
          }
          assert(ss.size() == 1);
          auto bit = ss.bits()[0];
          auto w = bit.wire;
          if (not w) {
            //FIXME: constants
            continue;
          }
          auto offset = bit.offset;
          //Find w in map
          auto nit = nets.find(w);
          assert(nit != nets.end());
#if SNL_YOSYS_PLUGIN_DEBUG
          std::cerr << "Found: " << std::endl;
          YosysDebug::print(w, 0);
#endif
          auto& n = nit->second;
          assert(n.bits_.size() == w->width);
          auto pw = module->wire(conn.first);
          assert(pw);

          //Find inst term
          const auto& terms = model.terms_;
          auto it = terms.find(pw->port_id);
          assert(it != terms.end());
          auto tid = it->second;
          if (n.bits_.size() > 1) {
            n.bits_[offset].components_.emplace_back(Component(instanceID, tid, true, offset));
          } else {
            n.bits_.back().components_.emplace_back(Component(instanceID, tid, false, 0));
          }
          std::cerr << "tid: " << tid << std::endl;
			  }
        ++instanceID;
      }

      if (not nets.empty()) {
        auto dumpNets = design.initNets(nets.size());
        size_t netID = 0;
        size_t autoNameID = 0;
        for (auto& [wire, net]: nets) {
          std::string name;
          //rename net or name net
          auto yosysName = wire->name.c_str();
          if (*yosysName == '$') {
            name = '_' + std::to_string(autoNameID++) + '_';
          } else {
            name = getName(wire->name);
          }
          //
          auto dumpNet = dumpNets[netID];
          if (net.isBus_) {
            dumpBusNet(dumpNet, name, net, netID);
          } else {
            dumpScalarNet(dumpNet, name, net, netID);
          }
          ++netID;
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
    for (auto module: design->modules()) {
      if (module->get_blackbox_attribute()) {
        continue;
      }
      userModules.insert(module);
      for (auto cell: module->cells()) {
        auto model = design->module(cell->type); 
				if (model->get_blackbox_attribute()) {
          primitiveModules.insert(model);
        }
      }
    }
    
    Models models;
    dumpInterface(primitiveModules, userModules, dir/"db_interface.snl", models);
    dumpImplementation(design, userModules, dir/"db_implementation.snl", models);
  }

	void help() override
	{
		log("\n");
		log("    snl_backend [options]\n");
		log("\n");
	}

} SNLBackend;

PRIVATE_NAMESPACE_END
