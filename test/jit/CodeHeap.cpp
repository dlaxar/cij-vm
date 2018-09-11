#include <vector>

#include <catch2/catch.hpp>

#include <jit/CodeHeap.hpp>

using namespace am2017s;
using namespace jit;

TEST_CASE("CodeHeap manages its bitmap correctly", "[jit]")
{
	CodeHeap heap;
	std::vector<CodeSegment> allocations;

	for(int i = 0; i != 3; ++i)
		allocations.emplace_back(heap.allocate(1));

	REQUIRE(heap._bitmap[0] == true);
	REQUIRE(heap._bitmap[1] == true);
	REQUIRE(heap._bitmap[2] == true);

	allocations.erase(allocations.begin() + 1);
	REQUIRE(heap._bitmap[0] == true);
	REQUIRE(heap._bitmap[1] == false);
	REQUIRE(heap._bitmap[2] == true);

	allocations.clear();
	REQUIRE(heap._bitmap[0] == false);
	REQUIRE(heap._bitmap[1] == false);
	REQUIRE(heap._bitmap[2] == false);
}
