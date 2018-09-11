#pragma once

#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>

#include <types.hpp>
#include <exception/InvalidResultException.hpp>
#include <exception/NotImplementedException.hpp>
#include <exception/TypeNotPackedException.hpp>

#include <jit/Operands.hpp>

#define BINARY_INSTRUCTIONS \
case Opcode::ADD:\
case Opcode::SUB:\
case Opcode::MUL:\
case Opcode::DIV:\
case Opcode::MOD:\
case Opcode::GT:\
case Opcode::GTE:\
case Opcode::EQ:\
case Opcode::NEQ:\
case Opcode::LTE:\
case Opcode::LT:\
case Opcode::AND:\
case Opcode::OR:

#define UNARY_INSTRUCTIONS \
case Opcode::STORE:\
case Opcode::LOAD:\
case Opcode::NEG:\
case Opcode::NOT:\
case Opcode::RETURN:

#define CONST_INSTRUCTIONS \
case Opcode::CONST:

#define ARRAY_INSTRUCTIONS \
case Opcode::LOAD_IDX:\
case Opcode::STORE_IDX:\
case Opcode::LENGTH:

#define ALLOCATE_INSTRUCTIONS \
case Opcode::NEW:

#define OBJ_ALLOCATE_INSTRUCTIONS \
case Opcode::ALLOCATE:

#define OBJ_ACCESS_INSTRUCTIONS \
case Opcode::OBJ_LOAD: \
case Opcode::OBJ_STORE:

#define GLOB_ACCESS_INSTRUCTIONS \
case Opcode::GLOB_LOAD: \
case Opcode::GLOB_STORE:

#define JUMP_INSTRUCTIONS \
case Opcode::GOTO:\
case Opcode::IF_GOTO:

#define CALL_INSTRUCTIONS \
case Opcode::CALL:\
case Opcode::SPECIAL:\
case Opcode::CALL_VOID:\
case Opcode::SPECIAL_VOID:\

#define MEMBER_CALL_INSTRUCTINS \
case Opcode::VOID_MEMBER_CALL:\
case Opcode::MEMBER_CALL:

#define OPCODE_INSTRUCTIONS \
case Opcode::RET_VOID:

#define PHI_INSTRUCTIONS \
case Opcode::PHI:

namespace am2017s { namespace bytecode
{
	using am2017s::jit::OperandSize;

	struct BytecodeLoaderException : std::runtime_error
	{
		BytecodeLoaderException(std::string const& what)
			: runtime_error(what)
		{}
	};

	enum struct BaseType : u8
	{
		VOID  = 0,
		BOOL  = 1,
		INT8  = 2,
		CHAR  = 3,
		INT16 = 4,
		INT32 = 5,
		INT64 = 6,
		FLP32 = 7,
		FLP64 = 8
	};

	struct Type
	{
		bool isArray;
		u8 baseType;

		Type(bool isArray, u8 baseType) : isArray(isArray), baseType(baseType) {}

		Type(u8 baseType) : isArray(false), baseType(baseType) {}

		Type(BaseType baseType) : isArray(false), baseType((u8) baseType) {}

		Type() : isArray(false), baseType(0) {}

		bool isFloatingPoint() const {
			return !isArray && (baseType == 7 || baseType == 8);
		}

		bool isInteger() const {
			return baseType <= 6 || baseType >= 9;
		}

		bool operator==(const Type& other)
		{
			return isArray == other.isArray &&
			       (baseType == other.baseType || baseType == (u8) BaseType::VOID || other.baseType == (u8) BaseType::VOID);
		}

		OperandSize size() const {
			if(isArray) {
				// reference to array is a pointer
				return OperandSize::QWORD;
			}

			switch((BaseType) baseType) {
			case BaseType::VOID: return OperandSize::QWORD; // used for null constants
			case BaseType::BOOL:
			case BaseType::INT8: return OperandSize::BYTE;
			case BaseType::CHAR:
			case BaseType::INT16: return OperandSize::WORD;
			case BaseType::INT32:
			case BaseType::FLP32: return OperandSize::DWORD;
			case BaseType::INT64:
			case BaseType::FLP64: return OperandSize::QWORD;
			default:
				if(baseType >= 9) {
					// ptr type
					return OperandSize::QWORD;
				}

				throw std::runtime_error("size not available for given basetype");
			}
		};
	};

	struct Local
	{
		Type type;
		std::string name;
	};

	struct Block {
		u16 instructionCount;
		std::vector<u16> successors;
		std::vector<u16> predecessors;
	};

	enum struct Opcode : u8
	{
		NOP       =   0,
		LOAD      =   1,
		STORE     =   2,
		LOAD_IDX  = 129,
		STORE_IDX = 130,

		CONST     =   3,

		ADD       =   4,
		SUB       =   5,
		MUL       =   6,
		DIV       =   7,
		MOD       =   8,

		NEG       =   9,

		GT        =  10,
		GTE       =  11,
		EQ        =  12,
		NEQ       =  13,
		LTE       =  14,
		LT        =  15,

		AND       =  18,
		OR        =  19,
		NOT       =  20,

		NEW       =  21,
		GOTO      =  22,
		IF_GOTO   =  23,

		LENGTH    =  25,
		PHI       =  26,

		CALL      =  28,
		SPECIAL   =  29,
		CALL_VOID =  30,
		SPECIAL_VOID = 31,
		RET_VOID  =  32,
		RETURN    =  33,

		ALLOCATE  = 100,
		OBJ_LOAD  = 101,
		OBJ_STORE = 102,
		GLOB_LOAD = 103,
		GLOB_STORE = 104,

		VOID_MEMBER_CALL = 105,
		MEMBER_CALL = 106
	};

	struct UnaryOp
	{
		u16 dstIdx;
		u16 srcIdx;
	};

	struct BinaryOp
	{
		u16 dstIdx;
		u16 lsrcIdx;
		u16 rsrcIdx;
	};

	struct ConstOp
	{
		u16 dstIdx;
		Type type;
		i64 value;
	};

	struct ArrayOp
	{
		/**
		 * @brief index of temporary, that points to an array
		 */
		u16 memoryIdx;

		/**
		 * @brief index of temporary, that point to the array index
		 */
		u16 indexIdx;

		/**
		 * @brief index of temporary, that holds the value (store instruction) or the dstIdx (load instruction)
		 */
		u16 valueIdx;
	};

	struct NewOp
	{
		u16 dstIdx;
		Type type;
		u16 sizeIdx;
	};

	struct GotoOp
	{
		u16 branchIdx;
		u16 conditionIdx;
	};

	struct CallOp
	{
		u16 dstIdx;
		u16 functionIdx;
		std::vector<u16> args;
	};

	struct MemberCallOp
	{
		u16 dstIdx;
		u16 ptrIdx;
		u8 functionIdx;
		std::vector<u16> args;
	};

	struct PhiEdge {
		u16 temp;
		u16 block;
	};

	struct PhiOp
	{
		u16 dstIdx;
		std::vector<PhiEdge> args;

	public:
		u16 inputOf(u16 block) const {
			for(auto edge : args) {
				if(edge.block == block) {
					return edge.temp;
				}
			}

			throw InvalidResultException();
		}
	};

	struct AllocOp {
		u16 dstIdx;
		u8 typeId;
	};

	struct AccessOp {
		u16 ptrIdx;
		u8 typeId;
		u8 fieldIdx;
		u16 valueIdx;
	};

	struct GlobalAccessOp {
		u16 globalIdx;
		u16 value;
	};

	enum SpecialCallIdx
	{
		BEGIN = 0,
		END = 1,
		PRINTLN = 2,
		EQ = 3,
	};

	struct Instruction
	{
	public:
		Opcode opcode;
		u16 id;

		union
		{
			UnaryOp unary;
			BinaryOp binary;
			ConstOp constant;
			ArrayOp array;
			NewOp alloc;
			GotoOp jump;
			CallOp call;
			MemberCallOp member_call;
			PhiOp phi;
			AllocOp obj_alloc;
			AccessOp access;
			GlobalAccessOp global;
		};

		bool isPure() const {
			// todo implement
			return true;
		}

		std::vector<u16> inputOperands() const {
			std::vector<u16> inputOperands;
			switch(opcode) {
				case Opcode::NOP:
					break;

				BINARY_INSTRUCTIONS
					inputOperands.push_back(binary.lsrcIdx);
					inputOperands.push_back(binary.rsrcIdx);
					break;

				UNARY_INSTRUCTIONS
					inputOperands.push_back(unary.srcIdx);
					break;

				CONST_INSTRUCTIONS
					break;

				case Opcode::STORE_IDX:
					inputOperands.push_back(array.valueIdx);
				case Opcode::LOAD_IDX:
					inputOperands.push_back(array.indexIdx);
				case Opcode::LENGTH:
					inputOperands.push_back(array.memoryIdx);
					break;

				ALLOCATE_INSTRUCTIONS
					inputOperands.push_back(alloc.sizeIdx);
					break;

				OBJ_ALLOCATE_INSTRUCTIONS
					break;

				case Opcode::OBJ_LOAD:
					inputOperands.push_back(access.ptrIdx);;

				case Opcode::OBJ_STORE:
					inputOperands.push_back(access.ptrIdx);
					inputOperands.push_back(access.valueIdx);
					break;

				case Opcode::GOTO:
					break;
				case Opcode::IF_GOTO:
					inputOperands.push_back(jump.conditionIdx);
					break;

				CALL_INSTRUCTIONS
					inputOperands.insert(inputOperands.end(), call.args.begin(), call.args.end());
					break;

				MEMBER_CALL_INSTRUCTINS
					inputOperands.insert(inputOperands.end(), member_call.args.begin(), member_call.args.end());
					break;

				OPCODE_INSTRUCTIONS
					break;

				PHI_INSTRUCTIONS
					// not considered in this step
					for(auto& edge : phi.args) {
						inputOperands.push_back(edge.temp);
					}
					break;

				default:
					throw std::runtime_error("opcode not handled in inputOperands()");
			}

			return inputOperands;
		}

		Instruction() {}
		Instruction(Opcode op) : opcode(op) {
			switch(opcode)
			{
				CALL_INSTRUCTIONS
					createCallOp();
					break;
				MEMBER_CALL_INSTRUCTINS
					createMemberCallOp();
					break;
				PHI_INSTRUCTIONS
					createPhiOp();
				default:
					break;
			}
		}

		Instruction(Instruction const& old)
				: opcode(old.opcode), id(old.id)
		{
			switch(opcode)
			{
			BINARY_INSTRUCTIONS
				new (&binary) BinaryOp(old.binary);
				break;
			UNARY_INSTRUCTIONS
				new (&unary) UnaryOp(old.unary);
				break;
			CONST_INSTRUCTIONS
				new (&constant) ConstOp(old.constant);
				break;
			ARRAY_INSTRUCTIONS
				new (&array) ArrayOp(old.array);
				break;
			ALLOCATE_INSTRUCTIONS
				new (&alloc) NewOp(old.alloc);
				break;
			JUMP_INSTRUCTIONS
				new (&jump) GotoOp(old.jump);
				break;
			CALL_INSTRUCTIONS
				new (&call) CallOp(old.call);
				break;
			MEMBER_CALL_INSTRUCTINS
				new (&member_call) MemberCallOp(old.member_call);
				break;
			OPCODE_INSTRUCTIONS
				break;
			PHI_INSTRUCTIONS
				new (&phi) PhiOp(old.phi);
				break;
			OBJ_ALLOCATE_INSTRUCTIONS
				new (&obj_alloc) AllocOp(old.obj_alloc);
				break;
			OBJ_ACCESS_INSTRUCTIONS
				new (&access) AccessOp(old.access);
				break;
			GLOB_ACCESS_INSTRUCTIONS
				new (&global) GlobalAccessOp(old.global);
				break;

			case Opcode::NOP:
				break;

			default:
				throw std::runtime_error("opcode not handled in copy constructor");
			}
		}

		Instruction& operator = (Instruction const& other)
		{
			if(this == &other)
				return *this;

			this->~Instruction();
			return *new (this) Instruction(other);
		}

		~Instruction()
		{
			switch(opcode)
			{
			CALL_INSTRUCTIONS
				call.~CallOp();
				break;
			PHI_INSTRUCTIONS
				phi.~PhiOp();
				break;
			default: break;
			}
		}

		void createCallOp() {
			new (&call) CallOp;
		}

		void createMemberCallOp() {
			new (&member_call) MemberCallOp;
		}

		void createPhiOp() {
			new (&phi) PhiOp;
		}

		Optional<u16> dstIdx() const
		{
			switch(opcode)
			{
			case Opcode::LOAD:
			case Opcode::NEG:
			case Opcode::NOT:
				return unary.dstIdx;

			case Opcode::ADD:
			case Opcode::SUB:
			case Opcode::MUL:
			case Opcode::DIV:
			case Opcode::MOD:
			case Opcode::GT:
			case Opcode::GTE:
			case Opcode::EQ:
			case Opcode::NEQ:
			case Opcode::LTE:
			case Opcode::LT:
			case Opcode::AND:
			case Opcode::OR:
				return binary.dstIdx;

			case Opcode::CONST:
				return constant.dstIdx;

			case Opcode::LENGTH:
			case Opcode::LOAD_IDX:
				return array.valueIdx;

			case Opcode::NEW:
				return alloc.dstIdx;

			case Opcode::CALL:
			case Opcode::SPECIAL:
				return call.dstIdx;

			case Opcode::PHI:
				return phi.dstIdx;

			case Opcode::NOP:
			case Opcode::STORE:
			case Opcode::STORE_IDX:
			case Opcode::RETURN:
			case Opcode::RET_VOID:
			case Opcode::CALL_VOID:
			case Opcode::SPECIAL_VOID:
			case Opcode::GOTO:
			case Opcode::IF_GOTO:
				break;

			case Opcode::ALLOCATE:
				return obj_alloc.dstIdx;
			case Opcode::OBJ_STORE:
				break;

			default:
				throw std::runtime_error("opcode not handled in dstIdx()");

			}

			return {};
		}
	};

	struct Field {
		u8 typeId;
		std::string name;
		u16 offset;

		OperandSize getSize() const {
			switch((BaseType) typeId) {
			case BaseType::VOID:
				throw std::runtime_error("invalid member of type void");
			case BaseType::BOOL:
			case BaseType::INT8:
				return OperandSize::BYTE;
			case BaseType::CHAR:
			case BaseType::INT16:
				return OperandSize::WORD;
			case BaseType::FLP32:
			case BaseType::INT32:
				return OperandSize::DWORD;
			case BaseType::FLP64:
			case BaseType::INT64:
				return OperandSize::QWORD;
			default:
				if(typeId < 9) {
					throw std::runtime_error("not yet implemented type id " + std::to_string(typeId));
				} else {
					return OperandSize::QWORD; // ptr type
				}
			}
		}
	};

	struct StructType {
		u8 id;
		std::string name;
		std::vector<Field> fields;
		std::vector<u16> vTable;

	private:
		u16 _size = 0;

	public:
		StructType(u8 id, const std::string &name, const std::vector<Field> &fields, std::vector<u16> const& vTable)
			: id(id), name(name), fields(fields), vTable(vTable) {}

		u16 getSize() {
			if(_size != 0) {
				return _size;
			}

			return _size = calculateSize();
		}

		u16 calculateSize() {
			u16 sum = 8; // vPTR
			for(auto& field : fields) {
				field.offset = sum;
				sum += field.getSize();
			}

			return sum;
		}

		u16 getOffset(u16 fieldIdx) const {
			if(_size == 0) {
				throw TypeNotPackedException("type " + std::to_string(id) + " not yet packed");
			}

			return fields[fieldIdx].offset;
		}

		OperandSize getFieldSize(u16 fieldIdx) const {
			if(_size == 0) {
				throw TypeNotPackedException("type " + std::to_string(id) + " not yet packed");
			}

			return fields[fieldIdx].getSize();
		}

	};

	struct Function
	{
		std::string name;
		std::vector<Local> parameters;
		Type returnType;
		std::vector<Local> variables;
		std::vector<Instruction> instructions;
		std::vector<Block> blocks;
		u16 temporyCount;
		std::vector<Type> temporaryTypes;

		Local const& local(u16 idx)
		{
			if(parameters.size() > idx) {
				return parameters[idx];
			} else {
				idx -= parameters.size();
			}

			if(variables.size() > idx) {
				return variables[idx];
			} else {
				throw std::out_of_range("Invalid variable index " + std::to_string(idx)
				                        + " (originally was " + std::to_string(idx + parameters.size()) + ")");
			}
		}
	};

	struct Program
	{
		std::vector<Field> globals;
		std::map<u8, StructType> types;
		std::vector<Function> functions;
	};

	Program loadBytecode(std::string const& filepath);

	namespace internal
	{
		void read(std::istream& is, am2017s::bytecode::Program& program);

		template <typename T>
		T read(std::istream& is);

		u16 countTemporaries(std::vector<Local>& parameters, std::vector<Instruction>& instructions);

		void assignTypesToTemporaries(Program &p, Function& f);
	}

	void dump(std::ostream& os, Program const& program, Function const& function);
	void dump(std::ostream& os, Program const& program);
}}
