#pragma once

#include <types.hpp>
#include <jit/Operands.hpp>

namespace am2017s { namespace jit { namespace allocator {

enum StackType {
	PARAMETER, // input for the current function
	ARGUMENT,  // input for a function called from the current function
	SCRATCH
};

struct StackSlot {
	StackType type;
	OperandSize size;
	u16 index;

	friend std::ostream& operator<<(std::ostream& os, const StackSlot& obj) {
		return os << ((obj.type == ARGUMENT) ? "arg " : "    ") << obj.index << " (" << obj.size << "B)";
	}
};

class StackAllocator {
private:
	u16 bytesArguments = 0;
	u16 bytesScratch = 0;
	u16 padding = 0;

	bool frozen = false;

public:
	StackSlot reserveArgument(u16 index);
	StackSlot reserveScratch(OperandSize size);

	void freeze();

	u16 getStackSize() const;
	MemOp getAddressing(StackSlot const& slot) const;
};

}}}