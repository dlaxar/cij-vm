#pragma once

#include <types.hpp>
#include <bytecode.hpp>

using namespace am2017s;
using namespace bytecode;

namespace am2017s { namespace tests { namespace generate {

	Function buildFunction(std::vector<Local> const& params, std::vector<Type> const& temps, std::vector<Instruction> const& instrs);

	Instruction unary(Opcode opcode, u16 dstIdx, u16 srcIdx);

	Instruction binary(Opcode opcode, u16 dstIdx, u16 lsrcIdx, u16 rsrcIdx);

	Instruction load(u16 dstIdx, u16 srcIdx);

	Instruction store(u16 dstIdx, u16 srcIdx);

	Instruction return_(u16 srcIdx);

	Instruction const_(u16 dstIdx, Type type, i64 value);

	Type long_();

	Type array(Type t);

	Instruction arrayInstr(Opcode op, u16 dstIdx, u16 arrayIdx, u16 idxIdx, u16 valueIdx);

	Instruction load_idx(u16 dstIdx, u16 arrayIdx, u16 idxIdx);

	Instruction store_idx(u16 arrayIdx, u16 idxIdx, u16 valueIdx);

	Instruction length(u16 dstIdx, u16 arrayIdx);

	Instruction callInstr(Opcode opcode, bool special, u16 funcIdx, u16 dstIdx, std::vector<u16> const& args);

	Instruction call(u16 funcIdx, u16 dstIdx, std::vector<u16> const& args);

	Instruction call_void(u16 funcIdx, std::vector<u16> const& args);

	Instruction call_special(u16 funcIdx, std::vector<u16> const& args);

	Type int_();

	Type bool_();

	Instruction jumpInstr(Opcode opcode, u16 condIdx, u16 targetIdx);

	Instruction if_goto(u16 condIdx, u16 targetIdx);

	Instruction goto_(u16 targetIdx);

}}}