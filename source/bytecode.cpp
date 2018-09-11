#include <fstream>
#include <iostream>
#include <limits>
#include <utility>

#include <bytecode.hpp>

namespace am2017s { namespace bytecode { namespace internal
{
	template <typename>
	struct OverloadTag
	{};

	static
	void checkForErrors(std::istream& is)
	{
		if(!is)
			throw BytecodeLoaderException("failed to load bytecode file");
	}

	template <typename T>
	static
	T doRead(std::istream& is, OverloadTag<T>)
	{
		T value;
		is.read(reinterpret_cast<char*>(&value), sizeof value);

		checkForErrors(is);
		return value;
	}

	static
	std::string doRead(std::istream& is, OverloadTag<std::string>)
	{
		auto length = read<u16>(is);

		std::string result;
		result.resize(length);
		is.read(const_cast<char*>(result.data()), result.size());

		checkForErrors(is);
		return result;
	}

	template <typename T>
	static
	std::vector<T> doRead(std::istream& is, OverloadTag<std::vector<T>>)
	{
		std::vector<T> result;

		for(auto length = read<u16>(is); length--;)
			result.push_back(read<T>(is));

		return result;
	}

	static
	Type doRead(std::istream& is, OverloadTag<Type>)
	{
		auto byte = read<u8>(is);

		Type result;
		result.isArray = byte >> 7;
		result.baseType = (u8) (byte & 0x7F);
		return result;
	}

	static
	Field doRead(std::istream& is, OverloadTag<Field>) {
		u8 typeId = read<u8>(is);
		return {
				typeId, read<std::string>(is)
		};
	}

	static
	StructType doRead(std::istream& is, OverloadTag<StructType>) {
		return {
			read<u8>(is),
			read<std::string>(is),
			read<std::vector<Field>>(is),
			read<std::vector<u16>>(is)
		};
	}

	static
	Local doRead(std::istream& is, OverloadTag<Local>)
	{
		Local result;
		result.type = read<Type>(is);
		result.name = read<std::string>(is);
		return result;
	}

	static
	Opcode doRead(std::istream& is, OverloadTag<Opcode>)
	{
		auto op = static_cast<Opcode>(read<u8>(is));

		switch(op)
		{
		case Opcode::LOAD:
		case Opcode::LOAD_IDX:
		case Opcode::CONST:
		case Opcode::ADD:
		case Opcode::SUB:
		case Opcode::MUL:
		case Opcode::DIV:
		case Opcode::MOD:
		case Opcode::NEG:
		case Opcode::GT:
		case Opcode::GTE:
		case Opcode::EQ:
		case Opcode::NEQ:
		case Opcode::LTE:
		case Opcode::LT:
		case Opcode::AND:
		case Opcode::OR:
		case Opcode::NOT:
		case Opcode::NEW:
		case Opcode::STORE:
		case Opcode::STORE_IDX:
		case Opcode::GOTO:
		case Opcode::IF_GOTO:
		case Opcode::LENGTH:
		case Opcode::PHI:
		case Opcode::CALL:
		case Opcode::SPECIAL:
		case Opcode::CALL_VOID:
		case Opcode::SPECIAL_VOID:
		case Opcode::RET_VOID:
		case Opcode::RETURN:
		case Opcode::ALLOCATE:
		case Opcode::OBJ_LOAD:
		case Opcode::OBJ_STORE:
		case Opcode::GLOB_LOAD:
		case Opcode::GLOB_STORE:
		case Opcode::VOID_MEMBER_CALL:
		case Opcode::MEMBER_CALL:
			return op;

		default: break;
		}

		throw BytecodeLoaderException("invalid opcode encountered " + std::to_string((u8) op));
	}

	static
	Instruction doRead(std::istream& is, OverloadTag<Instruction>)
	{
		Instruction result(read<Opcode>(is));

		switch(result.opcode)
		{

		case Opcode::NEG:
		case Opcode::NOT:
			result.unary.srcIdx = read<u16>(is);
			break;

		case Opcode::CONST:
			result.constant.type = read<Type>(is);

			if(result.constant.type.isArray)
			{
				throw BytecodeLoaderException("received const with isArray flag");
			}

			switch((BaseType) result.constant.type.baseType)
			{
			case BaseType::VOID:
				result.constant.value = 0;
				break;
			case BaseType::BOOL:
			case BaseType::INT8:
				result.constant.value = read<u8>(is);
				break;
			case BaseType::CHAR:
			case BaseType::INT16:
				result.constant.value = read<u16>(is);
				break;
			case BaseType::INT32:
				result.constant.value = read<i32>(is);
				break;
			case BaseType::INT64:
				result.constant.value = read<i64>(is);
				break;
			case BaseType::FLP32:
				result.constant.value = read<i32>(is);
				break;
			case BaseType::FLP64:
				result.constant.value = read<i64>(is);
				break;
			default:
				throw BytecodeLoaderException("Unexpected type in const instruction");
			}
			break;

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

			result.binary.lsrcIdx = read<u16>(is);
			result.binary.rsrcIdx = read<u16>(is);
			break;

		case Opcode::LOAD_IDX:
			result.array.memoryIdx = read<u16>(is);
			result.array.indexIdx = read<u16>(is);
			break;

		case Opcode::STORE_IDX:
			result.array.memoryIdx = read<u16>(is);
			result.array.indexIdx = read<u16>(is);
			result.array.valueIdx = read<u16>(is);
			break;

		case Opcode::NEW:
			result.alloc.type = read<Type>(is);
			result.alloc.sizeIdx = read<u16>(is);
			break;

		case Opcode::GOTO:
			result.jump.branchIdx = read<u16>(is);
			break;

		case Opcode::IF_GOTO:
			result.jump.conditionIdx = read<u16>(is);
			result.jump.branchIdx = read<u16>(is);
			break;

		case Opcode::LENGTH:
			result.array.memoryIdx = read<u16>(is);
			break;

		case Opcode::PHI:
			result.phi.args = read<std::vector<PhiEdge>>(is);
			break;

		case Opcode::SPECIAL:
		case Opcode::SPECIAL_VOID:
			result.call.functionIdx = read<u8>(is);
			result.call.args = read<std::vector<u16>>(is);
			break;

		case Opcode::CALL:
		case Opcode::CALL_VOID:
			result.call.functionIdx = read<u16>(is);
			result.call.args = read<std::vector<u16>>(is);
			break;

		case Opcode::RET_VOID:
			break;
		case Opcode::RETURN:
			result.unary.srcIdx = read<u16>(is);
			break;

		case Opcode::ALLOCATE:
			result.obj_alloc.typeId = read<u8>(is);
			break;

		case Opcode::OBJ_LOAD:
			result.access.ptrIdx = read<u16>(is);
			result.access.typeId = read<u8>(is);
			result.access.fieldIdx = read<u8>(is);
			break;

		case Opcode::OBJ_STORE:
			result.access.ptrIdx = read<u16>(is);
			result.access.typeId = read<u8>(is);
			result.access.fieldIdx = read<u8>(is);
			result.access.valueIdx = read<u16>(is);
			break;

		case Opcode::GLOB_LOAD:
			result.global.globalIdx = read<u16>(is);
			break;

		case Opcode::GLOB_STORE:
			result.global.globalIdx = read<u16>(is);
			result.global.value = read<u16>(is);
			break;

		case Opcode::VOID_MEMBER_CALL:
			result.member_call.functionIdx = read<u8>(is);
			result.member_call.args = read<std::vector<u16>>(is);
			result.member_call.ptrIdx = result.member_call.args[0];
			break;

		case Opcode::MEMBER_CALL:
			result.member_call.functionIdx = read<u8>(is);
			result.member_call.args = read<std::vector<u16>>(is);
			result.member_call.ptrIdx = result.member_call.args[0];
			break;

		default:
			throw BytecodeLoaderException("unhandled opcode");
		}

		return result;
	}

	static
	Block doRead(std::istream& is, OverloadTag<Block>) {
		Block block;
		block.instructionCount = read<u16>(is);
		block.successors = read<std::vector<u16>>(is);
		return block;
	}

	static
	PhiEdge doRead(std::istream& is, OverloadTag<PhiEdge>) {
		return {read<u16>(is), read<u16>(is)};
	}

	template <typename T>
	T read(std::istream& is)
	{
		return doRead(is, OverloadTag<T>());
	}

	u16 countTemporaries(std::vector<Local>& parameters, std::vector<Instruction>& instructions)
	{
		u16 result = parameters.size();

		u16 instructionId = 0;
		for(auto& instr : instructions)
		{
			instr.id = instructionId++;
			switch(instr.opcode)
			{
			case Opcode::STORE:
			case Opcode::STORE_IDX: break;

			case Opcode::LOAD:
			case Opcode::NEG:
			case Opcode::NOT:
			case Opcode::CONST: instr.unary.dstIdx = result++; break;

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
			case Opcode::OR: instr.binary.dstIdx = result++; break;

			case Opcode::NEW: instr.alloc.dstIdx = result++; break;

			case Opcode::LENGTH:
			case Opcode::LOAD_IDX: instr.array.valueIdx = result++; break;

			case Opcode::GOTO:
			case Opcode::IF_GOTO: break;

			case Opcode::CALL_VOID:
			case Opcode::SPECIAL_VOID: break;

			case Opcode::CALL:
			case Opcode::SPECIAL: instr.call.dstIdx = result++; break;

			case Opcode::RET_VOID:
			case Opcode::RETURN: break;

			case Opcode::PHI: instr.phi.dstIdx = result++; break;

			case Opcode::ALLOCATE: instr.obj_alloc.dstIdx = result++; break;

			case Opcode::OBJ_LOAD: instr.access.valueIdx = result++; break;
			case Opcode::OBJ_STORE: break;

			case Opcode::GLOB_LOAD: instr.global.value = result++; break;
			case Opcode::GLOB_STORE: break;

			case Opcode::VOID_MEMBER_CALL: break;
			case Opcode::MEMBER_CALL: instr.member_call.dstIdx = result++; break;

			default:
				throw BytecodeLoaderException("unhandled opcode when determining temporary indicies");
			}
		}

		return result;
	}

	static void setBlockPredecessors(std::vector<Block>& blocks) {
		u16 predecessorId = 0;
		for(auto& predecessor : blocks) {
			for(u16 successorId : predecessor.successors) {
				blocks[successorId].predecessors.push_back(predecessorId);
			}
			predecessorId++;
		}
	}

	static
	void read(std::istream& is, Function& function)
	{
		function.name         = read<std::string>(is);
		function.parameters   = read<std::vector<Local>>(is);
		function.returnType   = read<Type>(is);
//		function.variables    = read<std::vector<Local>>(is);
		function.blocks       = read<std::vector<Block>>(is);

		setBlockPredecessors(function.blocks);

		function.instructions = read<std::vector<Instruction>>(is);
		function.temporyCount = countTemporaries(function.parameters, function.instructions);
	}

	void read(std::istream& is, Program& program)
	{
		auto magic = read<u16>(is);
		if(magic == ('#' | ('!' << 8)))
		{
			is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			magic = read<u16>(is);
		}

		if(magic != 1706) {
			throw BytecodeLoaderException("Magic constant did not appear as expected");
		}

		program.globals = read<std::vector<Field>>(is);

		std::vector<StructType> types = read<std::vector<StructType>>(is);
		for(auto& type : types) {
			program.types.emplace(type.id, type);
		}

		auto functionCount = read<u16>(is);
		for(Function function; functionCount--;)
		{
			read(is, function);
			program.functions.push_back(std::move(function));
		}
	}

	void assignTypesToTemporaries(Program& p, Function& f)
	{
		f.temporaryTypes.clear();
		f.temporaryTypes.resize(f.temporyCount);

		int currentTemporary = 0;
		for(Local const& l : f.parameters) {
			f.temporaryTypes[currentTemporary++] = l.type;
		}

		for(Instruction const& instr : f.instructions)
		{
			switch(instr.opcode)
			{
			case Opcode::GT:
			case Opcode::GTE:
			case Opcode::EQ:
			case Opcode::NEQ:
			case Opcode::LTE:
			case Opcode::LT:
				if(f.temporaryTypes[instr.binary.lsrcIdx].isArray || f.temporaryTypes[instr.binary.rsrcIdx].isArray)
				{
					throw BytecodeLoaderException("Compare instruction is not allowed on arrays");
				}
				else if(f.temporaryTypes[instr.binary.lsrcIdx] == f.temporaryTypes[instr.binary.rsrcIdx])
				{
					f.temporaryTypes[currentTemporary++].baseType = (u8) BaseType::BOOL;
				}
				else
				{
					throw BytecodeLoaderException("Types on compare instruction do not agree");
				}
				break;

			case Opcode::LOAD:
				f.temporaryTypes[currentTemporary++] = f.local(instr.unary.srcIdx).type;
				break;

			case Opcode::LOAD_IDX:
				if(!f.temporaryTypes.at(instr.array.memoryIdx).isArray)
				{
					throw BytecodeLoaderException("Type for loadIdx is not an array");
				}

				f.temporaryTypes[currentTemporary] = f.temporaryTypes.at(instr.array.memoryIdx);
				f.temporaryTypes[currentTemporary].isArray = false;
				currentTemporary++;
				break;

			case Opcode::CONST:
				if(instr.constant.type.isArray)
				{
					throw BytecodeLoaderException("const cannot have array type");
				}

				f.temporaryTypes[currentTemporary++] = instr.constant.type;
				break;

			case Opcode::ADD:
			case Opcode::SUB:
			case Opcode::MUL:
			case Opcode::DIV:
			case Opcode::MOD:
			case Opcode::AND:
			case Opcode::OR:
				if(f.temporaryTypes[instr.binary.lsrcIdx].isArray || f.temporaryTypes[instr.binary.rsrcIdx].isArray)
				{
					throw BytecodeLoaderException("Binary instruction is not allowed on arrays");
				}
				else if(f.temporaryTypes[instr.binary.lsrcIdx] == f.temporaryTypes[instr.binary.rsrcIdx])
				{
					f.temporaryTypes[currentTemporary++] = f.temporaryTypes[instr.binary.lsrcIdx];
				}
				else
				{
					throw BytecodeLoaderException("Types on binary instruction do not agree");
				}
				break;

			case Opcode::NOT:
				if(f.temporaryTypes[instr.unary.srcIdx].baseType != (u8) BaseType::BOOL || f.temporaryTypes[instr.unary.srcIdx].isArray)
				{
					throw BytecodeLoaderException("argument for `not` must be of type simple boolean");
				}

				f.temporaryTypes[currentTemporary++] = f.temporaryTypes[instr.unary.srcIdx];
				break;
			case Opcode::NEG:
				if(f.temporaryTypes[instr.unary.srcIdx].isArray)
				{
					throw BytecodeLoaderException("argument for `neg` cannot have array type");
				}

				f.temporaryTypes[currentTemporary++] = f.temporaryTypes[instr.unary.srcIdx];
				break;

			case Opcode::CALL:
				f.temporaryTypes[currentTemporary++] = p.functions[instr.call.functionIdx].returnType;
				break;

			case Opcode::LENGTH:
				if(!f.temporaryTypes[instr.array.memoryIdx].isArray)
				{
					throw BytecodeLoaderException("argument for `length` is not an array");
				}

				f.temporaryTypes[currentTemporary].isArray = false;
				f.temporaryTypes[currentTemporary].baseType = (u8) BaseType::INT32;
				currentTemporary++;
				break;

			case Opcode::NEW:
				f.temporaryTypes[currentTemporary] = instr.alloc.type;
				f.temporaryTypes[currentTemporary].isArray = true;
				currentTemporary++;
				break;

			case Opcode::PHI:
				f.temporaryTypes[currentTemporary].isArray = f.temporaryTypes[instr.phi.args.front().temp].isArray;
				f.temporaryTypes[currentTemporary].baseType = f.temporaryTypes[instr.phi.args.front().temp].baseType;
				currentTemporary++;
				break;

			case Opcode::ALLOCATE:
				f.temporaryTypes[currentTemporary].isArray = false;
				f.temporaryTypes[currentTemporary].baseType = instr.obj_alloc.typeId;
				currentTemporary++;
				break;

			case Opcode::OBJ_LOAD: {
				u8 typeId = p.types.at(instr.access.typeId).fields[instr.access.fieldIdx].typeId;
				f.temporaryTypes[currentTemporary].isArray = typeId >> 7;
				f.temporaryTypes[currentTemporary].baseType = (typeId & 0b01111111);
				currentTemporary++;
			}
				break;

			case Opcode::GLOB_LOAD:
				f.temporaryTypes[currentTemporary].isArray = false;
				f.temporaryTypes[currentTemporary].baseType = p.globals[instr.global.globalIdx].typeId;
				currentTemporary++;
				break;

			case Opcode::MEMBER_CALL: {
				StructType const& s = p.types.at(f.temporaryTypes[instr.member_call.ptrIdx].baseType);
				Function const& calledF = p.functions.at(s.vTable[instr.member_call.functionIdx]);

				f.temporaryTypes[currentTemporary] = calledF.returnType;
				currentTemporary++;
				break;
			}

			default:
				// no temporary will be created
				break;
			}

		}
	}

	static
	void staticAnalysis(Program& program)
	{
		u16 offset = 0;
		for(auto& global : program.globals) {
			global.offset = offset;
			offset += global.getSize();
		}

		for(Function& f : program.functions)
		{
			assignTypesToTemporaries(program, f);
		}
	}
}

	Program loadBytecode(std::string const& filepath)
	{
		std::ifstream file(filepath, std::ios::binary);
		am2017s::bytecode::internal::checkForErrors(file);

		Program program;
		am2017s::bytecode::internal::read(file, program);

		if(file.peek() != EOF)
			throw BytecodeLoaderException("failed to read file '" + filepath + "'");

		internal::staticAnalysis(program);

		return program;
	}

	template <typename Cont, typename F, typename G>
	void forEachWithBetween(Cont const& cont, F&& f, G&& g)
	{
		if(!cont.empty())
		{
			f(cont[0]);

			for(auto it = ++cont.begin(); it != cont.end(); ++it)
			{
				g();
				f(*it);
			}
		}
	}
}}
