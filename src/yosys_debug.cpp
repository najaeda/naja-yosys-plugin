#include "yosys_debug.h"

USING_YOSYS_NAMESPACE

void YosysDebug::print(const RTLIL::Wire* w, size_t indent, std::ostream& stream) {
  stream << std::string(indent, ' ') << "Wire:" << std::endl;
	stream << std::string(indent+2, ' ') << w->module->name.c_str() << std::endl;
	stream << std::string(indent+2, ' ') << w->name.c_str() << std::endl;
	stream << std::string(indent+2, ' ')
		<< "width:" << w->width << ", start_offset:" << w->start_offset << ", port_id:" << w->port_id
		<< std::endl;
	stream << std::string(indent+2, ' ')
		<< "port_input:" << w->port_output << ", port_output:" << w->port_output << ", upto:" << w->upto
		<< ", is_signed:" << w->is_signed
		<< std::endl;
}

void YosysDebug::print(const RTLIL::Cell* c, size_t indent, std::ostream& stream) {
  stream << std::string(indent, ' ') << "Cell:" << std::endl;
	stream << std::string(indent+2, ' ') << c->module->name.c_str() << std::endl;
	stream << std::string(indent+2, ' ') << c->name.c_str() << std::endl;
	stream << std::string(indent+2, ' ') << c->type.c_str() << std::endl;
	stream << std::string(indent+2, ' ') << "connections: " << c->connections().size() << std::endl;
	stream << std::string(indent+2, ' ') << "parameters: " << c->parameters.size() << std::endl;
  if (not c->parameters.empty()) {
    for (auto parameter: c->parameters) {
      YosysDebug::printParameter(parameter.first, parameter.second, 2, stream);
    }
  }
}

void YosysDebug::printParameter(const RTLIL::IdString& s, const RTLIL::Const& v, size_t indent, std::ostream& stream) {
  stream << std::string(indent, ' ') << "Parameter: ";
  stream << s.str() << ", " << v.as_string() << std::endl;
}

void YosysDebug::print(
  const RTLIL::IdString& id, const RTLIL::SigSpec& s,
  size_t indent, std::ostream& stream) {
  stream << std::string(indent, ' ') << "Connection:" << std::endl;
	stream << std::string(indent+2, ' ') << "id: " << id.c_str() << std::endl;
	stream << std::string(indent+2, ' ') << "size: " << s.size() << std::endl;
	stream << std::string(indent+2, ' ') << "chunks: " << s.chunks().size() << std::endl;
  if (not s.chunks().empty()) {
    for (auto chunk: s.chunks()) {
      YosysDebug::print(chunk, indent+2, stream);
    }
  }
	stream << std::string(indent+2, ' ') << "bits: " << s.bits().size() << std::endl;
  if (not s.bits().empty()) {
    for (auto bit: s.bits()) {
      YosysDebug::print(bit, indent+2, stream);
    }
  }
}

void YosysDebug::print(const RTLIL::State& s, size_t indent, std::ostream& stream) {
  stream << std::string(indent, ' ') << " state: ";
  switch (s) {
    case RTLIL::State::S0: stream << "S0"; break;
    case RTLIL::State::S1: stream << "S1"; break;
    case RTLIL::State::Sx: stream << "Sx"; break;
    case RTLIL::State::Sz: stream << "Sz"; break;
    case RTLIL::State::Sa: stream << "Sa"; break;
    case RTLIL::State::Sm: stream << "Sm"; break;
	}
  stream << std::endl;
}

void YosysDebug::print(const RTLIL::SigBit& b, size_t indent, std::ostream& stream) {
  stream << std::string(indent, ' ') << "Bit:";
  if (b.wire) {
    stream << " wire: " << b.wire->name.c_str() << ", offset: " << b.offset << std::endl;
  } else { 
    stream << " data: " << std::endl;
    YosysDebug::print(b.data, indent+2, stream);
  }
}

void YosysDebug::print(const RTLIL::SigChunk& c, size_t indent, std::ostream& stream) {
  stream << std::string(indent, ' ') << "Chunk:";
  if (c.wire) {
    stream << " wire: " << c.wire->name.c_str() << ", offset: " << c.offset << std::endl;
  } else { 
    stream << " data: ";
    bool first = true;
    for (auto d: c.data) {
      if (not first) {
        stream << ", ";
      }
      stream << d;
      first = false;
    }
    stream << std::endl;
  }
}
