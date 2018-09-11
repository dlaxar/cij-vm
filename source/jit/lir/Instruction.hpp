#pragma once

#include <types.hpp>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <exception/InvalidResultException.hpp>
#include <jit/Operands.hpp>

namespace am2017s { namespace jit { namespace lir {

enum Operation {
	MOV,
	PHI,
	CMP,
	SET,
	NOT,
	NEG,
	TEST,
	JMP,
	JNZ,
	ADD,
	SUB,
	MUL,
	DIV,
	RET,
	CQO,
	CALL,
	ALLOC,
	MOV_MEM,
	CALL_IDX_IN_REG,

	FMOV,
	FADD,

	MOV_I2F,

	NOP
};

static std::string opToString(Operation op) {
	switch(op) {
		case MOV: return "mov";
		case PHI: return "phi";
		case CMP: return "cmp";
		case SET: return "set";
		case NOT: return "not";
		case NEG: return "neg";
		case TEST: return "test";
		case ADD: return "add";
		case SUB: return "sub";
		case MUL: return "mul";
		case DIV: return "div";
		case JMP: return "jmp";
		case JNZ: return "jnz";
		case RET: return "ret";
		case CQO: return "cqo";
		case CALL: return "call";
		case ALLOC: return "alloc";
		case MOV_MEM: return "mov";
		case CALL_IDX_IN_REG: return "call";

		case FMOV: return "fmov";
		case FADD: return "fadd";

		case MOV_I2F: return "mov2f";

		case NOP: return "[invalid]";
	}

	return "[invalid]";
}

using vr = u16;

struct MovOp {
	bool isImm;
	i64 imm; // we need to use the biggest possible type here
	vr src;
	vr dst;

	OperandSize size;

	friend std::ostream& operator<<(std::ostream& os, const MovOp& obj)
	{
		if(obj.isImm) {
			return os << "i" << obj.dst << ", " << "$" << obj.imm;
		} else {
			return os << "i" << obj.dst << ", " << "i" << obj.src;
		}
	}
};

struct MovMemOp {
	vr a;

	vr base;
	i32 offset;

	bool isIndexed;
	vr index;
	u8 scale;

	OperandSize size;

	bool toMem;

	void printPtr(std::ostream& os) const {
		os << "PTR[i" << base << " + ";
		if(isIndexed) {
			os << "(i" << index << " * $" << std::to_string(scale) << ") + ";
		}
		os << "$" << offset << "]";
	}

	friend std::ostream& operator<<(std::ostream& os, const MovMemOp& obj) {
		if(obj.toMem) {
			obj.printPtr(os);
			return os << ", i" << obj.a;
		} else {
			os << "i" << obj.a  << ", ";
			obj.printPtr(os);
			return os;
		}
	}
};

struct PhiEdge {
	vr vreg;
	u16 block;
};

struct PhiOp {
	vr dst;
	std::vector<PhiEdge> edges;

	vr inputOf(u16 block) const {
		for(auto edge : edges) {
			if(edge.block == block) {
				return edge.vreg;
			}
		}

		throw InvalidResultException();
	}

	friend std::ostream& operator<<(std::ostream& os, const PhiOp& obj)
	{
		os << obj.dst << " = (";
		for(auto edge : obj.edges) {
			os << edge.vreg << " from " << edge.block << ", ";
		}
		return os << ")";
	}
};

struct CmpOp {
	vr l, r;

	friend std::ostream& operator<<(std::ostream& os, const CmpOp& obj)
	{
		return os << "i" << obj.l << ", i" << obj.r;
	}
};

enum FlagOpMode {
	LT, LTE, EQ, NEQ, GTE, GT
};

struct FlagOp {
	vr reg;
	FlagOpMode mode;

	friend std::ostream& operator<<(std::ostream& os, const FlagOp& obj)
	{
		return os << "i" << obj.reg;
	}
};

struct UnaryOp {
	vr dst;

	friend std::ostream& operator<<(std::ostream& os, const UnaryOp& obj)
	{
		return os << "i" << obj.dst;
	}
};

struct BinaryOp {
	vr dst;
	vr src;

	friend std::ostream& operator<<(std::ostream& os, const BinaryOp& obj)
	{
		return os << "i" << obj.dst << ", i" << obj.src;
	}
};

struct TernaryOp {
	std::vector<vr> dst;
	vr srcA;
	vr srcB;

	friend std::ostream& operator<<(std::ostream& os, const TernaryOp& obj)
	{
		os << "{";
		for(auto dst : obj.dst) {
			os << "i" << dst << " ";
		}
		return os << "}" << ", i" << obj.srcA << ", i" << obj.srcB;
	}
};

struct JumpOp {
	u16 target;

	friend std::ostream& operator<<(std::ostream& os, const JumpOp& obj)
	{
		return os << "block " << obj.target;
	}
};

struct CallOp {
	bool isVoid;
	vr dst;
	std::vector<vr> args;
	std::vector<vr> clears;
	i32 function;

	friend std::ostream& operator<<(std::ostream& os, const CallOp& obj)
	{
		os << obj.function << " i" << obj.dst << " = (";
		if(obj.function < 0) {
			os << "JitEngine* ";
		}
		for(auto arg : obj.args) {
			os << "i" << arg << " ";
		}
		return os << ")";
	}

};

struct RegCallOp {
	bool isVoid;
	vr dst;
	std::vector<vr> args;
	std::vector<vr> clears;
	vr idxReg;

	friend std::ostream& operator<<(std::ostream& os, const RegCallOp& obj) {
		os << "(reg i" << obj.idxReg << ") i" << obj.dst << " = (";
		for(auto arg : obj.args) {
			os << "i" << arg << " ";
		}
		return os << ")";
	}
};

struct AllocOp {
	vr dst;
	u16 bytes;

	friend std::ostream& operator<<(std::ostream& os, const AllocOp& obj)
	{
		return os << "i" << obj.dst << ", size:" << obj.bytes;
	}
};

struct Instruction {

	Operation operation;
	u16 id;

	union {
		MovOp mov;
		PhiOp phi;
		CmpOp cmp;
		FlagOp flag;
		UnaryOp unary;
		BinaryOp binary;
		TernaryOp ternary;
		JumpOp jump;
		CallOp call;
		AllocOp alloc;
		MovMemOp memmov;
		RegCallOp reg_call;
	};

	Instruction() {
		operation = NOP;
	}
	Instruction(Operation op, u16 _id) : operation(op), id(_id) {
		if(op == PHI) {
			new(&phi) PhiOp;
		}

		if(op == DIV) {
			new(&ternary) TernaryOp;
		}

		if(op == CALL) {
			new(&call) CallOp;
		}

		if(op == CALL_IDX_IN_REG) {
			new(&reg_call) RegCallOp;
		}
	}

	Instruction(Instruction const& old) {
		operation = old.operation;
		id = old.id;
		switch(operation) {
			case MOV:
			case FMOV:
			case MOV_I2F: new(&mov) MovOp(old.mov); break;
			case PHI: new(&phi) PhiOp(old.phi); break;
			case CMP: new(&cmp) CmpOp(old.cmp); break;
			case TEST:
			case SET: new(&flag) FlagOp(old.flag); break;
			case MUL:
			case CQO:
			case SUB:
			case ADD:
			case FADD: new(&binary) BinaryOp(old.binary); break;
			case DIV: new(&ternary) TernaryOp(old.ternary); break;
			case NEG:
			case NOT: new(&unary) UnaryOp(old.unary); break;
			case JMP:
			case JNZ: new(&jump) JumpOp(old.jump); break;
			case CALL: new(&call) CallOp(old.call); break;
			case ALLOC: new(&alloc) AllocOp(old.alloc); break;
			case MOV_MEM: new(&memmov) MovMemOp(old.memmov); break;
			case CALL_IDX_IN_REG: new(&reg_call) RegCallOp(old.reg_call); break;
			case RET: break;
			case NOP: break;
		}
	}
	~Instruction() {
		if(operation == PHI) {
			phi.~PhiOp();
		}

		if(operation == DIV) {
			ternary.~TernaryOp();
		}

		if(operation == CALL) {
			call.~CallOp();
		}

		if(operation == CALL_IDX_IN_REG) {
			reg_call.~RegCallOp();
		}
	}
	Instruction& operator = (Instruction const& other) {
		if(this == &other) {
			return *this;
		}

		this->~Instruction();
		return *new(this) Instruction(other);
	}

	std::vector<vr> dst() const {
		switch(operation) {
			case MOV:
			case FMOV:
			case MOV_I2F: return {mov.dst};
			case PHI: return {phi.dst};
			case CMP: return {};
			case TEST: return {};
			case SET: return {flag.reg};
			case MUL: return {binary.dst};
			case CQO:
			case SUB:
			case ADD:
			case FADD: return {binary.dst};
			case DIV: return ternary.dst;
			case NEG:
			case NOT: return {unary.dst};
			case JMP:
			case JNZ: return {};
			case RET: return {};
			case CALL: if(call.isVoid) { return {}; } else { return {call.dst}; };
			case ALLOC: return {alloc.dst};
			case MOV_MEM:
				if(memmov.toMem) { return {}; } else { return {memmov.a}; }
			case CALL_IDX_IN_REG: if(reg_call.isVoid) { return {}; } else { return {reg_call.dst}; }
			default:
				throw InvalidResultException();
		}
	}

	std::vector<vr> inputs() const {
		std::vector<vr> input;
		switch(operation) {
			case MOV:
			case FMOV:
			case MOV_I2F:
				if(mov.isImm) return {};
				else          return {mov.src};
			case PHI:
				for(auto& edge : phi.edges) {
					input.push_back(edge.vreg);
				}
				return input;
			case CMP:
				return {cmp.l, cmp.r};
			case TEST:
				return {flag.reg};
			case SET:
				return {};
			case NEG:
			case NOT:
				return {unary.dst};
			case CQO:
				return {binary.src};
			case DIV:
				return {ternary.srcA, ternary.srcB};
			case MUL:
			case SUB:
			case ADD:
			case FADD:
				return {binary.src, binary.dst};
			case JMP:
			case JNZ:
				return {};
			case RET:
				return {};
			case CALL:
				return call.args;
			case ALLOC:
				return {};
			case MOV_MEM:
				if(memmov.isIndexed) {
					input.push_back(memmov.index);
				}

				input.push_back(memmov.base);

				if(memmov.toMem) {
					input.push_back(memmov.a);
				}

				return input;

			case CALL_IDX_IN_REG:
				return reg_call.args;
			default:
				std::cerr << "Fallthrough in lir::Instruction.input()" << std::endl;
				throw InvalidResultException();
		}
	}

	std::vector<vr> clears() const {
		switch(operation) {
			case CALL:
				return call.clears;
			case CALL_IDX_IN_REG:
				return call.clears;
			default:
				return {};
		}
	}

	friend std::ostream& operator<<(std::ostream& os, const Instruction& that)
	{
		auto& s = os << "(" << std::setw(3) << that.id << ") " << opToString(that.operation) << " ";

		switch(that.operation) {
			case MOV:
			case FMOV:
			case MOV_I2F:
				return s << that.mov;
			case PHI:
				return s << that.phi;
			case CMP:
				return s << that.cmp;
			case TEST:
			case SET:
				return s << that.flag;
			case NEG:
			case NOT:
				return s << that.unary;
			case MUL:
			case SUB:
			case ADD:
			case FADD:
				return s << that.binary;
			case DIV:
				return s << that.ternary;
			case JMP:
			case JNZ:
				return s << that.jump;
			case RET:
				break;
			case CQO:
				return s;
			case CALL:
				return s << that.call;
			case ALLOC:
				return s << that.alloc;
			case MOV_MEM:
				return s << that.memmov;
			case CALL_IDX_IN_REG:
				return s << that.reg_call;
			default:
				throw InvalidResultException();
		}

		return s;
	}

};

struct Usage {
	bool mustHaveReg;
};

using UsageMap = std::map<lir::vr, std::map<i32, lir::Usage>>;

}}}