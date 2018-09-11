#pragma once

#include <bytecode.hpp>
#include <algorithm>
#include <set>
#include <vector>
#include <queue>
#include <functional>
#include <log/Logger.hpp>
#include <jit/Operands.hpp>
#include <exception/InvalidResultException.hpp>
#include <iomanip>
#include <exception/InvalidRangeOrderException.hpp>
#include <exception/InvalidIntervalSplittingException.hpp>
#include <jit/lir/Instruction.hpp>
#include <jit/allocator/register/StackAllocator.hpp>
#include <map>

using namespace std;

namespace am2017s { namespace jit {

struct Lifespan {
	i32 from, to;

	friend std::ostream& operator<<(std::ostream& os, const Lifespan& that)
	{
		os << '[' << std::to_string(that.from) << ' ' << std::to_string(that.to) << "]";
		return os;
	}

	inline bool operator<(Lifespan const& other) const {
		return from < other.from;
	};

	inline bool operator>(Lifespan const& other) const {
		return other < *this;
	};

	inline bool operator==(Lifespan const& other) const {
		return !(other < *this) && !(other > *this);
	}

};

struct Interval;

namespace interval { namespace internal {

template<typename RegType>
RegType reg(Interval const& i);

template<>
RegOp reg(Interval const& i);

template<>
XMMOp reg(Interval const& i);

}}

struct Interval {
	std::deque<Lifespan> lifespans;
	lir::vr vr;
	bytecode::Type type;

	RegOp _reg = NONE;
	XMMOp _xmm = XMMNONE;
	allocator::StackSlot stack;

	bool argument = false;

	bool phi = false;
	lir::Instruction definingPhi;

	/**
	 * true if the interval has been split
	 */
	bool hasFollower = false;

	/**
	 * Whether or not the it is a fixed interval
	 */
	bool isFixed = false;

public:
	std::map<i32, lir::Usage> usages;

	/**
	 * Requires the newSpan to be smaller (before) all previous spans
	 * @param newSpan
	 */
	void addRange(Lifespan newSpan) {
		if(!lifespans.empty()) {
			if(newSpan.from > lifespans.front().from) {
				throw InvalidRangeOrderException();
			}
		}

		bool merged = false;
		for(auto it = lifespans.begin(); it != lifespans.end();) {
			bool deleted = false;

			auto& oldSpan = *it;

			// if the newSpan covers (at least part of) the oldSpan we merge them
			if(newSpan.from <= oldSpan.to && oldSpan.from <= newSpan.to) {
				oldSpan.from = std::min(oldSpan.from, newSpan.from);
				oldSpan.to = std::max(oldSpan.to, newSpan.to);

				// if we have merged before and now found *another* range that has been extended
				// and they are the same (because the newSpan touches multiple oldSpan's) then
				// we delete the old span
				if(merged && oldSpan == newSpan) {
					it = lifespans.erase(it);
					deleted = true;
				} else {
					// we must only set newSpan to oldSpan if we didn't just modify the
					// iterator because otherwise oldSpan already points to the next element!

					// make sure that when searching for other intersections we use the extended
					// span
					newSpan = oldSpan;
				}
				merged = true;
			}

			if(!deleted) {
				++it;
			}
		}

		// if we've merged intervals than the span is already in the list so no need to add it
		if(merged) {
			return;
		}

		lifespans.push_front(newSpan);
	}

	Lifespan& startingSpan() {
		return lifespans.front();
	}

	i32 start() const {
		return lifespans.front().from;
	}

	i32 end() const {
		return lifespans.back().to;
	}

	bool covers(u16 position) const {
		for(auto lifespan : lifespans) {
			if(lifespan.from <= position && lifespan.to >= position) {
				return true;
			}
		}

		return false;
	}

	bool intersectsWith(Interval& other) {
		auto that = lifespans.begin();
		auto those = other.lifespans.begin();

		while(that != lifespans.end() && those != other.lifespans.end()) {
			if(that->to < those->from) {
				++that;
				continue;
			}
			if(those->to < that->from) {
				++those;
				continue;
			}

			if(those->from <= that->to && that->from <= those->to) {
				return true;
			}
		}

		return false;
	}

	u16 intersect(Interval& other) {
		auto that = lifespans.begin();
		auto those = other.lifespans.begin();

		while(that != lifespans.end() && those != other.lifespans.end()) {
			if(that->to < those->from) {
				++that;
				continue;
			}
			if(those->to < that->from) {
				++those;
				continue;
			}

			if(those->from <= that->to && that->from <= those->to) {
				if(that->from <= those->from) {
					return those->from;
				} else /*if(those->from <= that->from)*/ {
					return that->from;
				}
			}
		}

		throw InvalidResultException();
	}

	Interval split(u16 at) {
		Logger::log(Topic::REG_SPLIT) << "splitting interval " << vr << " (currently on " << _reg << ") at " << at << ": ";
		Interval interval;
		interval.vr = vr;
		interval.hasFollower = hasFollower;
		interval.usages = usages;
		interval.type = type;

		for(auto it = lifespans.begin(); it != lifespans.end(); ++it) {
			if(it->from == at) {
				Logger::log(Topic::REG_SPLIT) << it->from << " " << it->to;
				std::move(it, lifespans.end(), std::back_inserter(interval.lifespans));
				lifespans.erase(it, lifespans.end());
				break;
			} else if(it->from < at && it->to >= at) {
				Logger::log(Topic::REG_SPLIT) << it->from << " " << it->to;
				Lifespan border = *it;
				border.from = at;

				interval.lifespans.push_front(border);
				std::move(it + 1, lifespans.end(), std::back_inserter(interval.lifespans));
				lifespans.erase(it + 1, lifespans.end());

				it->to = at - 1;
				break;
			}
		}

		hasFollower = true;

		if(lifespans.empty() || interval.lifespans.empty()) {
			throw InvalidIntervalSplittingException();
		}

		Logger::log(Topic::REG_SPLIT) << ": old interval [" << start() << ", " << end() << "] new interval [" << interval.start()
		              << ", " << interval.end() << "]" << std::endl;

		return interval;
	}

	bool hasUsage() const {
		i32 thisStartsAt = start();
		for(auto& pair : usages) {
			if(pair.first >= thisStartsAt) {
				return true;
			}
		}

		return false;
	}

	u16 firstUsage() const {
		i32 thisStartsAt = start();
		for(auto& pair : usages) {
			if(pair.first >= thisStartsAt) {
				return pair.first;
			}
		}

		throw InvalidResultException();
	}

	u16 firstRegisterUsage() const {
		i32 thisStartsAt = start();
		for(auto& pair : usages) {
			if(pair.first >= thisStartsAt) {
				if(pair.second.mustHaveReg) {
					return pair.first;
				}
			}
		}

		throw InvalidResultException();
	}

	bool hasRegisterUsage() const {
		i32 thisStartsAt = start();
		i32 thisEndsAt = end();
		for(auto& pair : usages) {
			if(pair.first >= thisStartsAt && pair.first <= thisEndsAt) {
				if(pair.second.mustHaveReg) {
					return true;
				}
			}
		}

		return false;
	}

	u16 endOfHole(u16 startSearchAt) const {
		for(auto& span : lifespans) {
			if(span.from >= startSearchAt) {
				return span.from;
			}
		}

		throw InvalidResultException();
	}

	template<typename RegType>
	RegType reg() const {
		return interval::internal::reg<RegType>(*this);
	}

	void reg(RegOp reg) {
		_reg = reg;
	}

	void reg(XMMOp reg) {
		_xmm = reg;
	}

	bool hasRegister() const {
		if(type.isInteger() && _reg != NONE) {
			return true;
		}

		if(type.isFloatingPoint() && _xmm != XMMNONE) {
			return true;
		}

		return false;
	}

	void toLifeline(std::ostream& os) const {
		os << (isFixed ? "fixed   " : "volatile") << " interval i" << std::left << std::setw(6) << vr;

		if(_reg != NONE) {
			if(isFixed) {
			os << " (in fixed        " << std::right << std::setw(3) << std::to_string(_reg) <<   "):     ";
			} else {
			os << " (in register     " << std::right << std::setw(3) << std::to_string(_reg) <<   "):     ";
			}
		} else if(_xmm != XMMNONE) {
			if(isFixed) {
			os << " (in xmm fixed    " << std::right << std::setw(3) << std::to_string(_xmm) <<   "):     ";
			} else {
			os << " (in xmm register " << std::right << std::setw(3) << std::to_string(_xmm) <<   "):     ";
			}
		} else {
			os << " (on stack " << stack << "):     ";
		}

		if(argument) {
			os << "a";
		} else {
			os << "|";
		}

		int instruction = 0;
		for(auto span : lifespans) {
			while(instruction < span.from) {
				os << ' ';
				++instruction;
			}

			while(instruction <= span.to) {
				if(usages.count((u16)instruction)) {
					os << ((usages.at((u16)instruction).mustHaveReg) ? 'r' : 'x');
				} else {
					os << 'o';
				}
				++instruction;
			}
		}

		os << std::endl;
	}

	inline bool operator<(Interval const& other) const {
		if(start() < other.start()) {
			return true;
		}

		if((start() == other.start()) && argument && !other.argument) {
			return true;
		}

		if((start() == other.start()) && argument && other.argument) {
			return vr < other.vr;
		}

		if((start() == other.start() && !argument && !other.argument) && isFixed && !other.isFixed) {
			return true;
		}

		// it's vital for the functionality of allocateBlockedRegsiter, that (if everything else is equal) the
		// intervals with the first usage is processed first
		if((start() == other.start() && argument == other.argument && isFixed == other.isFixed) && !hasUsage() && other.hasUsage()) {
			return true;
		}

		if((start() == other.start() && argument == other.argument && isFixed == other.isFixed) && hasUsage() && other.hasUsage() && firstUsage() < other.firstUsage()) {
			return true;
		}

		return false;
	};

	inline bool operator>(Interval const& other) const {
		return other < *this;
	};

	friend std::ostream& operator<<(std::ostream& os, const Interval& that)
	{
		for(auto life : that.lifespans) {
			os << life;
		}
		return os << " in register " << that._reg;
	}

};

struct Block {
	bytecode::Block blockInfo;
	bytecode::Function const& function;
	u16 index;
	std::set<u16> temporariesGenerated;
	std::set<u16> liveIn;

	std::vector<am2017s::jit::lir::Instruction> lirs;

private:
	u16 from, to;

public:
	Block(bytecode::Block const& _blockInfo, bytecode::Function const& _function, u16 _from, u16 _to)
			: blockInfo(_blockInfo), function(_function), from(_from), to(_to) {};


	bool isLoopHeader(std::vector<Block> blocks);
	u16 getLoopEnd(std::vector<Block> blocks);
	u16 fromLIR() const;
	u16 toLIR() const;
	std::vector<bytecode::Instruction>::const_iterator instructionBegin();
	std::vector<bytecode::Instruction>::const_iterator instructionEnd();
	std::vector<bytecode::Instruction>::const_reverse_iterator instructionReverseBegin();
	std::vector<bytecode::Instruction>::const_reverse_iterator instructionReverseEnd();
};

class LifetimeAnalyzer {
private:
	bytecode::Function const& _function;
	std::vector<Block>& blocks;
	u16 lirCount;
public:
	LifetimeAnalyzer(const bytecode::Function& function, vector<Block> _blocks, u16 i);
	vector<Interval> run() &&;
};

}}


