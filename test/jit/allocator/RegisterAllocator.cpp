#include <catch2/catch.hpp>

#include <jit/allocator/register/RegisterAllocator.hpp>
#include <jit/allocator/TwoRegArchitecture.hpp>
#include <bytecode.hpp>
#include <jit/machine/MachineCompiler.hpp>

using namespace am2017s::jit::allocator;

using namespace am2017s::jit;


//TEST_CASE("Splits interval", "[!hide]") {
//	const am2017s::bytecode::Function f{};
//	vector<Interval> liveIntervals;
//	Interval i1;
//	i1.addRange({0, 20});
////	i1.usedAt(20);
////	i1.usedAt(12);
////	i1.usedAt(0);
//	i1.vr = 1;
//	Interval i2;
//	i2.addRange({16, 20});
//	i2.addRange({0, 9});
////	i2.usedAt(20);
////	i2.usedAt(16);
////	i2.usedAt(9);
////	i2.usedAt(0);
//	i2.vr = 2;
//	Interval i3;
//	i3.addRange({10, 18});
////	i3.usedAt(18);
////	i3.usedAt(10);
//	i3.vr = 3;
//
//
//	liveIntervals.push_back(i1);
//	liveIntervals.push_back(i2);
//	liveIntervals.push_back(i3);
//
//	for(auto it : liveIntervals) {
//		it.toLifeline(std::cout, {});
//	}
//
//	auto allocator = am2017s::jit::allocator::RegisterAllocation<TwoRegArchitecture>(f, liveIntervals,
//	                                                                                 map<lir::vr, map<am2017s::u16, lir::Usage>>());
//	allocator.run();
//
//	REQUIRE(allocator.handled.size() == 5);
//}

TEST_CASE("topological sorting (connected)", "") {
	std::vector<std::pair<RegMemOp, RegMemOp>> input;
	input.push_back({RegMemOp(RegOp::RDX), RegMemOp(RegOp::RAX)});
	input.push_back({RegMemOp(RegOp::RAX), RegMemOp(RegOp::R8)});
	input.push_back({RegMemOp(RegOp::R8), RegMemOp(RegOp::R10)});


	auto machine = MachineCompiler({}, {});
	auto sort = machine.topologicallySort(input);
	REQUIRE(sort.size() == 3);
	REQUIRE(sort[0].first.reg() == R8);
	REQUIRE(sort[1].first.reg() == RAX);
	REQUIRE(sort[2].first.reg() == RDX);
}

TEST_CASE("topological sorting (independent)", "") {
	std::vector<std::pair<RegMemOp, RegMemOp>> input;
	input.push_back({RegMemOp(RegOp::RDX), RegMemOp(RegOp::RAX)});
	input.push_back({RegMemOp(RegOp::R8), RegMemOp(RegOp::R10)});

	auto machine = MachineCompiler({}, {});
	auto sort = machine.topologicallySort(input);

	REQUIRE(sort.size() == 2);
}

TEST_CASE("topological sorting (cyclic)", "") {
	std::vector<std::pair<RegMemOp, RegMemOp>> input;
	input.push_back({RegMemOp(RegOp::RDX), RegMemOp(RegOp::RAX)});
	input.push_back({RegMemOp(RegOp::RAX), RegMemOp(RegOp::RDX)});

	auto machine = MachineCompiler({}, {});
	auto sort = machine.topologicallySort(input);

	REQUIRE(sort.size() == 0);
}