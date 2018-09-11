#include <jit/machine/MachineCompiler.hpp>
#include <jit/lifetime/LifetimeAnalyzer.hpp>
#include <log/Logger.hpp>
#include <exception/NotImplementedException.hpp>

namespace am2017s { namespace jit {

MachineCompiler::MachineCompiler(std::vector<jit::Block> const& _blocks,
                                 std::vector<Interval> const& _intervals,
                                 allocator::StackAllocator const& _stack,
                                 std::map<lir::vr, bytecode::Type> const& vrTypes,
                                 std::vector<StackSpillMovOp> const& stackFrameSpills)
	: blocks(_blocks), intervals(_intervals), stack(_stack), vrTypes(vrTypes), stackFrameSpills(stackFrameSpills) {}

void MachineCompiler::run() {

	std::map<u16, std::map<u16, std::vector<SpillMovOp>>> edgeInstructions;

	// target -> source
	std::map<u16, u16> conditionalEdgeInstructionsAtTarget;

	for(Block const& predecessor : blocks) {
		for(u16 sIndex : predecessor.blockInfo.successors) {
			Block const& successor = blocks[sIndex];

			// todo make slot compatible (only reg for now)
			for(Interval const& interval : intervals) {
				if(interval.isFixed) {
					continue;
				}

				if(interval.covers(successor.fromLIR())) {
					RegOp moveFrom;

					// if it starts at begin of successor then
					// first check if the interval starts exactly at the start position
					if(interval.start() == successor.fromLIR()) {
						// if so, make sure it's actually a phi node (it could be any other instruction
						// because we do not waste the first instruction on useless instructions)
						if(interval.phi) {
							lir::Instruction const& phi = interval.definingPhi;
							lir::vr operand = phi.phi.inputOf(predecessor.index);
							moveFrom = intervalFor(predecessor.toLIR(), operand)._reg;
						} else {
							// if it's not a phi node than interval.covers(success.fromLIR()) should have been
							// false in the first place so let's just continue
							continue;
						}
					} else {
						// we come here if we extend a interval from one block to another
						// e.g. if the predecessor block has multiple children that all inherit
						// a variable or if the predecessor block is daisy-chained to the successor
						moveFrom = intervalFor(predecessor.toLIR(), interval.vr)._reg;
					}

					RegOp moveTo = intervalFor(successor.fromLIR(), interval.vr)._reg;

					if(moveFrom != moveTo) {
						// if the edge instruction relates to a conditional jump
						if(predecessor.lirs.back().operation == lir::Operation::JNZ) {
							// there's a simple solution if the successor has only one parent: put the
							// edge instructions at the beginning of the successor
							if(successor.blockInfo.predecessors.size() == 1) {
								conditionalEdgeInstructionsAtTarget[successor.index] = predecessor.index;
							} else {
								throw std::runtime_error("don't know how to insert edge instructions for conditional jumps");
							}
						}

						edgeInstructions[predecessor.index][successor.index].push_back({moveFrom, moveTo, interval.type.size()});
						Logger::log(Topic::MACHINE) << "(block " << predecessor.index << " -> block " << sIndex
						                            << ") moving i" << interval.vr << " from " << moveFrom << " to "
						                            << moveTo << std::endl;
					}
				}
			}
		}
	}

	//// Generate spill moves

	std::map<u16, std::vector<SpillMovOp>> spillMoves;

	for(Interval const& interval : intervals) {
		if(interval.hasFollower) {
			u16 startsAt = (u16) (interval.end() + 1);
			lir::vr vr = interval.vr;
			Interval const& follower = *std::find_if(intervals.begin(), intervals.end(),
					[startsAt, vr](auto f) { return f.vr == vr && f.start() == startsAt; });

			RegMemOp src;
			if(interval._reg != NONE) {
				src = RegMemOp(interval._reg);
			} else if(interval._xmm != XMMNONE) {
				src = RegMemOp(interval._xmm);
			} else {
				src = RegMemOp(stack.getAddressing(interval.stack));
			}

			RegMemOp dst;
			if(follower._reg != NONE) {
				dst = RegMemOp(follower._reg);
			} else if(follower._xmm != XMMNONE) {
				dst = RegMemOp(follower._xmm);
			} else {
				dst = RegMemOp(stack.getAddressing(follower.stack));
			}

			if(src == dst) {
				continue;
			}

			Logger::log(Topic::MACHINE) << "determined a spill move for interval " << vr << " at location " << startsAt << ": " << src << " -> " << dst << std::endl;
			spillMoves[startsAt].push_back({src, dst, vrTypes.at(vr).size()});
		}
	}


	auto sortedInstructions = orderEdgeInstructions(edgeInstructions);

	//// Function header
	builder.sub(RSP, stack.getStackSize());
	for(auto spill : stackFrameSpills) {
		builder.mov(spill.source, stack.getAddressing(spill.target), QWORD);
	}

	//// Actual Instructions follow

	/**
	 * blockIndex -> {rip, where to insert}
	 *
	 * contains a mapping where to insert the blockIndex
	 *
	 * e.g. 1 -> {16, 12} means that we want to insert the address of block 1 at the consecutive
	 * bytes [12, 13, 14, 15] (or the qword 12-15) and since it's RIP relative the RIP
	 * at the given instruction is 16.
	 */
	std::map<u16, std::set<std::pair<u32, u32>>> insertBlockAddressAt;
	std::map<u16, u32> blockAddresses;

	u16 prevBlock = (u16)-1;
	for(Block const& block : blocks) {

		if(!edgeInstructions[prevBlock][block.index].empty()) {
			insertEdgeInstructions(edgeInstructions, sortedInstructions, prevBlock, block.index);
		}

		// save block address after the edge instructions since jumps regulate their edge instructions
		// differently
		blockAddresses[block.index] = builder.offset();

		if(conditionalEdgeInstructionsAtTarget.count(block.index)) {
			insertEdgeInstructions(edgeInstructions, sortedInstructions,
			                       conditionalEdgeInstructionsAtTarget.at(block.index), block.index);
		}


		for(lir::Instruction const& instruction : block.lirs) {
			u16 id = instruction.id;
			u32 offset;

			if(spillMoves.count(id)) {
				auto sorted = topologicallySort(spillMoves[id]);
				if(sorted.empty()) {
					// todo fix this
					Logger::err() << "no topological sorting for spill moves found" << std::endl;
				} else {
					for(auto movPair : sorted) {
						Logger::log(Topic::MACHINE) << "spilling (at offset " << builder.offset() << ") before instruction " << id << std::endl;
						builder.mov(movPair.first, movPair.second, movPair.size);
					}
				}
			}

			switch(instruction.operation) {
				case lir::FMOV:
				case lir::MOV:
					if(instruction.mov.isImm) {
						Interval const& dst = intervalFor(id, instruction.mov.dst);
						builder.movimm(instruction.mov.imm, dst._reg);
					} else {
						RegMemOp src = operandFor(id, instruction.mov.src);
						RegMemOp dst = operandFor(id, instruction.mov.dst);

						builder.mov(src, dst, instruction.mov.size);
					}
					break;
				case lir::PHI:break;
				case lir::CMP:
				{
					// left is guaranteed to be in a reg
					RegMemOp left = operandFor(id, instruction.cmp.l);
					RegMemOp right = operandFor(id, instruction.cmp.r);
					builder.cmp(left.reg(), right);
				}
					break;
				case lir::SET:
				{
					// todo distinguish reg/mem
					RegMemOp reg = operandFor(id, instruction.flag.reg);
					switch(instruction.flag.mode) {
						case lir::LT: builder.set(jit::internal::Comparison::LT, reg.reg()); break;
						case lir::LTE: builder.set(jit::internal::Comparison::LTE, reg.reg()); break;
						case lir::EQ: builder.set(jit::internal::Comparison::EQ, reg.reg()); break;
						case lir::NEQ: builder.set(jit::internal::Comparison::NEQ, reg.reg()); break;
						case lir::GTE: builder.set(jit::internal::Comparison::GTE, reg.reg()); break;
						case lir::GT: builder.set(jit::internal::Comparison::GT, reg.reg()); break;
					}

				}
					break;
				case lir::NEG:
				{
					// guaranteed to be in reg
					RegMemOp reg = operandFor(id, instruction.unary.dst);
					builder.neg(reg.reg(), vrTypes.at(instruction.unary.dst).size());
				}
				case lir::NOT:
				{
					// guaranteed to be in reg
					RegMemOp reg = operandFor(id, instruction.unary.dst);
					builder.not_(reg.reg());
				}
					break;
				case lir::TEST:
					// todo distinguish reg/mem
					if(!operandFor(id, instruction.flag.reg).isReg()) {
						throw new std::runtime_error("test not implemented for mem operands yet");
					}

					builder.test(operandFor(id, instruction.flag.reg).reg());
					break;
				case lir::JMP:
					insertEdgeInstructions(edgeInstructions, sortedInstructions, block.index, instruction.jump.target);
					offset = builder.jmp_riprel();
					insertBlockAddressAt[instruction.jump.target].insert({builder.offset(), offset});
					break;
				case lir::JNZ:
					if(!edgeInstructions[block.index][instruction.jump.target].empty()) {
						if(blocks[instruction.jump.target].blockInfo.predecessors.size() == 1) {
							std::cout << "could insert the edge instructions to the beginning of the following block";
						}
					}

					offset = builder.jmp_nz_riprel();
					insertBlockAddressAt[instruction.jump.target].insert({builder.offset(), offset});
					break;
				case lir::ADD:
				case lir::FADD:
				{
					// reg or mem
					RegMemOp src = operandFor(id, instruction.binary.src);
					// always reg
					RegMemOp dst = operandFor(id, instruction.binary.dst);

					if(src.isReg() && dst.isReg()) {
						builder.add(src.reg(), dst.reg());
					} else if(src.isXMM() && dst.isXMM()) {
						builder.addf(src.xmm(), dst.xmm(), vrTypes.at(instruction.binary.src).size());
					} else if(src.isMem() && dst.isReg()) {
						builder.add(src.mem(), dst.reg());
					} else {
						throw std::runtime_error("fallthrough in add");
					}
				}
					break;

				case lir::SUB:
				{
					// reg or mem
					RegMemOp src = operandFor(id, instruction.binary.src);
					// always reg
					RegMemOp dst = operandFor(id, instruction.binary.dst);

					builder.sub(src, dst.reg());
				}
					break;
				case lir::MUL:
				{
					RegMemOp dst = operandFor(id, instruction.binary.dst);
					// srcB is guaranteed to be in register
					RegMemOp src = operandFor(id, instruction.binary.src);

					if((src.isReg() && dst.isReg()) || (src.isMem() && dst.isReg())) {
						builder.imul(dst.reg(), src);
					} else if(src.isXMM() && dst.isXMM()) {
						builder.mulf(src.xmm(), dst.xmm(), vrTypes.at(instruction.binary.dst).size());
					}

				}
					break;

				case lir::DIV:
				{
					RegMemOp srcB = operandFor(id, instruction.ternary.srcB);
					if(vrTypes.at(instruction.ternary.srcB).isInteger()) {
						builder.idiv(srcB);
					} else {
						RegMemOp srcA = operandFor(id, instruction.ternary.srcA);
						builder.divf(srcA.xmm(), srcB, vrTypes.at(instruction.ternary.srcB).size());
					}
				}
					break;
				case lir::RET:
					for(auto spill : stackFrameSpills) {
						builder.mov(stack.getAddressing(spill.target), spill.source, QWORD);
					}
					builder.add(RSP, stack.getStackSize());
					builder.ret();
					break;

				case lir::CQO:
					builder.cqo();
					break;

				case lir::CALL:
				{
					builder.call(RegOp::RBP, instruction.call.function * 8);
				}
					break;

				case lir::MOV_MEM:
				{
					// base is guaranteed to be in register
					MemOp memOp{operandFor(id, instruction.memmov.base).reg(), instruction.memmov.offset};

					if(instruction.memmov.isIndexed) {
						memOp = MemOp(operandFor(id, instruction.memmov.base).reg(),
						              operandFor(id, instruction.memmov.index).reg(),
						              instruction.memmov.scale,
						              instruction.memmov.offset);
					}

					// a is guaranteed to be in register
					RegMemOp const& reg = operandFor(id, instruction.memmov.a);

					if(reg.isReg()) {
						RegOp regOp{reg.reg()};
						if (instruction.memmov.toMem) {
							builder.mov(regOp, RegMemOp(memOp), instruction.memmov.size);
						} else {
							if (instruction.memmov.size <= WORD) {
								builder.movsx(RegMemOp(memOp), regOp, instruction.memmov.size);
							} else if (instruction.memmov.size == DWORD) {
								builder.movsxd(RegMemOp(memOp), regOp, instruction.memmov.size);
							} else {
								builder.mov(RegMemOp(memOp), regOp, instruction.memmov.size);
							}
						}
					} else {
						// reg is a xmm
						if(instruction.memmov.toMem) {
							builder.mov(reg, memOp, instruction.memmov.size);
						} else {
							if(instruction.memmov.size == DWORD) {
								builder.movss(memOp, reg.xmm());
							} else if(instruction.memmov.size == QWORD) {
								builder.movq(memOp, reg.xmm(), instruction.memmov.size);
							} else {
								throw std::runtime_error("fallthrough in machine compiler for memmov");
							}
						}
					}
				}
					break;

				case lir::CALL_IDX_IN_REG:
				{
//					builder.mov(RegMemOp(MemOp{RBP, RAX, 1}), RDI, QWORD);
					builder.call(RBP, operandFor(id, instruction.reg_call.idxReg).reg());
				}
					break;

				case lir::NOP:
					break;

				case lir::MOV_I2F:
				{
					RegOp src = operandFor(id, instruction.mov.src).reg();
					XMMOp dst = operandFor(id, instruction.mov.dst).xmm();

					builder.movd(src, dst, instruction.mov.size);
					break;
				}

				default:
				 throw std::runtime_error("LIR opcode not implemented!");
			}
		}

		prevBlock = block.index;
	}

	for(auto& pair : insertBlockAddressAt) {
		u16 blockIndex = pair.first;
		std::set<std::pair<u32, u32>>& ripAndInsertPoints = pair.second;

		for(auto& ripAndInsertPoint : ripAndInsertPoints) {
			builder.quad(blockAddresses[blockIndex] - ripAndInsertPoint.first, ripAndInsertPoint.second);
		}
	}
}

const Interval& MachineCompiler::intervalFor(u16 id, lir::vr vr) {
	for(auto& interval : intervals) {
		if(interval.vr == vr && interval.start() <= id && interval.end() >= id) {
			return interval;
		}
	}

	throw InvalidResultException();
}

std::map<u16, std::map<u16, std::vector<SpillMovOp>>>
MachineCompiler::orderEdgeInstructions(std::map<u16, std::map<u16, std::vector<SpillMovOp>>> edgeInstructions) {
	for(auto& pred : edgeInstructions) {
		for(auto& succ : pred.second) {
			std::vector<SpillMovOp> sorting = topologicallySort(succ.second);
			if(!sorting.empty()) {
				succ.second = sorting;
			} else {
				succ.second = {};
			}
		}
	}
	return edgeInstructions;
}

/**
 * Kahn's algorithm
 *
 * @param vector
 * @return
 */
std::vector<SpillMovOp>
MachineCompiler::topologicallySort(std::vector<SpillMovOp> vector) {
//	for(auto pair : vector) {
//		std::cout << "move " << pair.first << " -> " << pair.second << std::endl;
//	}

	std::map<RegMemOp, RegMemOp> edges;
	std::map<RegMemOp, std::map<RegMemOp, OperandSize>> sizes;
	std::set<RegMemOp> S;
	for(auto const& pair : vector) {
		edges.insert({pair.second, pair.first});
		if(!sizes.count(pair.second)) {
			sizes.insert({pair.second, {}});
		}
		sizes.at(pair.second).insert({pair.first, pair.size});
		S.insert(pair.first);
		S.insert(pair.second);
	}

//	for(auto pair : edges) {
//		std::cout << "edge " << pair.first << " -> " << pair.second << std::endl;
//	}

	for(auto pair : edges) {
//		std::cout << "remove edge starting at " << pair.second << std::endl;
		S.erase(pair.second);
	}

//	for(auto s : S) {
//		std::cout << "S contains " << s << std::endl;
//	}

	std::vector<SpillMovOp> L;
	while(!S.empty()) {
		RegMemOp n = *S.begin();

//		std::cout << n << std::endl;

//		std::cout << "must be true: " << n << " == " << secondN << ": " << (!(n < secondN) && !(secondN < n)) << std::endl;

		S.erase(*S.begin());

		if(edges.count(n)) {
			RegMemOp& y = edges[n];
			L.emplace_back(SpillMovOp{y, n, sizes.at(n).at(y)});
		}

//		std::cout << "n to L: " << n.reg() << std::endl;

		if(edges.count(n)) {
			RegMemOp m = edges[n];
			S.insert(m);
			edges.erase(n);
		}

//		std::cout << "size of S at end of iteration: " << S.size() << std::endl;
	}

//	for(auto pair : L) {
//		std::cout << "move " << pair.first.reg() << " -> " << pair.second.reg() << std::endl;
//	}

	if(!edges.empty()) {
		return {};
	}

	return L;
}

void MachineCompiler::insertEdgeInstructions(
		std::map<u16, std::map<u16, std::vector<SpillMovOp>>>& edgeInstructions,
		std::map<u16, std::map<u16, std::vector<SpillMovOp>>>& sortedInstructions,
		u16 predecessor, u16 successor) {

	if(edgeInstructions[predecessor][successor].empty()) {
		return;
	}

	Logger::log(Topic::MACHINE) << "inserting " << edgeInstructions[predecessor][successor].size() <<
	          " moves for transition of block " << predecessor << " -> " << successor << std::endl;

	if(sortedInstructions[predecessor][successor].empty()) {

		// no correct topological sorting could be determined. We fall back to a push-pop
		// game where we push everything and then pop it back into the correct registers
		// todo: make it work when some operands are on the stack

		for(auto it = edgeInstructions[predecessor][successor].begin();
		        it != edgeInstructions[predecessor][successor].end();
		        ++it) {
			auto& pair = *it;
			if(!pair.first.isReg() || !pair.second.isReg()) {
				throw std::runtime_error("loops in edge instructions with memory operands are not supported yet");
			}
			builder.push(pair.first.reg());
		}

		for(auto it = edgeInstructions[predecessor][successor].rbegin();
		        it != edgeInstructions[predecessor][successor].rend();
		        ++it) {
			auto& pair = *it;
			builder.pop(pair.second.reg());
		}
	} else {
		// a topological sorting is possible. now move the operands according to this sorting
		for(auto pair : sortedInstructions[predecessor][successor]) {
			if(pair.first.isReg()) {
				// {reg, regmem}
				builder.mov(pair.first.reg(), pair.second, pair.size);
			} else if(pair.second.isReg()) {
				// {mem, reg}
				builder.mov(pair.first, pair.second.reg(), pair.size);
			} else {
				// {mem, mem}
				throw NotImplementedException();
			}
		}
	}

}

RegMemOp MachineCompiler::operandFor(u16 instructionId, lir::vr vr) {
	Interval const& i = intervalFor(instructionId, vr);

	if(i._reg != NONE) {
		return RegMemOp(i._reg);
	} else if(i._xmm != XMMNONE) {
		return RegMemOp(i._xmm);
	} else {
		return RegMemOp(stack.getAddressing(i.stack));
	}
}


}}
