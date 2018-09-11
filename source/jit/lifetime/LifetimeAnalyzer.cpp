#include <set>
#include <vector>
#include <algorithm>
#include <log/Logger.hpp>
#include <jit/lifetime/LifetimeAnalyzer.hpp>

namespace am2017s { namespace jit {

namespace interval { namespace internal {

template<typename RegType>
RegType reg(Interval const& i) {
	throw std::runtime_error("unspecialized RegType for Interval::reg");
}

template<>
RegOp reg(Interval const& i) {
	return i._reg;
}

template<>
XMMOp reg(Interval const& i) {
	return i._xmm;
}

}}

using namespace bytecode;

LifetimeAnalyzer::LifetimeAnalyzer(const bytecode::Function& function, vector<Block> _blocks, u16 _lirCount)
		: _function(function), blocks(_blocks), lirCount(_lirCount) {}

static vector<Interval> unionIntervals(vector<Interval> vector) {
	std::vector<Interval> unionized;
	for(auto v : vector) {
		unionized.insert(unionized.end(), v);
	}

	return unionized;
}

vector<Interval> LifetimeAnalyzer::run()&& {

	std::vector<Interval> intervals(lirCount);

	u16 vrIndex = 0;
	for(auto& interval : intervals) {
		interval.vr = vrIndex++;
	}

	for(auto b = blocks.rbegin(); b != blocks.rend(); ++b) {
		std::set<lir::vr> live;
		for(auto blockIndex : b->blockInfo.successors) {
			Block& successor = blocks[blockIndex];

			// live = union of liveIn of all successors of b
			live.insert(successor.liveIn.begin(), successor.liveIn.end());


			// foreach phi function of successors of b
			for(auto instruction = successor.lirs.begin();
					instruction != successor.lirs.end();
					++instruction) {

				if(instruction->operation == lir::Operation::PHI) {
					live.insert(instruction->phi.inputOf(b->index));
				}

			}
		}

		// phi nodes and inputs of following blocks need to live the whole block
		// (phi nodes + inputs of the following blocks) = live
		for(auto operand : live) {
			intervals[operand].addRange({b->fromLIR(), b->toLIR()});
		}

		for(auto instruction = b->lirs.rbegin();
				instruction != b->lirs.rend();
				++instruction) {
			// for each output operand of operation of b

			if(instruction->operation == lir::Operation::PHI) {
				continue;
			}

			std::vector<lir::vr> output = instruction->dst();
			for(lir::vr dstIdx : output) {
				if(intervals[dstIdx].lifespans.empty()) {
					Logger::log(Topic::LIFE_LOG) << "Unused vr " << dstIdx << std::endl;
					intervals[dstIdx].addRange({-1, (u16)(b->lirs.rbegin()->id)});
				}

				intervals[dstIdx].lifespans.front().from = instruction->id;
				live.erase(dstIdx);
			}


			std::vector<lir::vr> inputOperands;
			if(instruction->operation != lir::Operation::PHI) {
				inputOperands = instruction->inputs();
			}

			// for each input operand of operation of b
			for(auto operand : inputOperands) {
				intervals[operand].addRange({b->fromLIR(), instruction->id});
				live.insert(operand);
			}

			for(auto clear : instruction->clears()) {
				intervals[clear].addRange({instruction->id, instruction->id});
			}

		}

		// for each phi node of b
		for(auto instruction = b->lirs.rbegin();
		    instruction != b->lirs.rend();
		    ++instruction) {

			if(instruction->operation != lir::Operation::PHI) {
				continue;
			}

			live.erase(instruction->phi.dst);

			intervals[instruction->phi.dst].phi = true;
			intervals[instruction->phi.dst].definingPhi = *instruction;
		}

		if(b->isLoopHeader(blocks)) {
			u16 loopEndBlockIndex = b->getLoopEnd(blocks);
			for(auto operand : live) {
				intervals[operand].addRange({b->fromLIR(), blocks[loopEndBlockIndex].toLIR()});
			}
		}

		b->liveIn = live;
	}

	for(u16 i = 0; i != _function.parameters.size(); ++i) {
		intervals[i].argument = true;
		intervals[i].startingSpan().from = -1;
	}

	for(int temporary = 0; temporary < intervals.size(); ++temporary) {
		Logger::log(Topic::LIFE_RANGES) << temporary << " ranges: ";
		for(auto range = intervals[temporary].lifespans.begin(); range != intervals[temporary].lifespans.end(); ++range) {
			Logger::log(Topic::LIFE_RANGES) << *range << ' ';
		}
		Logger::log(Topic::LIFE_RANGES) << std::endl;
	}

	std::vector<Interval> unionized = unionIntervals(intervals);

	return unionized;
}

std::vector<bytecode::Instruction>::const_iterator Block::instructionBegin() {
	return function.instructions.cbegin() + from;
}

std::vector<bytecode::Instruction>::const_iterator Block::instructionEnd() {
	return function.instructions.cbegin() + to + 1;
}

std::vector<bytecode::Instruction>::const_reverse_iterator Block::instructionReverseBegin() {
	return function.instructions.crbegin() + (function.instructions.size() - (to + 1));
}

std::vector<bytecode::Instruction>::const_reverse_iterator Block::instructionReverseEnd() {
	return function.instructions.crbegin() + (function.instructions.size() - from);
}

bool Block::isLoopHeader(std::vector<Block> blocks) {
	for(auto it = blocks.begin() + this->index; it != blocks.end(); ++it) {
		std::vector<u16> successors = it->blockInfo.successors;
		if(std::find(successors.begin(), successors.end(), this->index) != successors.end()) {
			return true;
		}
	}
	return false;
}

u16 Block::getLoopEnd(std::vector<Block> blocks) {
	u16 max = 0;
	for(auto it = blocks.begin() + this->index; it != blocks.end(); ++it) {
		std::vector<u16> successors = it->blockInfo.successors;
		if(std::find(successors.begin(), successors.end(), this->index) != successors.end()) {
			max = it->index;
		}
	}
	return max;
}

u16 Block::fromLIR() const {
	return lirs.front().id;
}

u16 Block::toLIR() const {
	return lirs.back().id;
}
}}
