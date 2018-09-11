#include <queue>
#include <jit/allocator/register/RegisterAllocator.hpp>
#include <jit/Operands.hpp>
#include <jit/architecture/Architecture.hpp>
#include <log/Logger.hpp>
#include <map>
#include <exception>

namespace am2017s { namespace jit { namespace allocator {

template <class Architecture>
RegisterAllocation<Architecture>::RegisterAllocation(bytecode::Function const& _function,
                                                     vector<Interval>& _lifespans,
                                                     lir::UsageMap& _usages,
                                                     std::map<RegOp, lir::vr> const& _fixedToVR,
                                                     std::map<XMMOp, lir::vr> const& _fixedXMMToVR,
                                                     std::map<u16, lir::vr> const& _overflowArgToVR,
                                                     std::map<lir::vr, bytecode::Type> const& vrTypes,
                                                     std::set<std::set<lir::vr>> const& hintSame) :
function(_function), lifespans(_lifespans), usages(_usages), fixedToVR(_fixedToVR), fixedXMMToVR(_fixedXMMToVR), overflowArgToVR(_overflowArgToVR), vrTypes(vrTypes), hintSame(hintSame) {}

template <class Architecture>
int RegisterAllocation<Architecture>::run() {
	linearScan();
	stackAllocator.freeze();
	return 0;
}

template<class Architecture>
void RegisterAllocation<Architecture>::linearScan() {

	for(Interval& i : lifespans) {
		i.usages = usages.at(i.vr);

		for(auto pair : fixedToVR) {
			if(i.vr == pair.second) {
				i.isFixed = true;
				i._reg = pair.first;
				fixed.push_back(i);
			}
		}
		for(auto pair : fixedXMMToVR) {
			if(i.vr == pair.second) {
				i.isFixed = true;
				i._xmm = pair.first;
				fixed.push_back(i);
			}
		}
		for(auto pair : overflowArgToVR) {
			if(i.vr == pair.second) {
				i.isFixed = true;
				i._reg = NONE;
				i.stack = stackAllocator.reserveArgument(pair.first);
				fixed.push_back(i);
			}
		}
	}

	Logger::log(Topic::LIFE_LINES) << "Lifelines before register allocation: --------------" << std::endl;
	for(const Interval& i : lifespans) {
		i.toLifeline(Logger::log(Topic::LIFE_LINES));
	}

	// unhandled = list of intervals sorted by increasing start positions
	std::priority_queue<Interval, std::deque<Interval>, std::greater<>> unhandled(lifespans.begin(), lifespans.end());
	// active = { }; inactive = { }; handled = { }

	std::vector<RegOp> const& parameters = Architecture::parameters();
	auto paramIterator = parameters.cbegin();
	std::vector<XMMOp> const& floatParameters = Architecture::parametersFloat();
	auto floatParamIterator = floatParameters.cbegin();

	u16 overflowIndex = 0;
	for(u16 i = 0; i != function.parameters.size(); ++i) {
		Interval current = unhandled.top();
		unhandled.pop();

		if(current.lifespans.empty()) {
			continue;
		}

		if(vrTypes.at(current.vr).isFloatingPoint() && floatParamIterator != floatParameters.cend()) {
			current._xmm = *(floatParamIterator++);
		} else if(vrTypes.at(current.vr).isInteger() && paramIterator != parameters.cend()) {
			current._reg = *(paramIterator++);
		} else {
			current.stack = {PARAMETER, QWORD, overflowIndex};
			overflowIndex += 8;

			// if the stack argument has a register usage we split just before that
			// usage in order to make filling possible
			if(current.hasRegisterUsage()) {
				Interval tailOfCurrent = current.split(current.firstRegisterUsage());
				unhandled.push(tailOfCurrent);
			}

		}

		active.push_back(current);
	}


	// while lifespans != {} do
	while(!unhandled.empty()) {
		// current = pick and remove first interval from lifespans
		Interval current = unhandled.top();
		current.type = vrTypes.at(current.vr);
		unhandled.pop();


		if(current.lifespans.empty()) {
			continue;
		}


		// position = start position of current
		i32 position = current.start();

		Logger::log(Topic::REG_LOG) << "---- processing i" << current.vr << "(" << current.start() << ")" << std::endl;

		// check for intervals in active that are handled or inactive
		// for each interval it in active do
		for(auto interval = active.begin(); interval != active.end();) {
			// if it ends before position then
			if(interval->end() < position) {
				Logger::log(Topic::REG_LOG) << interval->vr << "(- " << interval->end() << ") is done" << std::endl;
				// move it from active to handled
				handled.push_back(*interval);
				interval = active.erase(interval);
				continue;
			}
			// else if it does not cover position then
			else if(!interval->covers(position)) {
				// move it from active to inactive
				inactive.push_back(*interval);
				interval = active.erase(interval);
				continue;
			}

			++interval;
		}

		// check for intervals in inactive that are handled or active
		// for each interval it in inactive do
		for(auto interval = inactive.begin(); interval != inactive.end();) {
			// if it ends before position then
			if(interval->end() < position) {
				// move it from active to handled
				handled.push_back(*interval);
				interval = inactive.erase(interval);
				continue;
			}
			// else if it covers position then
			else if(interval->covers(position)) {
				// move it from inactive to active
				// move it from active to inactive
				active.push_back(*interval);
				interval = inactive.erase(interval);
				continue;
			}

			++interval;
		}

		if(current.isFixed) {
			if(vrTypes.at(current.vr).isInteger()) {
				for(auto pair : fixedToVR) {
					if(current.vr == pair.second) {
						current.isFixed = true;
						current._reg = pair.first;
						fixedToInterval[current._reg] = current;

						for(auto x : active) {
							if(x._reg == current._reg) {
								Logger::log(Topic::REG_LOG) << "someone is on that reg!" << std::endl;
							}
						}

						handleIntervalBeingPushedOffRegister<RegOp>(current, unhandled, current._reg);
					}
				}
			} else {
				for(auto pair : fixedXMMToVR) {
					if(current.vr == pair.second) {
						current.isFixed = true;
						current._xmm = pair.first;
						fixedXMMToInterval[current._xmm] = current;

						for(auto x : active) {
							if(x._xmm == current._xmm) {
								Logger::log(Topic::REG_LOG) << "someone is on that xmm!" << std::endl;
							}
						}

						handleIntervalBeingPushedOffRegister<XMMOp>(current, unhandled, current._xmm);
					}
				}
			}
		} else {
			// find a register for current
			// TRYALLOCATEFREEREG
			bool allocationFailed;
			if(vrTypes.at(current.vr).isInteger()) {
				allocationFailed = !(tryAllocateFreeRegister<RegOp>(current, unhandled));
			} else {
				allocationFailed = !(tryAllocateFreeRegister<XMMOp>(current, unhandled));
			}

			// if allocation failed then ALLOCATEBLOCKEDREG
			if(allocationFailed && vrTypes.at(current.vr).isInteger()) {
				allocateBlockedRegister<RegOp>(current, unhandled);
			} else if(allocationFailed && vrTypes.at(current.vr).isFloatingPoint()) {
				allocateBlockedRegister<XMMOp>(current, unhandled);
			}
		}

		// if current has a register assigned then add current to active
		if(vrTypes.at(current.vr).isInteger() && current._reg != NONE) {
			active.push_back(current);
			usedRegisters.insert(current._reg);
			Logger::log(Topic::REG_LOG) << "assigned " << current._reg << " to i" << current.vr << " for " << current.start() << " - " << current.end() << std::endl;
		} else if(vrTypes.at(current.vr).isFloatingPoint() && current.reg<XMMOp>() != XMMNONE) {
			active.push_back(current);
			Logger::log(Topic::REG_LOG) << "assigned xmm " << current._xmm << " to i" << current.vr << " for " << current.start() << " - " << current.end() << std::endl;
		} else {
			handled.push_back(current);
			Logger::log(Topic::REG_LOG) << "assigned stack " << current.stack << " to i" << current.vr << " for " << current.start() << " - " << current.end() << std::endl;
		}
	}

	handled.insert(handled.end(), active.begin(), active.end());
	handled.insert(handled.end(), inactive.begin(), inactive.end());

	std::vector<RegOp> calleeSaved = Architecture::calleeSaved();
	for(auto reg : usedRegisters) {
		if(std::find(calleeSaved.begin(), calleeSaved.end(), reg) != calleeSaved.end()) {
			StackSlot const& stackSlot = stackAllocator.reserveScratch(QWORD);
			stackFrameSpills.push_back({RegMemOp(reg), stackSlot, QWORD});
		}
	}

	Logger::log(Topic::LIFE_LINES) << "Lifelines after register allocation: ---------------" << std::endl;
	for(const Interval& i : handled) {
		i.toLifeline(Logger::log(Topic::LIFE_LINES));
	}
}

/**
 * Assigns the key-value-pair to the map iff the map contains the given key
 *
 * @tparam key_type
 * @tparam value_type
 * @param map
 * @param key
 * @param value
 */
template<typename key_type, typename value_type>
static void mapAssign(std::map<key_type, value_type>& map, key_type key, value_type value) {
	if(map.count(key)) {
		map[key] = value;
	}
}

template<class Architecture>
template<typename RegType>
RegType RegisterAllocation<Architecture>::chooseFreeRegister(Interval const& current, std::map<RegType, u16> freeUntilPos) {

	std::set<RegType> registersFromHints;

	bool found = false;
	for(auto sameSet : hintSame) {
		for(auto x : sameSet) {
			if(x == current.vr) {
				Logger::log(Topic::REG_HINTS) << "found hint for i" << current.vr << std::endl;

				// iterate over the current same set again and check if any of those is
				// active
				for(auto x1 : sameSet) {
					for(auto interval : handled) {
						if(interval.vr == x1 && interval.hasRegister()) {
							found = true;
							Logger::log(Topic::REG_HINTS) << "found handled interval for vr " << x1 << " from the same set" << std::endl;
							Logger::log(Topic::REG_HINTS) << interval << std::endl;
							registersFromHints.insert(interval.template reg<RegType>());
						}
					}
				}

				if(!found) {
					Logger::log(Topic::REG_HINTS) << "nothing found" << std::endl;
				}

				break;
			}
		}
	}

	if(found) {
		RegType reg = *std::max_element(registersFromHints.begin(),
		                               registersFromHints.end(),
		                               [=](auto lhs, auto rhs) {
			return freeUntilPos.at(lhs) < freeUntilPos.at(rhs);
		});

		if(freeUntilPos.at(reg) != 0) {
			return reg;
		}

		// otherwise simply choose the highest available register
	}

	RegType reg = std::max_element(freeUntilPos.begin(),
	                               freeUntilPos.end(),
	                               [](auto lhs, auto rhs) {
		if(lhs.second < rhs.second) {
			return true;
		}

		// todo prefer callee saved registers

		return false;

	})->first;
	return reg;
}

template<class Architecture>
template<typename RegType>
bool RegisterAllocation<Architecture>::tryAllocateFreeRegister(Interval& current,
                                                               priority_queue<Interval, deque<Interval>, greater<>>& unhandled) {
	std::vector<RegType> registers = Architecture::template registers<RegType>();

	// set freeUntilPos of all physical registers to maxInt
	std::map<RegType, u16> freeUntilPos;
	for(auto reg : registers) {
		freeUntilPos[reg] = (u16)-1;
	}

	// for each interval it in active do
	for(auto it : active) {
		mapAssign(freeUntilPos, it.template reg<RegType>(), (u16) 0);
	}

	// for each interval it in inactive intersecting with current do
	for(auto it : inactive) {
		RegType reg = it.template reg<RegType>();
		if(it.intersectsWith(current)) {
			// freeUntilPos[it.reg] = next intersection of it with current
			// If the freeUntilPos for one register is set multiple times, the minimum of all positions is used. (p132-wimmer p143)

			// of the register does not exist in our list we cannot use it anyway
			if(freeUntilPos.count(reg)) {
				mapAssign(freeUntilPos, reg, std::min(it.intersect(current), freeUntilPos[reg]));
			}
		}
	}

	for(auto it : fixed) {
		RegType reg = it.template reg<RegType>();
		if(it.intersectsWith(current) && freeUntilPos.count(reg)) {
			freeUntilPos[reg] = std::min(it.intersect(current), freeUntilPos[reg]);
		}
	}

	// reg = register with highest freeUntilPos
	RegType reg = chooseFreeRegister(current, freeUntilPos);

	// if freeUntilPos[reg] = 0 then
	if(freeUntilPos[reg] == 0) {
		// no register available without spilling
		return false;
	}
	// else if current ends before freeUntilPos[reg] then
	else if(current.end() < freeUntilPos[reg]) {
		// register available for the whole interval
		current.reg(reg);
		return true;
	}
	// else
	else {
		// register available for the first part of the interval

		// current.reg = reg
		current.reg(reg);
		// split current before freeUntilPos[reg]
		Interval end = current.split(freeUntilPos[reg]);
		unhandled.push(end);
		return true;
	}
}

template<typename RegType>
static RegType chooseBlockedRegister(std::map<RegType, i32> const &nextUsePos) {
	return std::max_element(nextUsePos.begin(),
	                        nextUsePos.end(),
	                        [](auto lhs, auto rhs) { return lhs.second < rhs.second; })->first;
}

template<class Architecture>
template<typename RegType>
void RegisterAllocation<Architecture>::allocateBlockedRegister(Interval& current,
                                                               priority_queue<Interval, deque<Interval>, greater<>>& unhandled) {
	std::vector<RegType> registers = Architecture::template registers<RegType>();

	// set nextUsePos of all physical registers to maxInt
	std::map<RegType, i32> nextUsePos;
	for(auto reg : registers) {
		nextUsePos[reg] = (u16) -1;
	}

	// for each interval it in active do
	for(auto it : active) {
		RegType reg = it.template reg<RegType>();
		if(it.isFixed) {
			nextUsePos.erase(reg);
		} else {
			// nextUsePos[it.reg] = next use of it after start of current
			mapAssign(nextUsePos, reg, std::find_if(usages.at(it.vr).begin(), usages.at(it.vr).end(),
			                                        [current](auto use) { return use.first >= current.start(); })->first);
		}
	}

	// for each interval it in inactive intersecting with current do
	for(auto it : inactive) {
		RegType reg = it.template reg<RegType>();
		if(it.intersectsWith(current)) {
			if(it.isFixed) {
				nextUsePos.erase(reg);
			} else {
				// nextUsePos[it.reg] = next use of it after start of current
				mapAssign(nextUsePos, reg, std::find_if(usages.at(it.vr).begin(), usages.at(it.vr).end(),
				                                  [current](auto use) { return use.first >= current.start(); })->first);
			}
		}
	}

	for(auto it : fixed) {
		if(it.intersectsWith(current)) {
			nextUsePos.erase(it.template reg<RegType>());
		}
	}

	for(auto pair : nextUsePos) {
		Logger::log(Topic::REG_LOG) << pair.first << ": " << pair.second << ", ";
	}
	Logger::log(Topic::REG_LOG) << std::endl;

	if(nextUsePos.empty()) {
		Logger::log(Topic::REG_LOG) << "not a single register available" << std::endl;
	}

	// reg = register with highest nextUsePos
	RegType reg = chooseBlockedRegister(nextUsePos);

	// if first usage of current is after nextUsePos[reg] then
	if(!current.hasUsage() || current.firstUsage() > nextUsePos[reg]) {
		// all other intervals are used before current,
		// so it is best to spill current itself

		i32 startsAt = current.start();
		u16 vr = current.vr;

		// check whether a previous part of this interval is on the stack. if so we can reuse it
		auto predecessor = std::find_if(handled.begin(), handled.end(), [startsAt, vr](auto f) { return f.hasFollower && f.vr == vr && f.end() + 1 == startsAt && (((u8) f._reg) == NONE) && f._xmm == XMMNONE; });

		// todo perform linear scan or other strategy on the stack
		// assign spill slot to current
		if(predecessor != handled.end()) {
			Logger::log(Topic::REG_LOG) << "has stack follower" << std::endl;
			current.stack = predecessor->stack;
		} else {
			current.stack = stackAllocator.reserveScratch(QWORD);
		}

		Logger::log(Topic::REG_LOG) << "spill!" << std::endl;
		if(current.hasRegisterUsage()) {
			Interval tailOfCurrent = current.split(current.firstRegisterUsage());
			unhandled.push(tailOfCurrent);
		}
	} else {
		// spill intervals that currently block reg
		// current.reg = reg
		current.reg(reg);

		handleIntervalBeingPushedOffRegister<RegType>(current, unhandled, reg);
	}

	// make sure that current does not intersect with
	// the fixed interval for reg
	if(current.intersectsWith(fixedToInterval[current._reg])) {
		Interval tailOfCurrent = current.split(current.intersect(fixedToInterval[current._reg]));
		unhandled.push(tailOfCurrent);
	} else if(current.intersectsWith(fixedXMMToInterval[current._xmm])) {
		Interval tailOfCurrent = current.split(current.intersect(fixedXMMToInterval[current._xmm]));
		unhandled.push(tailOfCurrent);
	}
}

template<class Architecture>
template<typename RegType>
void RegisterAllocation<Architecture>::handleIntervalBeingPushedOffRegister(Interval& current,
                                                                           priority_queue<Interval, deque<Interval>, greater<>>& unhandled, RegType reg) {
	// split active interval for reg at position
	auto optionalOnReg = std::find_if(active.begin(), active.end(), [reg](auto aInterval) { return aInterval.template reg<RegType>() == reg; });
	if(optionalOnReg == active.end()) {
		return;
	}

	Interval& onReg = *optionalOnReg;
	Interval tailOfOnReg = onReg.split(current.start());
	unhandled.push(tailOfOnReg);

	// split any inactive interval for reg at the end of its lifetime hole
	// todo test
	for(Interval& it : inactive) {
		if(!it.isFixed && it.template reg<RegType>() == reg) {
			Logger::log(Topic::REG_LOG) << "splitting at end of lifetime hole" << std::endl;
			u16 endOfHole = it.endOfHole(current.start());
			Interval tailOfInactiveInterval = it.split(endOfHole);
			unhandled.push(tailOfInactiveInterval);
		}
	}
}

}}}
