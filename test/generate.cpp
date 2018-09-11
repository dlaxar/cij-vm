#include "generate.hpp"

namespace am2017s { namespace tests { namespace generate {
	Instruction const_(u16 dstIdx, Type type, i64 value)
	{
		Instruction instr;
		instr.opcode = Opcode::CONST;
		instr.constant.dstIdx = dstIdx;
		instr.constant.type = type;
		instr.constant.value = value;
		return instr;
	}

	Function buildFunction(std::vector<Local> const& params, std::vector<Type> const& temps,
	                       std::vector<Instruction> const& instrs)
	{
		Function func;
		func.parameters = params;
		func.instructions = instrs;
		func.temporaryTypes = temps;
		return func;
	}

	Instruction unary(Opcode opcode, u16 dstIdx, u16 srcIdx)
	{
		Instruction instr(opcode);
		instr.unary.srcIdx = srcIdx;
		instr.unary.dstIdx = dstIdx;
		return instr;
	}

	Instruction binary(Opcode opcode, u16 dstIdx, u16 lsrcIdx, u16 rsrcIdx)
	{
		Instruction instr(opcode);
		instr.binary.dstIdx = dstIdx;
		instr.binary.lsrcIdx = lsrcIdx;
		instr.binary.rsrcIdx = rsrcIdx;
		return instr;
	}

	Instruction load(u16 dstIdx, u16 srcIdx)
	{
		return unary(Opcode::LOAD, dstIdx, srcIdx);
	}

	Instruction store(u16 dstIdx, u16 srcIdx)
	{
		return unary(Opcode::STORE, dstIdx, srcIdx);
	}

	Instruction return_(u16 srcIdx)
	{
		return unary(Opcode::RETURN, 42 /* unused */, srcIdx);
	}

	Type long_()
	{
		return {false, BaseType::INT64};
	}

	Type array(Type t)
	{
		return {true, t.baseType};
	}

	Instruction arrayInstr(Opcode op, u16 dstIdx, u16 arrayIdx, u16 idxIdx, u16 valueIdx)
	{
		Instruction instr;
		instr.opcode = op;
		instr.array.dstIdx = dstIdx;
		instr.array.memoryIdx = arrayIdx;
		instr.array.indexIdx = idxIdx;
		instr.array.valueIdx = valueIdx;
		return instr;
	}

	Instruction load_idx(u16 dstIdx, u16 arrayIdx, u16 idxIdx)
	{
		return arrayInstr(Opcode::LOAD_IDX, dstIdx, arrayIdx, idxIdx, 42 /* ignored */);
	}

	Instruction store_idx(u16 arrayIdx, u16 idxIdx, u16 valueIdx)
	{
		return arrayInstr(Opcode::STORE_IDX, 42 /* ignored */, arrayIdx, idxIdx, valueIdx);
	}

	Instruction length(u16 dstIdx, u16 arrayIdx)
	{
		return arrayInstr(Opcode::LENGTH, dstIdx, arrayIdx, 42 /* ignored */, 42 /* ignored */);
	}

	Instruction callInstr(Opcode opcode, u16 funcIdx, u16 dstIdx, std::vector<u16> const& args)
	{
		Instruction instr(opcode);
		instr.call.functionIdx = funcIdx;
		instr.call.dstIdx = dstIdx;
		instr.call.args = args;
		return instr;
	}

	Instruction call(u16 funcIdx, u16 dstIdx, std::vector<u16> const& args)
	{
		return callInstr(Opcode::CALL, funcIdx, dstIdx, args);
	}

	Instruction call_void(u16 funcIdx, std::vector<u16> const& args)
	{
		return callInstr(Opcode::CALL_VOID, funcIdx, 42 /* ignored */, args);
	}

	Instruction call_special(u16 funcIdx, std::vector<u16> const& args)
	{
		return callInstr(Opcode::SPECIAL_VOID, funcIdx, 42 /* ignored */, args);
	}

	Type int_()
	{
		return {false, BaseType::INT32};
	}

	Type bool_()
	{
		return {false, BaseType::BOOL};
	}

	Instruction jumpInstr(Opcode opcode, u16 condIdx, u16 targetIdx)
	{
		Instruction instr(opcode);
		instr.jump.conditionIdx = condIdx;
		instr.jump.branchIdx = targetIdx;
		return instr;
	}

	Instruction if_goto(u16 condIdx, u16 targetIdx)
	{
		return jumpInstr(Opcode::IF_GOTO, condIdx, targetIdx);
	}

	Instruction goto_(u16 targetIdx)
	{
		return jumpInstr(Opcode::GOTO, 42 /* ignored */, targetIdx);
	}
}}}