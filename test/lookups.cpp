#include "lookups.hpp"

namespace am2017s { namespace tests {
	Value temp(am2017s::u16 idx)
	{
		return {ValueKind::TEMPORARY, idx};
	}

	Value local(u16 idx)
	{
		return {ValueKind::LOCAL, idx};
	}

	ValueLocation reg(RegOp r)
	{
		return {LocationType::REGISTER, r};
	}

	ValueLocation stack(u16 idx)
	{
		ValueLocation l = {LocationType::STACK};
		l.stackSlotIdx = idx;
		return l;
	}

	ValueLocation param(u16 idx)
	{
		ValueLocation l = {LocationType::PARAM};
		l.stackSlotIdx = idx;
		return l;
	}

	SpillEntry spill(RegOp reg, u16 slot) {
		return {reg, slot};
	}
}}
