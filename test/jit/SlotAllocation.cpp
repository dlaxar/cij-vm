#include <catch2/catch.hpp>

#include <jit/SlotAllocation.hpp>
#include "../generate.hpp"
#include "../lookups.hpp"

using namespace am2017s::tests;
using namespace generate;
using namespace am2017s::jit;

namespace Catch
{
	template <>
	struct StringMaker<ValueLocation>
	{
		static
		std::string convert(ValueLocation v)
		{
			switch(v.type)
			{
			case STACK:
				return "Stack: " + std::to_string(v.stackSlotIdx);
			case PARAM:
				return "Param: " + std::to_string(v.stackSlotIdx);
			case REGISTER:
				return "Register: " + std::to_string(v.reg);
			}
			return "";
		}
	};
}

static
bool operator==(ValueLocation const& x, ValueLocation const& y)
{
	if(x.type != y.type)
		return false;

	switch(x.type)
	{
	case STACK:
	case PARAM:
		return x.stackSlotIdx == y.stackSlotIdx;
	case REGISTER:
		return x.reg == y.reg;
	}
	return false;
}

TEST_CASE("Slot Allocation – no spilling", "[jit][registers]")
{
	auto func = buildFunction(
			{
					{array(long_())}, // reg
					{long_()}, // reg
			},
			{
					array(long_()),
					long_(),
					long_(),
					long_(),
			},
			{
					load(0, 0), // reg
					load(1, 1), // reg
					load_idx(2, 0, 1), // reg
					length(3, 0), // reg
					store_idx(0, 1, 3),
					return_(2),
			});

	RegisterAllocationInfo const& info = performRegisterAllocation(func, performLifetimeAnalysis(func));

	REQUIRE(info.stackFrameSize == 0);
	REQUIRE(info.spills.size() == 0);
	REQUIRE(info.callSiteSpfills.size() == 0);
	REQUIRE(info.allocations.size() == 6);
}

TEST_CASE("Slot Allocation – callee save spilling", "[jit][registers]")
{
	auto func = buildFunction(
			{
					{array(long_())}, // reg 1
					{array(long_())}, // reg 2
					{array(long_())}, // reg 3
					{array(long_())}, // reg 4
					{array(long_())}, // reg 5
			},
			{
					array(long_()),
					array(long_()),
					array(long_()),
					array(long_()),
					long_(),
					int_()
			},
			{
					load(0, 0), // reg - use param 1
					load(1, 0), // reg 6
					load(2, 0), // reg 7
					load(3, 0), // reg 8 – at this point we should have all the caller saved registers allocated
					store(1, 0), // use param 2
					store(2, 0), // use param 3
					store(3, 0), // use param 4
					store(4, 0), // use param 5
					store(5, 0), // use param 6
					store(6, 0), // use param 7
					load_idx(4, 1, 2), // reg - the life of %3 ends here however it spills once // todo we could not (probably) not spill this
					length(5, 0), // reg so %2 can actually reuse the slot of %1
					store_idx(0, 0, 5),
					return_(0),
			});

	RegisterAllocationInfo const& info = performRegisterAllocation(func, performLifetimeAnalysis(func));

	REQUIRE(info.stackFrameSize == 1);
	REQUIRE(info.spills.size() == 1);
	REQUIRE(info.callSiteSpfills.size() == 0);
	REQUIRE(info.allocations.size() == 5+6);
}

TEST_CASE("Slot Allocation – Exceeding args allocation", "[jit][registers]")
{
	auto func = buildFunction(
			{
					{array(long_())}, // reg 1
					{array(long_())}, // reg 2
					{array(long_())}, // reg 3
					{array(long_())}, // reg 4
					{array(long_())}, // reg 5
					{array(long_())}, // stack 0
					{array(long_())}, // stack 1
			},
			{
					array(long_()),
					array(long_()),
					array(long_()),
					long_(),
					int_()
			},
			{
					load(0, 0), // reg - use param 1
					load(1, 0), // reg 6
					load(2, 0), // reg 7
					load(3, 0), // reg 8 – at this point we should have all the caller saved registers allocated
					store(1, 0), // use param 2
					store(2, 0), // use param 3
					store(3, 0), // use param 4
					store(4, 0), // use param 5
					store(5, 0), // use param 6
					store(6, 0), // use param 7
					load_idx(4, 1, 2), // reg - the life of %3 ends here however it spills once // todo we could not (probably) not spill this
					length(5, 0), // reg so %2 can actually reuse the slot of %1
					store_idx(0, 0, 5),
					return_(0),
			});

	RegisterAllocationInfo info = performRegisterAllocation(func, performLifetimeAnalysis(func));

	REQUIRE(info.stackFrameSize == 1);
	REQUIRE(info.spills.size() == 1);
	REQUIRE(info.callSiteSpfills.size() == 0);
	REQUIRE(info.allocations.size() == 7+5);
	REQUIRE(info.allocations[local(5)] == param(0));
	REQUIRE(info.allocations[local(6)] == param(1));
}

TEST_CASE("Slot Allocation – Call Spilling", "[jit][registers]")
{
	auto func = buildFunction(
			{
					{array(long_())}, // reg 1
			},
			{
					array(long_()),
					int_(),
			},
			{
					load(0, 0), // reg - use param 1
					call(0, 1, {0}),
					return_(1)
			});

	RegisterAllocationInfo info = performRegisterAllocation(func, performLifetimeAnalysis(func));

	REQUIRE(info.stackFrameSize == 2);
	REQUIRE(info.spills.size() == 0);
	REQUIRE(info.allocations.size() == 3);
	REQUIRE(info.callSiteSpfills.size() == 1);
	REQUIRE(info.callSiteSpfills[1].size() == 2);
	REQUIRE(info.callSiteSpfills[1][0].reg == JitCC::PARAM_REGS[0]);
	REQUIRE(info.callSiteSpfills[1][1].reg == JitCC::CALLER_SAVED[1]);
	REQUIRE(info.callSiteSpfills[1][0].stackSlotIdx != info.callSiteSpfills[1][1].stackSlotIdx);
}

TEST_CASE("Slot Allocation – local spfills", "[jit][registers]")
{
	auto func = buildFunction(
			{
			},
			{
					long_()
			},
			{
					const_(0, long_(), 0),
					return_(0)
			});

	RegisterAllocationInfo info = performRegisterAllocation(func, performLifetimeAnalysis(func));

	REQUIRE(info.stackFrameSize == 0);
	REQUIRE(info.spills.size() == 0);
	REQUIRE(info.instructionRegisters[0].size() == 0); // because we can directly mov to destination
	REQUIRE(info.callSiteSpfills[0].size() == 0);
}

TEST_CASE("Slot Allocation – local spfills with stack usage", "[jit][registers]")
{
	auto func = buildFunction(
			{
					{long_()}, // cpu 1
			},
			{
					long_(), // cpu 2
					long_(), // cpu 3
					long_(), // cpu 4
					long_(), // cpu 5
					long_(), // cpu 6
					long_(), // cpu 7
					long_(), // cpu 8
					long_(), // cpu 9
					long_(), // cpu 10
					long_(), // cpu 11
					long_(), // cpu 12
					long_(), // cpu 13

					long_(), // stack 1
			},
			{
					load(0, 0),
					load(1, 0),
					load(2, 0),
					load(3, 0),
					load(4, 0),
					load(5, 0),
					load(6, 0),
					load(7, 0),
					load(8, 0),
					load(9, 0),
					load(10, 0),
					load(11, 0),

					const_(12, long_(), 0),

					store(0, 0),
					store(0, 1),
					store(0, 2),
					store(0, 3),
					store(0, 4),
					store(0, 5),
					store(0, 6),
					store(0, 7),
					store(0, 8),
					store(0, 9),
					store(0, 10),
					store(0, 11),
					return_(11)
			});

	RegisterAllocationInfo info = performRegisterAllocation(func, performLifetimeAnalysis(func));

	REQUIRE(info.stackFrameSize == 2 + 5); // 2 for the const operation and 5 callee save spills
	REQUIRE(info.spills.size() == 1);
	REQUIRE(info.spills[0].size() == 5); // all callee saved registers
	REQUIRE(info.instructionRegisters[12].size() == 1);
	REQUIRE(info.callSiteSpfills[12].size() == 1);
}

TEST_CASE("Slot Allocation – multiple local spfills with stack usage", "[jit][registers]")
{
	auto func = buildFunction(
			{
					{long_()}, // cpu 1
			},
			{
					long_(), // cpu 2
					long_(), // cpu 3
					long_(), // cpu 4
					long_(), // cpu 5
					long_(), // cpu 6
					long_(), // cpu 7
					long_(), // cpu 8
					long_(), // cpu 9
					long_(), // cpu 10
					long_(), // cpu 11
					long_(), // cpu 12
					long_(), // cpu 13

					long_(), // stack 1
					long_(), // stack 2
			},
			{
					load(0, 0),
					load(1, 0),
					load(2, 0),
					load(3, 0),
					load(4, 0),
					load(5, 0),
					load(6, 0),
					load(7, 0),
					load(8, 0),
					load(9, 0),
					load(10, 0),
					load(11, 0),

					const_(12, long_(), 0),
					const_(13, long_(), 0),

					store(0, 0),
					store(0, 1),
					store(0, 2),
					store(0, 3),
					store(0, 4),
					store(0, 5),
					store(0, 6),
					store(0, 7),
					store(0, 8),
					store(0, 9),
					store(0, 10),
					store(0, 11),
					store(0, 12),
					store(0, 13),
					return_(0)
			});

	RegisterAllocationInfo info = performRegisterAllocation(func, performLifetimeAnalysis(func));

	REQUIRE(info.stackFrameSize == 2 + 1 + 5); // 1 for each const operation temporary + 1 spilling and 5 callee save spills
	REQUIRE(info.spills.size() == 1);
	REQUIRE(info.spills[0].size() == 5); // all callee saved registers
	REQUIRE(info.instructionRegisters[12].size() == 1);
	REQUIRE(info.instructionRegisters[13].size() == 1);
	REQUIRE(info.callSiteSpfills[12].size() == 1);
	REQUIRE(info.callSiteSpfills[13].size() == 1);
}

TEST_CASE("Slot Allocation – no spfills because destination is a free register", "[jit][registers]")
{
	auto func = buildFunction(
			{
					{long_()}, // cpu 1
			},
			{
					long_(), // cpu 2
					long_(), // cpu 3
					long_(), // cpu 4
					long_(), // cpu 5
					long_(), // cpu 6
					long_(), // cpu 7
					long_(), // cpu 8
					long_(), // cpu 9
					long_(), // cpu 10
					long_(), // cpu 11
					long_(), // cpu 12
					long_(), // cpu 13
			},
			{
					load(0, 0),
					load(1, 0),
					load(2, 0),
					load(3, 0),
					load(4, 0),
					load(5, 0),
					load(6, 0),
					load(7, 0),
					load(8, 0),
					load(9, 0),
					load(10, 0),

					const_(11, long_(), 0),

					store(0, 0),
					store(0, 1),
					store(0, 2),
					store(0, 3),
					store(0, 4),
					store(0, 5),
					store(0, 6),
					store(0, 7),
					store(0, 8),
					store(0, 9),
					store(0, 10),
					store(0, 11),
					return_(0)
			});

	RegisterAllocationInfo info = performRegisterAllocation(func, performLifetimeAnalysis(func));

	REQUIRE(info.stackFrameSize == 5); // 5 callee save spills
	REQUIRE(info.spills.size() == 1);
	REQUIRE(info.spills[0].size() == 5); // all callee saved registers
	REQUIRE(info.instructionRegisters[11].size() == 0);
	REQUIRE(info.callSiteSpfills[11].size() == 0);
}
