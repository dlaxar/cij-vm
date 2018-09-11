#pragma once

#include <bytecode.hpp>
#include <jit/lifetime/LifetimeAnalyzer.hpp>
#include <jit/allocator/register/StackAllocator.hpp>
#include <jit/architecture/Architecture.hpp>
#include <jit/machine/MachineCompiler.hpp>

namespace am2017s { namespace jit { namespace allocator {

template <class Architecture>
class RegisterAllocation {

private:
	bytecode::Function const& function;
	vector<Interval>& lifespans;
	std::vector<Interval> active, inactive, fixed;
	lir::UsageMap const& usages;
	std::map<RegOp, lir::vr> fixedToVR;
	std::map<XMMOp, lir::vr> fixedXMMToVR;
	std::map<u16, lir::vr> overflowArgToVR;
	std::map<lir::vr, bytecode::Type> const& vrTypes;
	std::set<std::set<lir::vr>> const& hintSame;

	std::set<RegOp> usedRegisters;



public:
	RegisterAllocation(bytecode::Function const& _function,
		                   vector<Interval>& _lifespans,
		                   lir::UsageMap& _usages,
		                   std::map<RegOp, lir::vr> const& fixedToVR,
		                   std::map<XMMOp, lir::vr> const& fixedXMMToVR,
		                   std::map<u16, lir::vr> const& _overflowArgToVR,
		                   std::map<lir::vr, bytecode::Type> const& vrTypes,
		               std::set<std::set<lir::vr>> const& hintSame);
	int run();

	std::vector<Interval> handled;
	std::vector<StackSpillMovOp> stackFrameSpills;
	StackAllocator stackAllocator;

private:
	void linearScan();

	template<typename RegType>
	RegType chooseFreeRegister(Interval const& current, std::map<RegType, u16> freeUntilPos);

	template<typename RegType>
	bool tryAllocateFreeRegister(Interval& current,
	                             priority_queue<Interval, deque<Interval>, greater<>>& unhandled);

	template<typename RegType>
	void allocateBlockedRegister(Interval& current,
	                             priority_queue<Interval, deque<Interval>, greater<>>& unhandled);

	template<typename RegType>
	void handleIntervalBeingPushedOffRegister(Interval& current,
	                                          priority_queue<Interval, deque<Interval>, greater<>>& unhandled,
	                                          RegType reg);

	std::map<RegOp, Interval> fixedToInterval;
	std::map<XMMOp, Interval> fixedXMMToInterval;
};

#ifdef __amd64__
template class RegisterAllocation<AMD64>;
#endif

#ifdef TESTING
#include <jit/allocator/TwoRegArchitecture.hpp>
template class RegisterAllocation<TwoRegArchitecture>;
#endif

}}}


