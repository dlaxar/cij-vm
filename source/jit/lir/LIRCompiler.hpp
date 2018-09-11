#pragma once

#include <jit/lifetime/LifetimeAnalyzer.hpp>
#include <jit/lir/Instruction.hpp>
#include <jit/architecture/Architecture.hpp>
#include <bytecode.hpp>
#include <map>
#include <jit/JitEngine.hpp>

namespace am2017s { namespace jit {

template <class Architecture>
class LIRCompiler {
private:
	JitEngine* engine;
	am2017s::bytecode::Program const& program;
	std::map<u8, am2017s::bytecode::StructType>& types;
	bytecode::Function const& function;
	std::vector<bool>& skip;

	lir::vr nextVR = 0;
	lir::vr nextUnknownVR = (lir::vr)-1;
	std::map<u16, lir::vr> temporaryToVR;
	std::map<lir::vr, lir::vr> unknownToKnownVR;

public:
	u16 instructionCount;

	lir::UsageMap usages;
	std::map<RegOp, lir::vr> fixedToVR;
	std::map<XMMOp, lir::vr> fixedXMMToVR;
	std::map<u16, lir::vr> overflowArgToVR;
	std::map<lir::vr, bytecode::Type> vrTypes;

	std::set<std::set<lir::vr>> hintSame;

private:
	void analyseBlocks();

	void use(lir::vr vr, u16* id, bool mustHaveReg);
	void useParameter(lir::vr vr, bool mustHaveReg);

	void loadJitEngine(vector<lir::Instruction>& lirs,
	                   u16* id,
	                   std::vector<RegOp>::const_iterator& paramIterator,
	                   std::vector<lir::vr>& clearRegisters);

	void buildCall(vector<lir::Instruction>& lirs,
	               i32 fIdx, bool isMember,
	               u16 thisIdx,
	               u16* id, std::vector<lir::vr>& tmpArguments,
	               u16 dstIdxOrVoid);

	void transformArguments(std::vector<lir::vr>& into, std::vector<u16> const& from);

	/**
	 * Returns true iff all inputs of this instructions are integers
	 * @param instruction
	 * @return
	 */
	bool isIntegerOp(bytecode::Instruction const& instruction) const;

public:

	LIRCompiler(JitEngine* engine,
	            am2017s::bytecode::Program const& program,
	            std::map<u8, am2017s::bytecode::StructType>& _types,
	            const am2017s::bytecode::Function& _function,
	            std::vector<bool>& _skip);

	void run();

	void compileFunction();

	void compileInstruction(const bytecode::Instruction& instruction, u16* id, vector<lir::Instruction>& lirs);

	lir::vr vrForTemporary(u16 temporary);

	lir::vr vrForPossiblyUnknownTemporary(u16 temp);

	std::vector<jit::Block> blocks;

	lir::vr vr(bytecode::Type type);
	lir::vr vrForFixed(RegOp reg);
	lir::vr vrForFixedXMM(XMMOp reg);
	lir::vr vrForStackArgument(u16 overflowCount, bytecode::Type type);

	u16 numberOfLIRs();

};

#ifdef __amd64__
template class LIRCompiler<AMD64>;
#endif

#ifdef TESTING
#include <jit/allocator/TwoRegArchitecture.hpp>
template class RegisterAllocation<TwoRegArchitecture>;
#endif

}}
