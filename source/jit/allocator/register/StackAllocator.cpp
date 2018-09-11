#include <jit/allocator/register/StackAllocator.hpp>
#include <exception/StackModificationException.hpp>
#include "StackAllocator.hpp"

namespace am2017s { namespace jit { namespace allocator {

StackSlot StackAllocator::reserveArgument(am2017s::u16 index) {
	if(frozen) {
		throw StackModificationException();
	}
	bytesArguments += 8;
	return {ARGUMENT, QWORD, (u16)(index * 8)};
}

StackSlot StackAllocator::reserveScratch(OperandSize size) {
	if(frozen) {
		throw StackModificationException();
	}
	u16 startingPos = bytesScratch;
	bytesScratch += 8;
	return {SCRATCH, QWORD, startingPos};
}

void StackAllocator::freeze() {
	frozen = true;

	u16 intermediateSize = bytesScratch + bytesArguments;

	if(intermediateSize % 16 == 8) {
		padding = 0;
	} else if(intermediateSize % 16 < 8) {
		padding = (u16)(8 - intermediateSize % 16);
	} else /*(intermediateSize % 16 > 8)*/ {
		padding = (u16)(16 - (intermediateSize % 16 - 8));
	}
}

u16 StackAllocator::getStackSize() const {
	return bytesArguments + padding + bytesScratch;
}

MemOp StackAllocator::getAddressing(StackSlot const& slot) const {
	if(!frozen) {
		throw std::runtime_error("Cannot get addressing before stack has been frozen");
	}
	if(slot.type == ARGUMENT) {
		return MemOp(RSP, slot.index);
	} else if(slot.type == PARAMETER) {
		return MemOp(RSP, getStackSize() + 8 /* return address */ + slot.index);
	} else /* slot.type == SCRATCH */ {
		return MemOp(RSP, bytesArguments + padding + slot.index);
	}
}

}}}
