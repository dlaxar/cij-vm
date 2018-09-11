#pragma once

#include <jit/lifetime/LifetimeAnalyzer.hpp>
#include <jit/CodeBuilder.hpp>
#include <jit/allocator/register/StackAllocator.hpp>

namespace am2017s { namespace jit {

struct SpillMovOp {
	RegMemOp first;
	RegMemOp second;
	OperandSize size;
};

struct StackSpillMovOp {
	RegMemOp source;
	allocator::StackSlot target;
	OperandSize size;
};

class MachineCompiler {
private:
	std::vector<jit::Block> const& blocks;
	std::vector<Interval> const& intervals;
	allocator::StackAllocator stack;

	std::map<lir::vr, bytecode::Type> const& vrTypes;
	std::vector<StackSpillMovOp> const& stackFrameSpills;

public:
	MachineCompiler(std::vector <jit::Block> const& _blocks,
			        std::vector<Interval> const& _intervals,
			        allocator::StackAllocator const& _stack,
			        std::map<lir::vr, bytecode::Type> const& vrTypes,
			        std::vector<StackSpillMovOp> const& stackFrameSpills);

	void run();

	const Interval& intervalFor(u16 id, lir::vr vr);

	CodeBuilder builder;

	std::map<u16, std::map<u16, std::vector<SpillMovOp>>>
	orderEdgeInstructions(std::map<u16, std::map<u16, std::vector<SpillMovOp>>> edgeInstructions);

	void insertEdgeInstructions(
			std::map<u16, std::map<u16, std::vector<SpillMovOp>>>& edgeInstructions,
			std::map<u16, std::map<u16, std::vector<SpillMovOp>>>& sortedInstructions,
			u16 predecessor,
			u16 successor);

	std::vector<SpillMovOp> topologicallySort(std::vector<SpillMovOp> vector);

	RegMemOp operandFor(u16 instructionId, lir::vr vr);
};

}}


