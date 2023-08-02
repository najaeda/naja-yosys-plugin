#ifndef __YOSYS_DEBUG_H_
#define __YOSYS_DEBUG_H_

#include "kernel/yosys.h"

#include <iostream>

class YosysDebug {
  public:
    static void print(const Yosys::RTLIL::Wire* wire, size_t indent, std::ostream& stream = std::cerr);
    static void print(const Yosys::RTLIL::Cell* cell, size_t indent, std::ostream& stream = std::cerr);
    static void print(const Yosys::RTLIL::SigBit& bit, size_t indent, std::ostream& stream = std::cerr);
    static void printParameter(const Yosys::RTLIL::IdString& id, const Yosys::RTLIL::Const& v,
        size_t indent, std::ostream& stream = std::cerr);
    static void print(
        const Yosys::RTLIL::IdString& id, const Yosys::RTLIL::SigSpec& spec,
        size_t indent, std::ostream& stream = std::cerr);
};

#endif /* __YOSYS_DEBUG_H_ */
