#include <catch2/catch.hpp>

#include <jit/LifetimeAnalysis.hpp>

#include "StringMakerExtensions.hpp"
#include "../generate.hpp"
#include "../lookups.hpp"

using namespace am2017s;
using namespace bytecode;
using namespace jit;
using namespace am2017s::tests;
using namespace generate;

namespace Catch
{
	template <>
	struct StringMaker<Value>
	{
		static
		std::string convert(Value v)
		{
			return (v.kind == ValueKind::LOCAL ? "L" : "T") + std::string(":") + std::to_string(v.index);
		}
	};
}

TEST_CASE("LifetimeAnalysis - basic", "[jit]")
{
	/*
	 * (long $0, long $1) -> long
	 * {
	 *      (long) %0 = load $0    -- first: $0, %0; last: $0;
	 *      (long) %1 = load $1    -- first: $1, %1; last: ;
	 *      (long) %2 = const 10   -- first: %2;     last: ;
	 *      (long) %3 = neg %2     -- first: %3;     last: %2;
	 *             store $1, %3    -- first: ;       last: %3, $1;
	 *      (long) %4 = add %0, %1 -- first: %4;     last: %0, %1;
	 *             return %4       -- first: ;       last: %4;
	 * }
	 */
	auto func = buildFunction(
		{
			{long_()},
			{long_()},
		},
		{
			long_(),
		        long_(),
		        long_(),
		        long_(),
		        long_(),
		},
		{
			load(0, 0),
			load(1, 1),
			const_(2, long_(), 10),
			unary(Opcode::NEG, 3, 2),
			store(1, 3),
			binary(Opcode::ADD, 4, 0, 1),
			return_(4),
		});

	auto lifetimes = performLifetimeAnalysis(func);

	std::map<u16, std::set<Value>> expectedFirstUses =
		{
			{0, {local(0), temp(0)}},
			{1, {local(1), temp(1)}},
			{2, {temp(2)}},
			{3, {temp(3)}},
			// 4, {}
			{5, {temp(4)}}
		        // 6, {}
		};

	std::map<u16, std::set<Value>> expectedLastUses =
		{
			{0, {local(0)}},
			// 1, {}
			// 2, {}
			{3, {temp(2)}},
			{4, {temp(3), local(1)}},
			{5, {temp(0), temp(1)}},
			{6, {temp(4)}},
		};

	REQUIRE(lifetimes.firstUsesByInstruction == expectedFirstUses);
	REQUIRE(lifetimes.lastUsesByInstruction == expectedLastUses);
}

TEST_CASE("LifetimeAnalysis - arrays", "[jit]")
{
	/*
	 * (int[] $0, int $1) -> int
	 * {
	 *      (int[]) %0 = load $0         -- first: $0, %0; last: $0;
	 *      (int)   %1 = load $1         -- first: $1, %1; last: $1;
	 *      (int)   %2 = load_idx %0, %1 -- first: %2;     last: ;
	 *      (int)   %3 = length %0       -- first: %3;     last: ;
	 *              store_idx %0, %1, %3 -- first: ;       last: %0, %1, %3;
	 *              return %2            -- first: ;       last: %2;
	 * }
	 */
	auto func = buildFunction(
		{
			{array(long_())},
			{long_()},
		},
		{
			array(long_()),
		        long_(),
		        long_(),
		        long_(),
		},
		{
			load(0, 0),
		        load(1, 1),
		        load_idx(2, 0, 1),
		        length(3, 0),
		        store_idx(0, 1, 3),
		        return_(2),
		});

	auto lifetimes = performLifetimeAnalysis(func);

	std::map<u16, std::set<Value>> expectedFirstUses =
		{
			{0, {local(0), temp(0)}},
			{1, {local(1), temp(1)}},
			{2, {temp(2)}},
			{3, {temp(3)}},
		        // 4, {}
			// 5, {}
		};

	std::map<u16, std::set<Value>> expectedLastUses =
		{
			{0, {local(0)}},
			{1, {local(1)}},
			// 2, {}
			// 3, {}
			{4, {temp(0), temp(1), temp(3)}},
			{5, {temp(2)}},
		};

	REQUIRE(lifetimes.firstUsesByInstruction == expectedFirstUses);
	REQUIRE(lifetimes.lastUsesByInstruction == expectedLastUses);
}

TEST_CASE("LifetimeAnalysis - calls", "[jit]")
{
	/*
	 * (long $0, long $1) -> void
	 * {
	 *      (long) %0 = load $0       -- first: $0, %0; last: $0;
	 *      (long) %1 = load $1       -- first: $1, %1; last: $1;
	 *      (long) %2 = call#0 %0, %1 -- first: %2;     last: %2;
	 *             call_special#0 %0  -- first: ;       last: ;
	 *             call_void#1 %0, %1 -- first: ;       last: %0, %1;
	 * }
	 */
	auto func = buildFunction(
		{
			{long_()},
			{long_()},
		},
		{
			long_(),
		        long_(),
		        long_(),
		},
		{
			load(0, 0),
		        load(1, 1),
		        call(0, 2, {0, 1}),
		        call_special(0, {0}),
		        call_void(1, {0, 1}),
		});

	auto lifetimes = performLifetimeAnalysis(func);

	std::map<u16, std::set<Value>> expectedFirstUses =
		{
			{0, {local(0), temp(0)}},
			{1, {local(1), temp(1)}},
			{2, {temp(2)}},
			// 3, {}
			// 4, {}
		};

	std::map<u16, std::set<Value>> expectedLastUses =
		{
			{0, {local(0)}},
			{1, {local(1)}},
			{2, {temp(2)}},
		        // 3, {}
			{4, {temp(0), temp(1)}},
		};

	REQUIRE(lifetimes.firstUsesByInstruction == expectedFirstUses);
	REQUIRE(lifetimes.lastUsesByInstruction == expectedLastUses);
}

TEST_CASE("LifetimeAnalysis - simple loop", "[jit]")
{
	/*
	 * (int $0) -> void
	 * {
	 *      (int)  %0 = const 10   -- first: %0; last: ;
	 *             store $0, %0    -- first: $0; last: %0;
	 *      (int)  %1 = const 0    -- first: %1; last: ;
	 *      (int)  %2 = const 1    -- first: %2; last: ;
	 *
	 * loop:
	 *      (int)  %3 = load $0    -- first: %3; last: ;
	 *      (bool) %4 = eq %3, %1  -- first: %4; last: ;
	 *             if %4 goto done -- first: ;   last: %4;
	 *
	 *             call_void#0     -- first: ;   last: ;
	 *
	 *      (int)  %5 = sub %3, %2 -- first: %5; last: %3;
	 *             store $0, %5    -- first: ;   last: %5;
	 *
	 *             goto loop       -- first: ;   last: $0, %1, %2;
	 *
	 * done:
	 * }
	 */
	auto func = buildFunction(
		{
			{int_()},
		},
		{
			int_(),
		        int_(),
		        int_(),
		        int_(),
		        bool_(),
		        int_(),
		},
		{
			const_(0, int_(), 10),
		        store(0, 0),
		        const_(1, int_(), 0),
		        const_(2, int_(), 1),

		        load(3, 0),
		        binary(Opcode::EQ, 4, 3, 1),
		        if_goto(4, 11),

		        call_void(0, {}),

		        binary(Opcode::SUB, 5, 3, 2),
		        store(0, 5),

		        goto_(4),
		});

	auto lifetimes = performLifetimeAnalysis(func);

	std::map<u16, std::set<Value>> expectedFirstUses =
		{
			{0, {temp(0)}},
			{1, {local(0)}},
			{2, {temp(1)}},
			{3, {temp(2)}},
			{4, {temp(3)}},
			{5, {temp(4)}},
			// 6, {}
			// 7, {}
			{8, {temp(5)}},
			// 9, {}
			// 10, {}
		};

	std::map<u16, std::set<Value>> expectedLastUses =
		{
			// 0, {}
			{1, {temp(0)}},
			// 2, {}
			// 3, {}
			// 4, {}
			// 5, {}
			{6, {temp(4)}},
			// 7, {}
			{8, {temp(3)}},
			{9, {temp(5)}},
			{10, {local(0), temp(1), temp(2)}},
		};

	REQUIRE(lifetimes.firstUsesByInstruction == expectedFirstUses);
	REQUIRE(lifetimes.lastUsesByInstruction == expectedLastUses);
}

TEST_CASE("LifetimeAnalysis - nested loops", "[jit]")
{
	/*
	 * (int $0) -> int
	 * {
	 *      (int)  %0 = const 42      -- first: %0;     last: ;
	 *      (int)  %1 = const 0       -- first: %1;     last: ;
	 *      (int)  %2 = const 1       -- first: %2;     last: ;
	 *      (int)  %3 = const 2       -- first: %3;     last: ;
	 *      (int)  %4 = const 3       -- first: %4;     last: ;
	 *      (int)  %5 = const 5       -- first: %5;     last: ;
	 *
	 * loop:
	 *      (int)  %6 = load $0       -- first: $0, %6; last: ;
	 *
	 * nested1:
	 *      (int)  %7 = add %6, %5    -- first: %7;     last: ;
	 *             store $0, %7       -- first: ;       last: ;
	 *
	 *      (int)  %8 = mod %7, %3    -- first: %8;     last: ;
	 *      (bool) %9 = eq %8, %1     -- first: %9;     last: %8;
	 *             if %9 goto nested1 -- first: ;       last: %6, %9;
	 *
	 * nested2:
	 *      (int)  %10 = sub %7, %4   -- first: %10;    last: ;
	 *             store $0, %10      -- first: ;       last: ;
	 *
	 *      (int)  %11 = mod %10, %5  -- first: %11;    last: ;
	 *      (bool) %12 = eq %11, %1   -- first: %12;    last: %11;
	 *             if %12 goto loop   -- first: ;       last: %12;
	 *
	 *      (int)  %13 = add %10, %2  -- first: %13;    last: %10;
	 *             store $0, %13      -- first: ;       last: ;
	 *
	 *      (bool) %14 = eq %13, %0   -- first: %14;    last: %13;
	 *             if %14 goto done   -- first: ;       last: %14;
	 *
	 *             goto nested2       -- first: ;       last: $0, %0, %1, %2, %3, %5, %7;
	 *
	 * done:
	 *             return %4          -- first: ;       last: %4;
	 * }
	 */
	auto func = buildFunction(
		{
			{int_()}
		},
		{
			int_(),
			int_(),
			int_(),
			int_(),
			int_(),
			int_(),
			int_(),
			int_(),
			int_(),
		        bool_(),
			int_(),
			int_(),
		        bool_(),
			int_(),
		        bool_(),
		},
		{
			const_(0, int_(), 42),
		        const_(1, int_(), 0),
		        const_(2, int_(), 1),
		        const_(3, int_(), 2),
		        const_(4, int_(), 3),
		        const_(5, int_(), 5),

		        load(6, 0),

		        binary(Opcode::ADD, 7, 6, 5),
		        store(0, 7),

		        binary(Opcode::MOD, 8, 7, 3),
		        binary(Opcode::EQ, 9, 8, 1),
		        if_goto(9, 7),

		        binary(Opcode::SUB, 10, 7, 4),
		        store(0, 10),

		        binary(Opcode::MOD, 11, 10, 5),
		        binary(Opcode::EQ, 12, 11, 1),
		        if_goto(12, 6),

		        binary(Opcode::ADD, 13, 10, 2),
		        store(0, 13),

		        binary(Opcode::EQ, 14, 13, 0),
		        if_goto(14, 22),

		        goto_(12),

		        return_(4),
		});

	auto lifetimes = performLifetimeAnalysis(func);

	std::map<u16, std::set<Value>> expectedFirstUses =
		{
			{0, {temp(0)}},
			{1, {temp(1)}},
			{2, {temp(2)}},
			{3, {temp(3)}},
			{4, {temp(4)}},
			{5, {temp(5)}},
			{6, {local(0), temp(6)}},
			{7, {temp(7)}},
		        // 8, {}
			{9, {temp(8)}},
			{10, {temp(9)}},
		        // 11, {}
			{12, {temp(10)}},
		        // 13, {}
			{14, {temp(11)}},
			{15, {temp(12)}},
		        // 16, {}
			{17, {temp(13)}},
		        // 18, {}
			{19, {temp(14)}},
		        // 20, {}
			// 21, {}
			// 22, {}
		};

	std::map<u16, std::set<Value>> expectedLastUses =
		{
			// 0, {}
			// 1, {}
			// 2, {}
			// 3, {}
			// 4, {}
			// 5, {}
			// 6, {}
			// 7, {}
			// 8, {}
			// 9, {}
			{10, {temp(8)}},
			{11, {temp(6), temp(9)}},
			// 12, {}
			// 13, {}
			// 14, {}
			{15, {temp(11)}},
			{16, {temp(3), temp(12)}},
			{17, {temp(10)}},
			// 18, {}
			{19, {temp(13)}},
			{20, {temp(14)}},
			{21, {local(0), temp(0), temp(1), temp(2), temp(5), temp(7)}},
			{22, {temp(4)}},
		};

	REQUIRE(lifetimes.firstUsesByInstruction == expectedFirstUses);
	REQUIRE(lifetimes.lastUsesByInstruction == expectedLastUses);
}

TEST_CASE("LifetimeAnalysis – param usages", "[jit]")
{
	auto func = buildFunction(
			{
					{array(long_())}, // reg 1
					{array(long_())}, // reg 2
					{array(long_())}, // reg 3
					{array(long_())}, // reg 4
					{array(long_())}, // reg 5
					{long_()}, // reg 6
					{long_()}, // reg 7 – at this point we should have all the caller saved registers allocated
			},
			{
					array(long_()),
					long_(),
					long_(),
					long_(),
			},
			{
					load(0, 0), // reg - use param 1
					store(1, 0), // use param 2
					store(2, 0), // use param 3
					store(3, 0), // use param 4
					store(4, 0), // use param 5
					store(5, 0), // use param 6
					store(6, 0), // use param 7
					/*7*/load_idx(1, 0, 1), // reg - the life of %1 ends here
					length(2, 0), // reg so %2 can actually reuse the slot of %1
					store_idx(0, 0, 2),
					return_(0),
			});

	auto lifetimes = performLifetimeAnalysis(func);

	std::map<u16, std::set<Value>> expectedFirstUses =
			{
					{0, {local(0), temp(0)}},
					{1, {local(1)}},
					{2, {local(2)}},
					{3, {local(3)}},
					{4, {local(4)}},
					{5, {local(5)}},
					{6, {local(6)}},
					{7, {temp(1)}},
					{8, {temp(2)}},
					// {9, {}},
			};

	std::map<u16, std::set<Value>> expectedLastUses =
			{
					{0, {local(0)}},
					{1, {local(1)}},
					{2, {local(2)}},
					{3, {local(3)}},
					{4, {local(4)}},
					{5, {local(5)}},
					{6, {local(6)}},
					{7, {temp(1)}},
					// {8, {}}
					{9, {temp(2)}},
					{10, {temp(0)}}
			};

	REQUIRE(lifetimes.firstUsesByInstruction == expectedFirstUses);
	REQUIRE(lifetimes.lastUsesByInstruction == expectedLastUses);
}
