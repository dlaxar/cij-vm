#include <jit/Operands.hpp>

#include <iostream>

namespace am2017s::jit {
	std::ostream& operator<<(std::ostream& os, RegOp const& reg) {
		return os << "%" << std::to_string(reg);
	}

	std::ostream& operator<<(std::ostream& os, MemOp const& obj) {
		return os << "[" << obj.base << " + " << obj.index << " * " << std::to_string(obj.scale) << " + " << std::to_string(obj.offset) << "]";
	}
}