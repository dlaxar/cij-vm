#include <interpreter/InterpretEngine.hpp>
#include <jit/allocator/memory/HeapAllocator.hpp>
#include <jit/SpecialFunctions.hpp>


namespace am2017s { namespace interpreter {

InterpretEngine::InterpretEngine(bytecode::Program program, Options const& options) :
program(std::move(program)), options(options) {

}

static
u16 findMain(std::vector<bytecode::Function> const& functions) {
	for(u16 i = 0; i != functions.size(); ++i)
		if(functions[i].name == "main")
			return i;

	throw std::logic_error("main function not found");
}

static
u16 blockIdxForInstruction(u16 instrId, std::vector<bytecode::Block> const& blocks) {
	u16 instrs = 0;
	for(u16 blockIdx = 0; blockIdx != blocks.size(); ++blockIdx) {
		instrs += blocks[blockIdx].instructionCount;
		if(instrs > instrId) {
			return blockIdx;
		}
	}

	throw std::runtime_error("no block could be found");
}

#define DISPATCH        { prev = rip; goto *labels[(u8)(++rip)->opcode];};
#define DISPATCH_DIRECT { goto *labels[(u8)(  rip)->opcode];};

#define CMP(op) { \
	auto instr = rip->binary; \
	switch((bytecode::BaseType) function.temporaryTypes[instr.lsrcIdx].baseType) { \
		case bytecode::BaseType::INT8: \
		values[instr.dstIdx].b = values[instr.lsrcIdx].byte op values[instr.rsrcIdx].byte; \
		break; \
\
		case bytecode::BaseType::INT16: \
		case bytecode::BaseType::CHAR: \
		values[instr.dstIdx].b = values[instr.lsrcIdx].s op values[instr.rsrcIdx].s; \
		break; \
\
		case bytecode::BaseType::INT32: \
		values[instr.dstIdx].b = values[instr.lsrcIdx].i op values[instr.rsrcIdx].i; \
		break; \
\
		case bytecode::BaseType::INT64: \
		values[instr.dstIdx].b = values[instr.lsrcIdx].l op values[instr.rsrcIdx].l; \
		break; \
\
		case bytecode::BaseType::FLP32: \
		values[instr.dstIdx].b = values[instr.lsrcIdx].f op values[instr.rsrcIdx].f; \
		break; \
\
		case bytecode::BaseType::FLP64: \
		values[instr.dstIdx].b = values[instr.lsrcIdx].d op values[instr.rsrcIdx].d; \
		break; \
\
		default: \
		values[instr.dstIdx].b = values[instr.lsrcIdx].ref op values[instr.rsrcIdx].ref; \
	} \
}

#define BINARY(op) { \
	auto instr = rip->binary; \
	switch((bytecode::BaseType) function.temporaryTypes[instr.lsrcIdx].baseType) { \
		case bytecode::BaseType::INT8: \
		values[instr.dstIdx].byte = values[instr.lsrcIdx].byte op values[instr.rsrcIdx].byte; \
		break; \
\
		case bytecode::BaseType::INT16: \
		case bytecode::BaseType::CHAR: \
		values[instr.dstIdx].s = values[instr.lsrcIdx].s op values[instr.rsrcIdx].s; \
		break; \
\
		case bytecode::BaseType::INT32: \
		values[instr.dstIdx].i = values[instr.lsrcIdx].i op values[instr.rsrcIdx].i; \
		break; \
\
		case bytecode::BaseType::INT64: \
		values[instr.dstIdx].l = values[instr.lsrcIdx].l op values[instr.rsrcIdx].l; \
		break; \
\
		case bytecode::BaseType::FLP32: \
		values[instr.dstIdx].s = values[instr.lsrcIdx].f op values[instr.rsrcIdx].f; \
		break; \
\
		case bytecode::BaseType::FLP64: \
		values[instr.dstIdx].d = values[instr.lsrcIdx].d op values[instr.rsrcIdx].d; \
		break; \
\
		default: \
		throw std::runtime_error("no valid binary op"); \
	} \
}

#define BINARYINT(op) { \
	auto instr = rip->binary; \
	switch((bytecode::BaseType) function.temporaryTypes[instr.lsrcIdx].baseType) { \
		case bytecode::BaseType::INT8: \
		values[instr.dstIdx].byte = values[instr.lsrcIdx].byte op values[instr.rsrcIdx].byte; \
		break; \
\
		case bytecode::BaseType::INT16: \
		case bytecode::BaseType::CHAR: \
		values[instr.dstIdx].s = values[instr.lsrcIdx].s op values[instr.rsrcIdx].s; \
		break; \
\
		case bytecode::BaseType::INT32: \
		values[instr.dstIdx].i = values[instr.lsrcIdx].i op values[instr.rsrcIdx].i; \
		break; \
\
		case bytecode::BaseType::INT64: \
		values[instr.dstIdx].l = values[instr.lsrcIdx].l op values[instr.rsrcIdx].l; \
		break; \
\
		default: \
		throw std::runtime_error("no valid binary int op"); \
	} \
}

void InterpretEngine::executeFunction(u16 idx, u16* args, Value* prevFrame, u16 retIdx) {

	bytecode::Function &function = program.functions.at(idx);
	Value* values = (Value*) malloc(sizeof(Value) * function.temporyCount);

	for(int i = 0; i != function.parameters.size(); ++i) {
		values[i] = prevFrame[args[i]];
	}

	auto prev = function.instructions.data();
	auto rip = function.instructions.data();

	static constexpr void* const labels[] = {
			&&nop,        // 0
			&&invalid,      // 1
			&&invalid,      // 2
			&&constant,   // 3
			&&add,
			&&sub,
			&&mul,
			&&div,
			&&mod,
			&&neg,
			&&gt,      // 10
			&&invalid,
			&&eq,
			&&neq,
			&&lte,
			&&lt,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&not_,      // 20
			&&new_,
			&&goto_,
			&&ifgoto,     // 23
			&&invalid,
			&&length,
			&&phi,
			&&invalid,
			&&call,
			&&invalid,
			&&call,      // 30
			&&specialcall,
			&&retvoid,    // 32
			&&ret,        // 33
			&&allocate,   // 34
			&&load,       // 35
			&&store,      // 36
			&&load_global,
			&&store_global,
			&&call_member_void,
			&&call_member,// 40
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,      // 50
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,
			&&invalid,      // 60
			&&invalid,
			&&invalid,
			&&loadidx,
			&&storeidx,
	};

	DISPATCH_DIRECT;

invalid:
	std::cout << "Invalid opcode: " << std::to_string((u8)rip->opcode) << std::endl;
	throw std::runtime_error("Invalid opcode");

nop: DISPATCH;

constant: {
		auto& instr = rip->constant;
		switch((bytecode::BaseType)instr.type.baseType) {
			case bytecode::BaseType::BOOL:
				values[instr.dstIdx].b = (bool) instr.value;
				break;

			case bytecode::BaseType::INT8:
				values[instr.dstIdx].byte = (u8) instr.value;
				break;

			case bytecode::BaseType::INT16:
			case bytecode::BaseType::CHAR:
				values[instr.dstIdx].s = (i16) instr.value;
				break;

			case bytecode::BaseType::INT32:
				values[instr.dstIdx].i = (i32) instr.value;
				break;

			case bytecode::BaseType::INT64:
				values[instr.dstIdx].l = (i64) instr.value;
				break;

			case bytecode::BaseType::FLP32:
				values[instr.dstIdx].f = reinterpret_cast<float&>(instr.value);
				break;

			case bytecode::BaseType::FLP64:
				values[instr.dstIdx].d = reinterpret_cast<double&>(instr.value);
				break;

			default:
				// null ptr
				values[instr.dstIdx].l = 0;
		}
	};
	DISPATCH;

add: BINARY(+); DISPATCH;
sub: BINARY(-); DISPATCH;
mul: BINARY(*); DISPATCH;
div: BINARY(/); DISPATCH;
mod: BINARYINT(%); DISPATCH;

neg: {
	auto& instr = rip->unary;
	switch((bytecode::BaseType)function.temporaryTypes[instr.dstIdx].baseType) {

		case bytecode::BaseType::INT8:
			values[instr.dstIdx].byte = -values[instr.srcIdx].byte;
			break;

		case bytecode::BaseType::INT16:
		case bytecode::BaseType::CHAR:
			values[instr.dstIdx].s = -values[instr.srcIdx].s;
			break;

		case bytecode::BaseType::INT32:
			values[instr.dstIdx].i = -values[instr.srcIdx].i;
			break;

		case bytecode::BaseType::INT64:
			values[instr.dstIdx].l = -values[instr.srcIdx].l;
			break;

		case bytecode::BaseType::FLP32:
			values[instr.dstIdx].f = -values[instr.srcIdx].f;
			break;

		case bytecode::BaseType::FLP64:
			values[instr.dstIdx].d = -values[instr.srcIdx].d;
			break;

		default:
			throw std::runtime_error("invalid neg operand");
	}
	DISPATCH;
};

gt: CMP(>) DISPATCH;
lt: CMP(<) DISPATCH;
lte: CMP(<=) DISPATCH;
eq: CMP(==) DISPATCH;
neq: CMP(!=) DISPATCH;

not_ : {
	values[rip->unary.dstIdx].b = !values[rip->unary.srcIdx].b;
	DISPATCH;
};

new_: {
	auto& instr = rip->alloc;
	values[instr.dstIdx].ref = jit::allocator::allocate_array(nullptr, instr.type.size(), instr.type.baseType, values[instr.sizeIdx].i);
	DISPATCH;
};

goto_: {
	u16 instrs = 0;
	for(u16 blockIdx = 0; blockIdx != rip->jump.branchIdx; ++blockIdx) {
		instrs += function.blocks[blockIdx].instructionCount;
	}

	prev = rip;
	rip = function.instructions.data() + instrs;
	DISPATCH_DIRECT;
};

ifgoto: {
		if(values[rip->jump.conditionIdx].b) {
			u16 instrs = 0;
			for(u16 blockIdx = 0; blockIdx != rip->jump.branchIdx; ++blockIdx) {
				instrs += function.blocks[blockIdx].instructionCount;
			}

			prev = rip;
			rip = function.instructions.data() + instrs;
			DISPATCH_DIRECT;
		} else {
			DISPATCH;
		}
	};

phi: {

	u16 prevBlock = blockIdxForInstruction(prev->id, function.blocks);
	for(bytecode::PhiEdge edge : rip->phi.args) {
		if(edge.block == prevBlock) {
			values[rip->phi.dstIdx] = values[edge.temp];
			break;
		}
	}

	// avoid setting `prev`
	++rip;
	DISPATCH_DIRECT;
	};

call: {
	executeFunction(rip->call.functionIdx, rip->call.args.data(), values, rip->call.dstIdx);
	DISPATCH;
};

specialcall: {
	auto& instr = rip->call;

	switch(instr.functionIdx) {
		case 0:
			bytecode::special::begin_int(this);
			break;
		case 1:
			bytecode::special::end_int(this);
			break;
		case 3:
			bytecode::special::printa_int(nullptr, (i32*)values[instr.args[0]].ref);
			break;
		case 4:
			bytecode::special::print_double(nullptr, values[instr.args[0]].d);
			break;
		case 5:
			bytecode::special::exit(nullptr, values[instr.args[0]].i);
			break;
		default:
			std::cout << "ignoring special call" << std::endl;
	}

	DISPATCH;
};

retvoid: return;

ret:
	prevFrame[retIdx] = values[rip->unary.srcIdx];
	return;

allocate: {
		program.types.at(rip->obj_alloc.typeId).calculateSize();
		values[rip->obj_alloc.dstIdx].ref = jit::allocator::allocate(nullptr, program.types.at(rip->obj_alloc.typeId).getSize());
		*((i64*)values[rip->obj_alloc.dstIdx].ref) = (i64)program.types.at(rip->obj_alloc.typeId).vTable.data();

		DISPATCH;
	};

load: {
		auto& instr = rip->access;
		void *offset = ((u8*)values[instr.ptrIdx].ref) + program.types.at(rip->access.typeId).getOffset(instr.fieldIdx);
		switch((bytecode::BaseType) function.temporaryTypes[instr.valueIdx].baseType) {
			case bytecode::BaseType::BOOL:
				values[instr.valueIdx].b = *((u8*)(offset));
				break;

			case bytecode::BaseType::INT8:
				values[instr.valueIdx].byte = (*((u8*)(offset)));
				break;

			case bytecode::BaseType::INT16:
			case bytecode::BaseType::CHAR:
				values[instr.valueIdx].s = *((i16*)(offset));
				break;

			case bytecode::BaseType::INT32:
				values[instr.valueIdx].i = *((i32*)(offset));
				break;

			case bytecode::BaseType::INT64:
				values[instr.valueIdx].l = *((i64*)(offset));
				break;

			case bytecode::BaseType::FLP32:
				values[instr.valueIdx].f = *((float*)(offset));
				break;

			case bytecode::BaseType::FLP64:
				values[instr.valueIdx].d = *((double*)(offset));
				break;

			default:
				values[instr.valueIdx].ref = (void*) *((i64*)(offset));
		}
	DISPATCH;
};

store: {
	auto& instr = rip->access;
	program.types.at(rip->access.typeId).calculateSize();


	void *offset = ((u8*)values[instr.ptrIdx].ref) + program.types.at(rip->access.typeId).getOffset(instr.fieldIdx);
	switch((bytecode::BaseType) function.temporaryTypes[instr.valueIdx].baseType) {
		case bytecode::BaseType::BOOL:
			*((u8*)(offset)) = (u8) values[instr.valueIdx].b;
			break;

		case bytecode::BaseType::INT8:
			(*((u8*)(offset))) = values[instr.valueIdx].byte;
			break;

		case bytecode::BaseType::INT16:
		case bytecode::BaseType::CHAR:
			*((i16*)(offset)) = values[instr.valueIdx].s;
			break;

		case bytecode::BaseType::INT32:
			*((i32*)(offset)) = values[instr.valueIdx].i;
			break;

		case bytecode::BaseType::INT64:
			*((i64*)(offset)) = values[instr.valueIdx].l;
			break;

		case bytecode::BaseType::FLP32:
			*((float*)(offset)) = values[instr.valueIdx].f;
			break;

		case bytecode::BaseType::FLP64:
			*((double*)(offset)) = values[instr.valueIdx].d;
			break;

		default:

			*(i64*)(offset) = (i64)values[instr.valueIdx].ref;
	}
	DISPATCH;
	};

length: {
	values[rip->array.valueIdx].i = ((i32*)(values[rip->array.memoryIdx].ref))[-1];
		DISPATCH;
	};

load_global: {
	values[rip->global.value] = global[rip->global.globalIdx];
	DISPATCH;
	};

store_global:{
	global[rip->global.globalIdx] = values[rip->global.value];
	DISPATCH;
	};

call_member:
call_member_void:
	{

		u16 *vTable = (u16 *) *((i64 *) (values[rip->member_call.ptrIdx].ref));

		u16 actualFunctionIdx = vTable[rip->member_call.functionIdx];

		executeFunction(actualFunctionIdx, rip->member_call.args.data(), values, rip->member_call.dstIdx);
	};
	DISPATCH;

loadidx:
	{
		auto& instr = rip->array;
		switch((bytecode::BaseType) function.temporaryTypes[instr.valueIdx].baseType) {
			case bytecode::BaseType::BOOL:
			values[instr.valueIdx].b = ((u8*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i];
			break;

			case bytecode::BaseType::INT8:
			values[instr.valueIdx].byte = ((u8*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i];
			break;

			case bytecode::BaseType::INT16:
			case bytecode::BaseType::CHAR:
			values[instr.valueIdx].s = ((u16*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i];
			break;

			case bytecode::BaseType::INT32:
			values[instr.valueIdx].i = ((i32*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i];
			break;

			case bytecode::BaseType::INT64:
			values[instr.valueIdx].l = ((i64*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i];
			break;

			case bytecode::BaseType::FLP32:
			values[instr.valueIdx].f = ((float*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i];
			break;

			case bytecode::BaseType::FLP64:
			values[instr.valueIdx].d = ((double*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i];
			break;

			default:
			values[instr.valueIdx].ref = (void*)((i64*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i];
		}

		DISPATCH;
	};

storeidx: {
	auto& instr = rip->array;
	switch((bytecode::BaseType) function.temporaryTypes[instr.valueIdx].baseType) {
		case bytecode::BaseType::BOOL:
			((u8*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i] = values[instr.valueIdx].b;
			break;

		case bytecode::BaseType::INT8:
			((u8*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i] = values[instr.valueIdx].byte;
			break;

		case bytecode::BaseType::INT16:
		case bytecode::BaseType::CHAR:
			((i16*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i] = values[instr.valueIdx].s;
			break;

		case bytecode::BaseType::INT32:
			((i32*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i] = values[instr.valueIdx].i;
			break;

		case bytecode::BaseType::INT64:
			((i64*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i] = values[instr.valueIdx].l;
			break;

		case bytecode::BaseType::FLP32:
			((float*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i] = values[instr.valueIdx].f;
			break;

		case bytecode::BaseType::FLP64:
			((double*)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i] = values[instr.valueIdx].d;
			break;

		default:
			((i64**)(values[instr.memoryIdx].ref))[values[instr.indexIdx].i] = (i64*)values[instr.valueIdx].ref;
	}

	DISPATCH;
	};

}


int InterpretEngine::execute() {

	for(bytecode::Function& f : program.functions) {
		for(bytecode::Instruction& i : f.instructions) {
			if(((u8)i.opcode) >= 100) {
				// make opcodes more compact 100 -> 34
				i.opcode = (bytecode::Opcode) ((u8)i.opcode - 66);
			}
		}
	}

	global = std::vector<Value>{program.globals.size()};

	auto idx = findMain(program.functions);

	Value ret;
	executeFunction(idx, nullptr, &ret, 0);

	std::cout << "returned " << std::to_string(ret.i) << std::endl;
	return ret.i;
};

}}