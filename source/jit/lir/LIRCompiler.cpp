#include <jit/lir/LIRCompiler.hpp>
#include <bytecode.hpp>

#include <log/Logger.hpp>
#include <jit/JitEngine.hpp>
#include <jit/SpecialFunctions.hpp>

#include <algorithm>
#include <iterator>

namespace am2017s {
namespace jit {

using lir::Operation;

template<class Architecture>
LIRCompiler<Architecture>::LIRCompiler(JitEngine* engine,
                                       am2017s::bytecode::Program const& program,
                                       std::map<u8, am2017s::bytecode::StructType>& _types,
                                       const am2017s::bytecode::Function& _function,
                                       std::vector<bool>& _skip)
		: engine(engine), program(program), types(_types), function(_function), skip(_skip) {}

template<class Architecture>
void LIRCompiler<Architecture>::analyseBlocks() {
	u16 numInstructions = 0;

	for (auto& blockInfo : function.blocks) {
		Block block(blockInfo, function, numInstructions, numInstructions + blockInfo.instructionCount - (u16) 1);
		numInstructions += blockInfo.instructionCount;
		block.index = (u16) blocks.size();
		blocks.push_back(block);
	}
}

template<class Architecture>
void LIRCompiler<Architecture>::run() {
	analyseBlocks();
	compileFunction();
}

template<class Architecture>
void LIRCompiler<Architecture>::compileFunction() {
	instructionCount = 0;

	std::vector<RegOp> const& parameters = Architecture::parameters();
	auto paramIterator = parameters.cbegin();

	for (u16 i = 0; i != function.parameters.size(); ++i) {
		if (paramIterator != parameters.cend()) {
			useParameter(vrForTemporary(i), true);
			++paramIterator;
		} else {
			useParameter(vrForTemporary(i), false);
		}
	}

	for (auto b = blocks.begin(); b != blocks.end(); ++b) {
		for (auto it = b->instructionBegin(); it != b->instructionEnd(); ++it) {
			// todo handle skips
			compileInstruction(*it, &instructionCount, b->lirs);
		}
	}

	// replace previously unknown VRs in phi nodes
	for (auto b = blocks.begin(); b != blocks.end(); ++b) {
		for (auto it = b->lirs.begin(); it != b->lirs.end(); ++it) {
			if (it->operation == lir::Operation::PHI) {
				for (auto& edge : it->phi.edges) {
					if (unknownToKnownVR.count(edge.vreg)) {
						lir::vr old = edge.vreg;
						edge.vreg = unknownToKnownVR[old];
						usages[edge.vreg].insert(usages[old].begin(), usages[old].end());
					}
				}
			}
		}
	}

	std::set<std::set<lir::vr>> newHintSame;
	// replace previously unknown VRs in hints
	for(auto setIterator = hintSame.begin(); setIterator != hintSame.end(); ++setIterator) {
		auto& sameSet = *setIterator;
		std::set<lir::vr> newSet;
		for(auto e : sameSet) {
			if(unknownToKnownVR.count(e)) {
				newSet.insert(unknownToKnownVR.at(e));
			} else {
				newSet.insert(e);
			}
		}

		newHintSame.insert(newSet);
	}

	hintSame = newHintSame;

	for (auto b = blocks.begin(); b != blocks.end(); ++b) {
		Logger::log(Topic::LIR_INSTRUCTIONS) << "-------- block " << b->index << std::endl;
		for (auto it = b->lirs.begin(); it != b->lirs.end(); ++it) {
			Logger::log(Topic::LIR_INSTRUCTIONS) << *it << std::endl;
		}
	}

}

template <class Architecture>
bool
LIRCompiler<Architecture>::isIntegerOp(bytecode::Instruction const& instruction) const {
	vector<u16> const& inputOperands = instruction.inputOperands();
	return std::all_of(inputOperands.begin(), inputOperands.end(), [=](u16 operand) { return function.temporaryTypes.at(operand).isInteger(); });
}

template<class Architecture>
void
LIRCompiler<Architecture>::compileInstruction(const bytecode::Instruction& instruction, u16* id,
                                              vector<lir::Instruction>& lirs) {
	using lir::Operation;

	lir::Instruction i{};

	// todo we randomly require registers (MUST) but for most instructions it would suffice if *either* operand is
	// in a register


	switch (instruction.opcode) {
	case bytecode::Opcode::NOP:
		break;

	case bytecode::Opcode::CONST: {
		i = {Operation::MOV, (*id)++};
		lir::vr vrImm;

		if(instruction.constant.type.isFloatingPoint()) {
			vrImm = vr(bytecode::BaseType::INT64);
		} else {
			vrImm = vrForTemporary(instruction.constant.dstIdx);
			i.mov.size = instruction.constant.type.size();
		}

		i.mov.dst = vrImm;
		i.mov.isImm = true;
		i.mov.imm = instruction.constant.value;
		// todo if type fits into 32bit we don't need a register
		use(i.mov.dst, id, true);

		lirs.push_back(i);

		if (instruction.constant.type.isFloatingPoint()) {
			i = {Operation::MOV_I2F, (*id)++};
			i.mov.isImm = false;
			i.mov.src = vrImm;
			use(i.mov.src, id, true);

			i.mov.dst = vrForTemporary(instruction.constant.dstIdx);
			use(i.mov.dst, id, true);

			i.mov.size = instruction.constant.type.size();

			lirs.push_back(i);
		}
	}
		break;

	case bytecode::Opcode::NEG:
		i = {Operation::MOV, (*id)++};
		i.mov.isImm = false;

		i.mov.src = vrForTemporary(instruction.unary.srcIdx);
		use(i.mov.src, id, false);
		i.mov.size = vrTypes.at(i.mov.src).size();

		i.mov.dst = vrForTemporary(instruction.unary.dstIdx);
		use(i.mov.dst, id, true);

		lirs.push_back(i);

		i = {Operation::NEG, (*id)++};
		i.unary.dst = vrForTemporary(instruction.unary.dstIdx);
		use(i.mov.dst, id, true);

		lirs.push_back(i);
		break;

	case bytecode::Opcode::ADD:
		// notice that this is not in SSA form mov.dst is modified by the add command
		// however since no block border is between those instructions it is fine :)
		if(isIntegerOp(instruction)) {
			i = {Operation::MOV, (*id)++};
			i.mov.isImm = false;

			i.mov.src = vrForTemporary(instruction.binary.lsrcIdx);
			use(i.mov.src, id, false);
			i.mov.size = vrTypes.at(i.mov.src).size();

			i.mov.dst = vrForTemporary(instruction.binary.dstIdx);
			use(i.mov.dst, id, true);

			lirs.push_back(i);

			i = {Operation::ADD, (*id)++};

			i.binary.dst = vrForTemporary(instruction.binary.dstIdx);
			use(i.binary.dst, id, true);

			i.binary.src = vrForTemporary(instruction.binary.rsrcIdx);
			use(i.binary.src, id, false);

			lirs.push_back(i);
		} else {
			i = {Operation::FMOV, (*id)++};
			i.mov.isImm = false;

			i.mov.src = vrForTemporary(instruction.binary.lsrcIdx);
			use(i.mov.src, id, false);
			i.mov.size = vrTypes.at(i.mov.src).size();

			i.mov.dst = vrForTemporary(instruction.binary.dstIdx);
			use(i.mov.dst, id, true);

			lirs.push_back(i);

			i = {Operation::FADD, (*id)++};

			i.binary.dst = vrForTemporary(instruction.binary.dstIdx);
			use(i.binary.dst, id, true);

			i.binary.src = vrForTemporary(instruction.binary.rsrcIdx);
			use(i.binary.src, id, false);

			lirs.push_back(i);
		}
		break;

	case bytecode::Opcode::SUB:
		i = {Operation::MOV, (*id)++};
		i.mov.isImm = false;

		i.mov.src = vrForTemporary(instruction.binary.lsrcIdx);
		use(i.mov.src, id, false);
		i.mov.size = vrTypes.at(i.mov.src).size();

		i.mov.dst = vrForTemporary(instruction.binary.dstIdx);
		use(i.mov.dst, id, true);

		lirs.push_back(i);

		i = lir::Instruction{Operation::SUB, (*id)++};
		i.binary.dst = vrForTemporary(instruction.binary.dstIdx);
		use(i.binary.dst, id, true);

		i.binary.src = vrForTemporary(instruction.binary.rsrcIdx);
		use(i.binary.src, id, false);

		lirs.push_back(i);

		break;
	case bytecode::Opcode::MUL:
		i = {Operation::MOV, (*id)++};
		i.mov.isImm = false;

		i.mov.src = vrForTemporary(instruction.binary.lsrcIdx);
		use(i.mov.src, id, false);
		i.mov.size = vrTypes.at(i.mov.src).size();

		i.mov.dst = vrForTemporary(instruction.binary.dstIdx);
		use(i.mov.dst, id, true);

		lirs.push_back(i);

		i = lir::Instruction{Operation::MUL, (*id)++};
		i.binary.dst = vrForTemporary(instruction.binary.dstIdx);
		use(i.binary.dst, id, true);

		i.binary.src = vrForTemporary(instruction.binary.rsrcIdx);
		use(i.binary.src, id, false);

		lirs.push_back(i);

		break;
	case bytecode::Opcode::DIV:
		if(vrTypes.at(vrForTemporary(instruction.binary.lsrcIdx)).isFloatingPoint()) {
			i = {Operation::MOV, (*id)++};
			i.mov.isImm = false;

			i.mov.src = vrForTemporary(instruction.binary.lsrcIdx);
			use(i.mov.src, id, true);
			i.mov.size = vrTypes.at(i.mov.src).size();

			i.mov.dst = vrForTemporary(instruction.binary.dstIdx);
			use(i.mov.dst, id, false);

			lirs.push_back(i);

			i = {Operation::DIV, (*id)++};
			i.ternary.dst = {vrForTemporary(instruction.binary.dstIdx)};
			use(vrForTemporary(instruction.binary.dstIdx), id, true);

			i.ternary.srcA = vrForTemporary(instruction.binary.dstIdx);
			use(i.ternary.srcA, id, true);

			i.ternary.srcB = vrForTemporary(instruction.binary.rsrcIdx);
			use(i.ternary.srcB, id, false);

			lirs.push_back(i);

			break;
		} else {
			// we can handle division and mod the same because on amd64 they are the same operation
			// which yields both results in different registers:
			// no break here
		}
	case bytecode::Opcode::MOD:

		i = {Operation::MOV, (*id)++};
		i.mov.isImm = false;

		i.mov.src = vrForTemporary(instruction.binary.lsrcIdx);
		use(i.mov.src, id, false);
		i.mov.size = vrTypes.at(i.mov.src).size();

		i.mov.dst = vrForFixed(RegOp::RAX);
		use(i.mov.dst, id, true);

		lirs.push_back(i);

		i = {Operation::CQO, (*id)++};
		i.binary.src = vrForFixed(RegOp::RAX);
		use(i.binary.src, id, true);

		i.binary.dst = vrForFixed(RegOp::RDX);
		use(i.binary.dst, id, true);

		lirs.push_back(i);

		i = {Operation::DIV, (*id)++};
		i.ternary.dst = {vrForFixed(RegOp::RAX)};
		use(vrForFixed(RegOp::RAX), id, true);

		i.ternary.srcA = vrForFixed(RegOp::RDX);
		use(vrForFixed(RegOp::RAX), id, true);

		i.ternary.srcB = vrForTemporary(instruction.binary.rsrcIdx);
		use(i.ternary.srcB, id, false);

		lirs.push_back(i);

		i = {Operation::MOV, (*id)++};
		i.mov.isImm = false;

		// This is the only thing that's different between div and mod. While the result of the
		// division is in RAX the remainder is located in RDX
		i.mov.src = vrForFixed((instruction.opcode == bytecode::Opcode::DIV) ? RegOp::RAX : RegOp::RDX);
		use(i.mov.src, id, true);
		i.mov.size = vrTypes.at(i.mov.src).size();

		i.mov.dst = vrForTemporary(instruction.binary.dstIdx);
		use(i.mov.dst, id, false);

		lirs.push_back(i);

		break;

	case bytecode::Opcode::GT:
		// We can handle all comparison operators the same since only the argument of SET
		// changes. We handle this later
	case bytecode::Opcode::GTE:
	case bytecode::Opcode::EQ:
	case bytecode::Opcode::NEQ:
	case bytecode::Opcode::LTE:
	case bytecode::Opcode::LT:
		i = {Operation::CMP, (*id)++};

		i.cmp.l = vrForTemporary(instruction.binary.lsrcIdx);
		use(i.cmp.l, id, true);

		i.cmp.r = vrForTemporary(instruction.binary.rsrcIdx);
		use(i.cmp.r, id, false);

		lirs.push_back(i);

		i = {Operation::SET, (*id)++};

		switch (instruction.opcode) {
		case bytecode::Opcode::GT:
			i.flag.mode = lir::FlagOpMode::GT;
			break;
		case bytecode::Opcode::GTE:
			i.flag.mode = lir::FlagOpMode::GTE;
			break;
		case bytecode::Opcode::EQ:
			i.flag.mode = lir::FlagOpMode::EQ;
			break;
		case bytecode::Opcode::NEQ:
			i.flag.mode = lir::FlagOpMode::NEQ;
			break;
		case bytecode::Opcode::LTE:
			i.flag.mode = lir::FlagOpMode::LTE;
			break;
		case bytecode::Opcode::LT:
			i.flag.mode = lir::FlagOpMode::LT;
			break;
		default:
			throw std::runtime_error("Invalid opcode in comparison operator switch");
		}

		i.flag.reg = vrForTemporary(instruction.binary.dstIdx);
		use(i.flag.reg, id, false);

		lirs.push_back(i);
		break;
	case bytecode::Opcode::NOT:
		i = {Operation::MOV, (*id)++};
		i.mov.isImm = false;

		i.mov.src = vrForTemporary(instruction.unary.srcIdx);
		use(i.mov.src, id, false);
		i.mov.size = vrTypes.at(i.mov.src).size();

		i.mov.dst = vrForTemporary(instruction.unary.dstIdx);
		use(i.mov.dst, id, true);

		lirs.push_back(i);

		i = {Operation::NOT, (*id)++};
		i.unary.dst = vrForTemporary(instruction.unary.dstIdx);
		use(i.mov.dst, id, true);

		lirs.push_back(i);
		break;
	case bytecode::Opcode::NEW: {
		i = {Operation::MOV, (*id)++};
		i.mov.isImm = true;
		i.mov.imm = instruction.alloc.type.size();

		i.mov.dst = vr({bytecode::BaseType::INT32});
		use(i.mov.dst, id, false);

		lirs.push_back(i);

		lir::Instruction movTypeId = {Operation::MOV, (*id)++};
		movTypeId.mov.isImm = true;
		movTypeId.mov.imm = instruction.alloc.type.baseType;

		movTypeId.mov.dst = vr({bytecode::BaseType::INT8});
		movTypeId.mov.size = BYTE;

		lirs.push_back(movTypeId);

		std::vector<lir::vr> arguments;
		arguments.push_back(i.mov.dst);
		arguments.push_back(movTypeId.mov.dst);
		arguments.push_back(vrForTemporary(instruction.alloc.sizeIdx));
		buildCall(lirs, JitEngine::specialFunctionIndex(SPECIAL_F_IDX_ALLOC_ARRAY), false, 0, id, arguments,
		          instruction.alloc.dstIdx);
	}

		break;
	case bytecode::Opcode::GOTO:
		i = {Operation::JMP, (*id)++};
		i.jump.target = instruction.jump.branchIdx;
		lirs.push_back(i);
		break;
	case bytecode::Opcode::IF_GOTO:
		i = {Operation::TEST, (*id)++};

		i.flag.reg = vrForTemporary(instruction.jump.conditionIdx);
		use(i.flag.reg, id, false);

		lirs.push_back(i);

		i = {Operation::JNZ, (*id)++};
		i.jump.target = instruction.jump.branchIdx;
		lirs.push_back(i);
		break;
	case bytecode::Opcode::LENGTH:
		i = {Operation::MOV_MEM, (*id)++};
		i.memmov.toMem = false;
		i.memmov.isIndexed = false;

		i.memmov.base = vrForTemporary(instruction.array.memoryIdx);
		use(i.memmov.base, id, true);

		i.memmov.a = vrForTemporary(instruction.array.valueIdx);
		use(i.memmov.a, id, true);

		i.memmov.offset = -4;
		i.memmov.size = DWORD;

		lirs.push_back(i);

		break;
	case bytecode::Opcode::PHI: {
		i = lir::Instruction{Operation::PHI, (*id)++};
		i.phi.dst = vrForTemporary(instruction.dstIdx().value());
		use(i.phi.dst, id, false);



		std::transform(instruction.phi.args.begin(), instruction.phi.args.end(), std::back_inserter(i.phi.edges),
		               [=](auto edge) {
			               return lir::PhiEdge{
					               /* vreg  */ vrForPossiblyUnknownTemporary(edge.temp),
					               /* block */ edge.block
			               };
		               });

		std::set<lir::vr> sames;

		std::transform(i.phi.edges.begin(), i.phi.edges.end(), std::inserter(sames, sames.end()), [](auto edge) {
			return edge.vreg;
		});

		sames.insert(i.phi.dst);

		hintSame.insert(sames);

		lirs.push_back(i);
	}
		break;

	case bytecode::Opcode::CALL_VOID:
	case bytecode::Opcode::CALL: {
		std::vector<lir::vr> vrArgs;
		transformArguments(vrArgs, instruction.call.args);
		u16 dstIdx = (instruction.opcode == bytecode::Opcode::CALL_VOID ? (u16) -1 : instruction.call.dstIdx);
		buildCall(lirs, instruction.call.functionIdx, false, 0, id, vrArgs, dstIdx);
	}
		break;

	case bytecode::Opcode::SPECIAL_VOID: {
		std::vector<lir::vr> vrArgs;
		transformArguments(vrArgs, instruction.call.args);
		buildCall(lirs,
		          JitEngine::specialFunctionIndex(
		                  bytecode::special::resolveSpecialBuiltinOpcodes((u8) instruction.call.functionIdx)),
		          false,
		          0,
		          id,
		          vrArgs,
		          (u16) -1);
	}
		break;
	case bytecode::Opcode::RETURN:
		if(function.returnType.isInteger()) {
			i = {Operation::MOV, (*id)++};

			i.mov.isImm = false;

			i.mov.src = vrForTemporary(instruction.unary.srcIdx);
			use(i.mov.src, id, false);
			i.mov.size = vrTypes.at(i.mov.src).size();

			i.mov.dst = vrForFixed(RegOp::RAX);
			use(i.mov.dst, id, true);

			lirs.push_back(i);

			i = {Operation::RET, (*id)++};
			lirs.push_back(i);

			use(vrForFixed(RegOp::RAX), id, true);
		} else {
			i = {Operation::MOV, (*id)++};

			i.mov.isImm = false;

			i.mov.src = vrForTemporary(instruction.unary.srcIdx);
			use(i.mov.src, id, false);
			i.mov.size = vrTypes.at(i.mov.src).size();

			i.mov.dst = vrForFixedXMM(XMMOp::XMM0);
			use(i.mov.dst, id, true);

			lirs.push_back(i);

			i = {Operation::RET, (*id)++};
			lirs.push_back(i);

			use(vrForFixedXMM(XMMOp::XMM0), id, true);
		}

		break;
	case bytecode::Opcode::RET_VOID:
		i = {Operation::RET, (*id)++};
		lirs.push_back(i);
		break;

	case bytecode::Opcode::ALLOCATE: {
		i = {Operation::MOV, (*id)++};
		i.mov.size = QWORD;
		i.mov.isImm = true;

		lir::vr sizeVR = vr({bytecode::BaseType::INT64});
		i.mov.dst = sizeVR;
		use(i.mov.dst, id, false);

		i.mov.imm = types.at(instruction.obj_alloc.typeId).getSize();

		lirs.push_back(i);

		std::vector<lir::vr> arguments;
		arguments.push_back(sizeVR);
		buildCall(lirs, JitEngine::specialFunctionIndex(SPECIAL_F_IDX_ALLOCATE), false, 0, id, arguments,
		          instruction.obj_alloc.dstIdx);

		i = {Operation::MOV, (*id)++};
		i.mov.size = QWORD;
		i.mov.isImm = true;
		i.mov.imm = (i64) types.at(instruction.obj_alloc.typeId).vTable.data();

		lir::vr vPTRVR = vr({bytecode::BaseType::INT64});
		i.mov.dst = vPTRVR;
		use(i.mov.dst, id, true);

		lirs.push_back(i);

		i = {Operation::MOV_MEM, (*id)++};

		i.memmov.toMem = true;
		i.memmov.isIndexed = false;
		i.memmov.size = QWORD;

		i.memmov.a = vPTRVR;
		use(i.memmov.a, id, true);

		i.memmov.base = vrForTemporary(instruction.obj_alloc.dstIdx);
		use(i.memmov.base, id, true);

		i.memmov.offset = 0;

		lirs.push_back(i);

	}
		break;

	case bytecode::Opcode::OBJ_LOAD:
	case bytecode::Opcode::OBJ_STORE: {
		i = {Operation::MOV_MEM, (*id)++};
		i.memmov.toMem = (instruction.opcode == bytecode::Opcode::OBJ_STORE);
		i.memmov.isIndexed = false;

		i.memmov.base = vrForTemporary(instruction.access.ptrIdx);
		use(i.memmov.base, id, true);

		types.at(instruction.access.typeId).getSize();

		i.memmov.offset = types.at(instruction.access.typeId).getOffset(instruction.access.fieldIdx);

		i.memmov.size = types.at(instruction.access.typeId).getFieldSize(instruction.access.fieldIdx);

		i.memmov.a = vrForTemporary(instruction.access.valueIdx);
		use(i.memmov.a, id, true);

		lirs.push_back(i);
	}
		break;

	case bytecode::Opcode::GLOB_LOAD:
	case bytecode::Opcode::GLOB_STORE: {
		// load address of `global` from memory
		lir::Instruction loadGlobalAddress{Operation::MOV_MEM, (*id)++};
		loadGlobalAddress.memmov.toMem = false;
		loadGlobalAddress.memmov.isIndexed = false;

		loadGlobalAddress.memmov.base = vrForFixed(RBP);
		use(loadGlobalAddress.memmov.base, id, true);

		loadGlobalAddress.memmov.offset = (i32) -16;
		loadGlobalAddress.memmov.size = QWORD;

		loadGlobalAddress.memmov.a = vr({bytecode::BaseType::INT64});
		use(loadGlobalAddress.memmov.a, id, true);

		lirs.push_back(loadGlobalAddress);

		lir::Instruction loadValue{Operation::MOV_MEM, (*id)++};
		loadValue.memmov.toMem = (instruction.opcode == bytecode::Opcode::GLOB_STORE);
		loadValue.memmov.isIndexed = false;

		loadValue.memmov.base = loadGlobalAddress.memmov.a;
		use(loadValue.memmov.base, id, true);

		loadValue.memmov.offset = program.globals[instruction.global.globalIdx].offset;
		loadValue.memmov.size = program.globals[instruction.global.globalIdx].getSize();

		loadValue.memmov.a = vrForTemporary(instruction.global.value);
		use(loadValue.memmov.a, id, true);

		lirs.push_back(loadValue);
	}
		break;

	case bytecode::Opcode::VOID_MEMBER_CALL:
	case bytecode::Opcode::MEMBER_CALL: {
		// mov vTableReg, QWORD PTR[thisreg+0]
		lir::Instruction loadVTable{Operation::MOV_MEM, (*id)++};

		loadVTable.memmov.toMem = false;
		loadVTable.memmov.isIndexed = false;

		loadVTable.memmov.base = vrForTemporary(instruction.member_call.ptrIdx);
		use(loadVTable.memmov.base, id, true);

		loadVTable.memmov.size = QWORD;
		loadVTable.memmov.offset = 0;

		loadVTable.memmov.a = vr({bytecode::BaseType::INT64});
		use(loadVTable.memmov.a, id, true);

		lirs.push_back(loadVTable);

		// mov globalFunctionIndexReg, QWORD PTR[vTableReg + 2 * functionIdx]
		lir::Instruction loadFunctionIndex{Operation::MOV_MEM, (*id)++};
		loadFunctionIndex.memmov.toMem = false;
		loadFunctionIndex.memmov.isIndexed = false;

		loadFunctionIndex.memmov.base = loadVTable.memmov.a;
		use(loadFunctionIndex.memmov.base, id, true);

		loadFunctionIndex.memmov.offset = instruction.member_call.functionIdx * 2;
		loadFunctionIndex.memmov.size = WORD;

		loadFunctionIndex.memmov.a = vrForFixed(RAX);
		use(loadFunctionIndex.memmov.a, id, true);

		lirs.push_back(loadFunctionIndex);

		std::vector<lir::vr> vrArgs;
		std::transform(instruction.member_call.args.begin(), instruction.member_call.args.end(),
		               std::back_inserter(vrArgs),
		               [=](u16 tmpIdx) {
			               return vrForTemporary(tmpIdx);
		               });

		u16 dstIdx = (instruction.opcode == bytecode::Opcode::VOID_MEMBER_CALL ? (u16) -1
		                                                                       : instruction.member_call.dstIdx);
		buildCall(lirs, 0, true, instruction.member_call.ptrIdx, id, vrArgs, dstIdx);
	}
		break;

	case bytecode::Opcode::LOAD_IDX:
	case bytecode::Opcode::STORE_IDX: {
		i = {Operation::MOV_MEM, (*id)++};
		i.memmov.toMem = instruction.opcode == bytecode::Opcode::STORE_IDX;

		i.memmov.base = vrForTemporary(instruction.array.memoryIdx);
		use(i.memmov.base, id, true);

		i.memmov.isIndexed = true;
		i.memmov.index = vrForTemporary(instruction.array.indexIdx);
		use(i.memmov.index, id, true);

		i.memmov.a = vrForTemporary(instruction.array.valueIdx);
		use(i.memmov.a, id, true);

		i.memmov.scale = vrTypes.at(i.memmov.a).size();
		i.memmov.size = vrTypes.at(i.memmov.a).size();

		i.memmov.offset = 0;

		lirs.push_back(i);
	}
		break;

	default:
		throw std::runtime_error("bytecode opcode not implemented " + std::to_string((u8) instruction.opcode));

	}
}

template<class Architecture>
void LIRCompiler<Architecture>::loadJitEngine(vector<lir::Instruction>& lirs,
                                              u16* id,
                                              std::vector<RegOp>::const_iterator& paramIterator,
                                              std::vector<lir::vr>& clearRegisters) {

	lir::Instruction movEngine{Operation::MOV_MEM, (*id)++};
	movEngine.memmov.toMem = false;
	movEngine.memmov.isIndexed = false;

	movEngine.memmov.base = vrForFixed(RegOp::RBP);
	use(movEngine.memmov.base, id, true);

	movEngine.memmov.offset = -8;

	movEngine.memmov.a = vrForFixed(*(paramIterator++));
	use(movEngine.memmov.a, id, true);

	movEngine.memmov.size = QWORD;

	// don't need to clear the register we put the JitEngine* in
	clearRegisters.erase(std::find(clearRegisters.begin(), clearRegisters.end(), movEngine.memmov.a));
	lirs.push_back(movEngine);
}

/**
 *
 * @tparam Architecture
 * @param lirs
 * @param fIdx
 * @param id
 * @param tmpArguments
 * @param dstIdxOrVoid (u16) -1 if void
 */
template<class Architecture>
void LIRCompiler<Architecture>::buildCall(vector<lir::Instruction>& lirs,
                                          i32 fIdx,
                                          bool isMember,
                                          u16 thisIdx,
                                          u16* id,
                                          std::vector<lir::vr>& tmpArguments,
                                          u16 dstIdxOrVoid) {

	std::vector<lir::vr> arguments;

	std::vector<RegOp> const& parameters = Architecture::parameters();
	std::vector<XMMOp> const& floatParameters = Architecture::parametersFloat();
	auto paramIterator = parameters.cbegin();
	auto paramIteratorFloat = floatParameters.cbegin();

	std::vector<lir::vr> clearRegisters;
	for (auto reg : Architecture::callerSaved()) {
		clearRegisters.push_back(vrForFixed(reg));
	}
	for(auto reg : Architecture::callerSavedFloat()) {
		clearRegisters.push_back(vrForFixedXMM(reg));
	}

	if (isMember) {
		// RAX contains our function index -> don't erase it
		clearRegisters.erase(std::find(clearRegisters.begin(), clearRegisters.end(), vrForFixed(RAX)));
	} else if (fIdx < 0) {
		loadJitEngine(lirs, id, paramIterator, clearRegisters);
	}

	u16 overflowArgument = 0;
	for (lir::vr const& arg : tmpArguments) {
		if(vrTypes.at(arg).isFloatingPoint() && paramIteratorFloat != floatParameters.cend()) {
			lir::Instruction moveToArgument{Operation::MOV, (*id)++};
			moveToArgument.mov.isImm = false;
			moveToArgument.mov.dst = vrForFixedXMM(*(paramIteratorFloat++));
			use(moveToArgument.mov.dst, id, true);
			moveToArgument.mov.src = arg;
			use(moveToArgument.mov.src, id, false);
			moveToArgument.mov.size = vrTypes.at(moveToArgument.mov.src).size();

			clearRegisters.erase(std::find(clearRegisters.begin(), clearRegisters.end(), moveToArgument.mov.dst));

			arguments.push_back(moveToArgument.mov.dst);
			lirs.push_back(moveToArgument);
		} else if (vrTypes.at(arg).isInteger() && paramIterator != parameters.cend()) {
			lir::Instruction moveToArgument{Operation::MOV, (*id)++};
			moveToArgument.mov.isImm = false;
			moveToArgument.mov.dst = vrForFixed(*(paramIterator++));
			use(moveToArgument.mov.dst, id, true);
			moveToArgument.mov.src = arg;
			use(moveToArgument.mov.src, id, false);
			moveToArgument.mov.size = vrTypes.at(moveToArgument.mov.src).size();

			clearRegisters.erase(std::find(clearRegisters.begin(), clearRegisters.end(), moveToArgument.mov.dst));

			arguments.push_back(moveToArgument.mov.dst);
			lirs.push_back(moveToArgument);
		} else {
			lir::Instruction moveToArgument{Operation::MOV, (*id)++};
			moveToArgument.mov.isImm = false;
			moveToArgument.mov.dst = vrForStackArgument(overflowArgument++, vrTypes.at(arg));
			use(moveToArgument.mov.dst, id, false);
			moveToArgument.mov.src = arg;
			use(moveToArgument.mov.src, id, true);
			moveToArgument.mov.size = QWORD;

			arguments.push_back(moveToArgument.mov.dst);
			lirs.push_back(moveToArgument);
		}
	}

	lir::Instruction call;

	std::vector<lir::vr>* passArgs;
	std::vector<lir::vr>* passClear;

	if (isMember) {
		call = {Operation::CALL_IDX_IN_REG, (*id)++};
		call.reg_call.isVoid = (dstIdxOrVoid == (u16) -1);

		call.reg_call.idxReg = vrForFixed(RAX);
		use(call.reg_call.idxReg, id, true);

		if (!call.reg_call.isVoid) {
			lir::vr dstVr = vrForTemporary(dstIdxOrVoid);
			if(vrTypes.at(dstVr).isFloatingPoint()) {
				call.reg_call.dst = vrForFixedXMM(XMMOp::XMM0);
			} else {
				call.reg_call.dst = vrForFixed(RegOp::RAX);
			}

			use(call.reg_call.dst, id, true);
		}

		passArgs = &call.reg_call.args;
		passClear = &call.reg_call.clears;
	} else {
		call = {Operation::CALL, (*id)++};
		call.call.isVoid = (dstIdxOrVoid == (u16) -1);
		call.call.function = fIdx;

		if(call.call.isVoid) {
			/* ignore */
		} else {
			lir::vr dstVr = vrForTemporary(dstIdxOrVoid);
			if(vrTypes.at(dstVr).isFloatingPoint()) {
				call.call.dst = vrForFixedXMM(XMMOp::XMM0);
			} else {
				call.call.dst = vrForFixed(RegOp::RAX);
			}

			use(call.call.dst, id, true);
		}

		passArgs = &call.call.args;
		passClear = &call.call.clears;
	}

	u16 argIndex = 0;
	for (auto arg : arguments) {
		passArgs->push_back(arg);
		use(arg, id, argIndex++ < parameters.size());
	}

	for (auto saved : clearRegisters) {
		use(saved, id, true);
		passClear->push_back(saved);
	}

	lirs.push_back(call);

	if (dstIdxOrVoid != (u16) -1) {
		lir::vr dstVr = vrForTemporary(dstIdxOrVoid);
		lir::vr returnVr;

		if(vrTypes.at(dstVr).isFloatingPoint()) {
			returnVr = vrForFixedXMM(XMM0);
		} else {
			returnVr = vrForFixed(RegOp::RAX);
		}

		lir::Instruction saveResult{Operation::MOV, (*id)++};
		saveResult.mov.isImm = false;

		saveResult.mov.src = returnVr;
		use(returnVr, id, true);
		saveResult.mov.size = vrTypes.at(saveResult.mov.src).size();

		saveResult.mov.dst = dstVr;
		use(saveResult.mov.dst, id, false);
		lirs.push_back(saveResult);
	}
}

template<class Architecture>
void LIRCompiler<Architecture>::transformArguments(std::vector<lir::vr>& into, std::vector<u16> const& from) {
	std::transform(from.begin(), from.end(), std::back_inserter(into),
	               [=](u16 tmpIdx) {
		               return vrForTemporary(tmpIdx);
	               });
}

template<class Architecture>
lir::vr LIRCompiler<Architecture>::vrForTemporary(u16 temporary) {
	// we haven't seen this before
	if (!temporaryToVR.count(temporary)) {
		temporaryToVR[temporary] = nextVR++;
		vrTypes[temporaryToVR[temporary]] = function.temporaryTypes.at(temporary);
	}

	// we have seen this before in a phi node, which means we need to
	// assign the actual vr number here and add a mapping
	if (temporaryToVR[temporary] > nextVR) {
		lir::vr old = temporaryToVR[temporary], n = nextVR++;
		unknownToKnownVR[old] = n;
		temporaryToVR[temporary] = n;
		vrTypes[n] = function.temporaryTypes.at(temporary);
	}

	return temporaryToVR[temporary];
}

template<class Architecture>
lir::vr LIRCompiler<Architecture>::vrForPossiblyUnknownTemporary(u16 temporary) {
	if (temporaryToVR.count(temporary)) {
		// temporary appeared before
		return vrForTemporary(temporary);
	} else {
		// temporary will appear later (we are in a phi node)
		return temporaryToVR[temporary] = nextUnknownVR--;
	}
}

template<class Architecture>
void LIRCompiler<Architecture>::use(lir::vr vr, u16* id, bool mustHaveReg) {
	usages[vr].insert({(u16) (*id - 1), {mustHaveReg}});
}

template<class Architecture>
void LIRCompiler<Architecture>::useParameter(lir::vr vr, bool mustHaveReg) {
	usages[vr].insert({(i32) -1, {mustHaveReg}});
}

template<class Architecture>
lir::vr LIRCompiler<Architecture>::vrForFixed(RegOp reg) {
	if (!fixedToVR.count(reg)) {
		fixedToVR[reg] = nextVR++;
		vrTypes[fixedToVR[reg]] = {bytecode::BaseType::INT64};
	}

	return fixedToVR[reg];
}

template<class Architecture>
lir::vr LIRCompiler<Architecture>::vrForFixedXMM(XMMOp reg) {
	if (!fixedXMMToVR.count(reg)) {
		fixedXMMToVR[reg] = nextVR++;
		vrTypes[fixedXMMToVR[reg]] = {bytecode::BaseType::FLP64};
	}

	return fixedXMMToVR[reg];
}

template<class Architecture>
lir::vr LIRCompiler<Architecture>::vrForStackArgument(u16 overflowCount, bytecode::Type type) {
	if (!overflowArgToVR.count(overflowCount)) {
		overflowArgToVR[overflowCount] = nextVR++;
		vrTypes[overflowArgToVR[overflowCount]] = type;
	}

	return overflowArgToVR[overflowCount];
}

template<class Architecture>
lir::vr LIRCompiler<Architecture>::vr(bytecode::Type type) {
	lir::vr vr = nextVR++;
	vrTypes[vr] = type;
	return vr;
}

template<class Architecture>
u16 LIRCompiler<Architecture>::numberOfLIRs() {
	return nextVR;
}

}
}
