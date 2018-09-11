#pragma once

#include <jit/LifetimeAnalysis.hpp>
#include <jit/SlotAllocation.hpp>

using namespace am2017s::jit;

namespace am2017s { namespace tests {
	Value temp(u16 idx);

	Value local(u16 idx);

	ValueLocation reg(RegOp r);

	ValueLocation stack(u16 idx);

	ValueLocation param(u16 idx);

	SpillEntry spill(RegOp reg, u16 slot);
}}
