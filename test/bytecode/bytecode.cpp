#include <vector>

#include <catch2/catch.hpp>

#include <bytecode.hpp>
#include <types.hpp>
#include <sstream>

using namespace am2017s::bytecode;

template <std::size_t N>
static
std::istringstream byteStreamOf(char const(& bytes)[N])
{
	return std::istringstream(std::string(bytes, sizeof bytes - 1));
}

TEST_CASE("File parsing", "[bytecode]")
{
	auto& bytes =
		"\x01\x00"     // one function
		"\x04\x00main" // called main
		"\x00\x00"     // having no params
		"\x05"         // returning int
		"\x00\x00"     // has no local variables
		"\x00\x00"     // and no instructions
	;

	auto stream = byteStreamOf(bytes);
	Program program;
	internal::read(stream, program);

	SECTION("finds a single function")
	{
		REQUIRE(program.functions.size() == 1);
	}

	SECTION("extracts return type correctly")
	{
		auto returnType = program.functions[0].returnType;
		REQUIRE(returnType.isArray == false);
		REQUIRE(returnType.baseType == BaseType::INT32);
	}
}

TEST_CASE("Parsing of instructions", "[bytecode]")
{
	SECTION("load")
	{
		SECTION("single load")
		{
			auto& bytes =
				"\x01"     // opcode
				"\x03\x00" // src index
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::LOAD);
			REQUIRE(instr.unary.srcIdx == 3);
		}

		SECTION("multiple loads")
		{
			auto& bytes =
				"\x02\x00"     // two instructions
				"\x01\x03\x00" // load %3
				"\x01\x04\xff" // load %ff04
			;

			auto stream = byteStreamOf(bytes);
			auto instrs = internal::read<std::vector<Instruction>>(stream);

			REQUIRE(instrs.size() == 2);

			REQUIRE(instrs[0].opcode == Opcode::LOAD);
			REQUIRE(instrs[0].unary.srcIdx == 3);
			REQUIRE(instrs[1].opcode == Opcode::LOAD);
			REQUIRE(instrs[1].unary.srcIdx == 0xff04);
		}
	}

	SECTION("store")
	{
		SECTION("single")
		{
			auto& bytes =
				"\x02"     // opcode
				"\x03\x00" // variable index
				"\x00\xff" // temporary index
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::STORE);
			REQUIRE(instr.unary.srcIdx == 0xff00);
			REQUIRE(instr.unary.dstIdx == 3);
		}
		SECTION("multiple")
		{
			auto& bytes =
				"\x02\x00"     // two instructions
				"\x02\x03\x00\x00\xff" // load %3
				"\x02\x03\x00\x00\xfa" // load %ff04
			;

			auto stream = byteStreamOf(bytes);
			auto instrs = internal::read<std::vector<Instruction>>(stream);

			REQUIRE(instrs.size() == 2);

			REQUIRE(instrs[0].opcode == Opcode::STORE);
			REQUIRE(instrs[0].unary.srcIdx == 0xff00);
			REQUIRE(instrs[0].unary.dstIdx == 3);
			REQUIRE(instrs[1].opcode == Opcode::STORE);
			REQUIRE(instrs[1].unary.srcIdx == 0xfa00);
			REQUIRE(instrs[1].unary.dstIdx == 3);
		}
	}

	SECTION("const")
	{
		SECTION("byte")
		{
			auto& bytes =
				"\x03" // opcode
				"\x01" // byte
				"\x01" // true
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::CONST);
			REQUIRE(instr.constant.type.isArray == false);
			REQUIRE(instr.constant.type.baseType == BaseType::BOOL);
			REQUIRE(instr.constant.value == 1);
		}

		SECTION("int32")
		{
			auto& bytes =
				"\x03" // opcode
				"\x05" // int32
				"\xff\xab\xcd\xef" // a negative value!
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::CONST);
			REQUIRE(instr.constant.type.isArray == false);
			REQUIRE(instr.constant.type.baseType == BaseType::INT32);
			REQUIRE(instr.constant.value < 0); // make sure that we have the correct sign
			REQUIRE(instr.constant.value == (am2017s::i32)0xefcdabff);

		}

		SECTION("int64")
		{
			auto& bytes =
				"\x03" // opcode
				"\x06" // int64
				"\xff\xab\xcd\xef\xff\xff\xff\xff" // a negative value!
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::CONST);
			REQUIRE(instr.constant.type.isArray == false);
			REQUIRE(instr.constant.type.baseType == BaseType::INT64);
			REQUIRE(instr.constant.value == 0xffffffffefcdabff);
		}
	}

	SECTION("binary operations")
	{
		SECTION("add")
		{
			auto& bytes =
				"\x04"     // opcode
				"\x05\xff" // left temporary index
				"\xff\x00" // right temporary index
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::ADD);
			REQUIRE(instr.binary.lsrcIdx == 0xff05);
			REQUIRE(instr.binary.rsrcIdx == 0x00ff);
		}

		SECTION("and (bitwise)")
		{
			auto& bytes =
				"\x12"     // opcode for bitwise and
				"\x05\xff" // left temporary index
				"\xff\x00" // right temporary index
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::AND);
			REQUIRE(instr.binary.lsrcIdx == 0xff05);
			REQUIRE(instr.binary.rsrcIdx == 0x00ff);
		}
	}

	SECTION("unary operations")
	{
		SECTION("neg")
		{
			auto& bytes =
				"\x09"     // opcode
				"\x05\xff" // temporary index
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::NEG);
			REQUIRE(instr.unary.srcIdx == 0xff05);
		}

		SECTION("not")
		{
			auto& bytes =
				"\x14"     // opcode
				"\x05\xff" // temporary index
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::NOT);
			REQUIRE(instr.unary.srcIdx == 0xff05);
		}
	}

	SECTION("array access")
	{
		SECTION("loadIndex")
		{
			auto& bytes =
				"\x81"     // opcode
				"\x05\xff" // temporary index (array)
				"\x00\xff" // temporary index (array index)
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::LOAD_IDX);
			REQUIRE(instr.array.memoryIdx == 0xff05);
			REQUIRE(instr.array.indexIdx == 0xff00);
		}

		SECTION("storeIndex")
		{
			auto& bytes =
				"\x82"     // opcode
				"\x05\xff" // temporary index (array)
				"\x00\xff" // temporary index (array index)
				"\x00\x01" // temporary index (value)
			;

			auto stream = byteStreamOf(bytes);
			auto instr = internal::read<Instruction>(stream);

			REQUIRE(instr.opcode == Opcode::STORE_IDX);
			REQUIRE(instr.array.memoryIdx == 0xff05);
			REQUIRE(instr.array.indexIdx == 0xff00);
			REQUIRE(instr.array.valueIdx == 0x0100);
		}
	}

	SECTION("new")
	{
		auto& bytes =
			"\x15"     // opcode
			"\x85"     // type (array | int32)
			"\x01\xff" // temporary index (size index)
		;

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::NEW);
		REQUIRE(instr.alloc.type.isArray == true);
		REQUIRE(instr.alloc.type.baseType == BaseType::INT32);
		REQUIRE(instr.alloc.sizeIdx == 0xff01);
	}

	SECTION("goto")
	{
		auto& bytes =
			"\x16"     // opcode
			"\xba\xba" // label index
		;

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::GOTO);
		REQUIRE(instr.jump.branchIdx == 0xbaba);
	}

	SECTION("conditional goto")
	{
		auto& bytes =
			"\x17"     // opcode
			"\xaa\xbb" // condition index
			"\xa5\xb7" // label index
		;

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::IF_GOTO);
		REQUIRE(instr.jump.conditionIdx == 0xbbaa);
		REQUIRE(instr.jump.branchIdx == 0xb7a5);
	}

	SECTION("length")
	{
		auto& bytes =
			"\x19"     // opcode
			"\xff\xff" // array temporary
		;

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::LENGTH);
		REQUIRE(instr.array.memoryIdx == 0xffff);
	}

	SECTION("return void")
	{
		auto& bytes = "\x20"; // opcode

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::RET_VOID);
	}

	SECTION("return")
	{
		auto& bytes =
			"\x21"     // opcode
			"\xff\xff" // value temporary
		;

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::RETURN);
		REQUIRE(instr.unary.srcIdx == 0xffff);
	}

	SECTION("special call")
	{
		auto& bytes =
			"\x1f"     // opcode
			"\xff\xff" // function
			"\x02\x00" // two args
			"\x00\x00" // arg0
			"\x01\x00" // arg1
		;

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::SPECIAL_VOID);
		REQUIRE(instr.call.functionIdx == 0xffff);
		REQUIRE(instr.call.args.size() == 2);
		REQUIRE(instr.call.args[0] == 0);
		REQUIRE(instr.call.args[1] == 1);
	}

	SECTION("call")
	{
		auto& bytes =
			"\x1c"     // opcode
			"\xff\xff" // function
			"\x02\x00" // two args
			"\x00\x00" // arg0
			"\x01\x00" // arg1
			"\x02\x00" // ret
		;

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::CALL);
		REQUIRE(instr.call.functionIdx == 0xffff);
		REQUIRE(instr.call.args.size() == 2);
		REQUIRE(instr.call.args[0] == 0);
		REQUIRE(instr.call.args[1] == 1);
	}

	SECTION("call void")
	{
		auto& bytes =
			"\x1e"     // opcode
			"\xff\xff" // function
			"\x02\x00" // two args
			"\x00\x00" // arg0
			"\x01\x00" // arg1
		;

		auto stream = byteStreamOf(bytes);
		auto instr = internal::read<Instruction>(stream);

		REQUIRE(instr.opcode == Opcode::CALL_VOID);
		REQUIRE(instr.call.functionIdx == 0xffff);
		REQUIRE(instr.call.args.size() == 2);
		REQUIRE(instr.call.args[0] == 0);
		REQUIRE(instr.call.args[1] == 1);
	}
}


#define CHECK_DST_IDX_ASSIGNED(type, ...) {\
	/* this value will be assigned to each dstIdx beforehand */ \
	const am2017s::u16 ridiculousDstIdx = 5000; \
	/* construct a vector of all arguments */\
	std::vector<std::tuple<Opcode, std::string, bool>> ops = {__VA_ARGS__}; \
	\
	for(std::tuple<Opcode, std::string, bool> const& op : ops) \
	{ \
		/* now create an instruction for each opcode and assign a default dstIdx */ \
		Instruction i(std::get<0>(op)); \
		i.type.dstIdx = ridiculousDstIdx; \
		instructions.push_back(i); \
	} \
	std::vector<Local> parameters; \
	/* count temporaries (reassigns dstIdx where needed) */ \
	internal::countTemporaries(parameters, instructions); \
	\
	/* finally check if all dstIdx have been assigned correctly */ \
	static int count = 0; \
	for(size_t j = 0; j < ops.size(); ++j) \
	{\
		SECTION(std::get<1>(ops[j])) \
		{ \
			REQUIRE(instructions[j].opcode == std::get<0>(ops[j])); \
			if(std::get<2>(ops[j])) \
			{ \
				REQUIRE(count++ == instructions[j].type.dstIdx); \
			} \
			else { \
				REQUIRE(ridiculousDstIdx == instructions[j].type.dstIdx); \
			} \
		} \
	} \
}\

//void CHECK_DST_IDX_ASSIGNED()



#define TEST_INSTR(...) \




TEST_CASE("numbering of dstIdx", "[bytecode][static]")
{
	std::vector<Instruction> instructions;

	SECTION("unaries") {
		CHECK_DST_IDX_ASSIGNED(unary,
		                       { Opcode::LOAD, "load", true },
		                       { Opcode::STORE, "store", false },
		                       { Opcode::NEG, "neg", true },
		                       { Opcode::NOT, "not", true },
		                       { Opcode::RETURN, "return", false }
		);
	}

	SECTION("binaries") {
		CHECK_DST_IDX_ASSIGNED(binary,
		                       { Opcode::ADD, "add", true },
		                       { Opcode::SUB, "sub", true },
		                       { Opcode::MUL, "mul", true },
		                       { Opcode::DIV, "div", true },
		                       { Opcode::MOD, "mod", true },
		                       { Opcode::GT, "gt", true },
		                       { Opcode::GTE, "gte", true },
		                       { Opcode::EQ, "eq", true },
		                       { Opcode::NEQ, "neq", true },
		                       { Opcode::LTE, "lte", true },
		                       { Opcode::LT, "lt", true },
		                       { Opcode::AND, "and", true },
		                       { Opcode::OR, "or", true }
		);
	}

	SECTION("call") {
		CHECK_DST_IDX_ASSIGNED(call,
		                       { Opcode::CALL, "call", true },
		                       { Opcode::CALL_VOID, "call void", false },
		                       { Opcode::SPECIAL_VOID, "svcall", false }
		);
	}

	SECTION("const") {
		CHECK_DST_IDX_ASSIGNED(constant,
		                       { Opcode::CONST, "const", true }
		);
	}

	SECTION("allocate") {
		CHECK_DST_IDX_ASSIGNED(alloc,
		                       { Opcode::NEW, "new", true }
		);
	}

	SECTION("array") {
		CHECK_DST_IDX_ASSIGNED(array,
		                       { Opcode::LENGTH, "length", true },
		                       { Opcode::LOAD_IDX, "loadIdx", true },
		                       { Opcode::STORE_IDX, "storeIdx", false }
		);
	}
}

TEST_CASE("assign types to temporaries", "[static]")
{
	Program p{};
	Function function{};

	Function foo{};
	foo.returnType.isArray = false;
	foo.returnType.baseType = BaseType::BOOL;
	p.functions.push_back(foo);

	p.functions.push_back(function);

	Local i{};
	i.type.isArray = false;
	i.type.baseType = BaseType::INT32;

	Local l{};
	l.type.isArray = false;
	l.type.baseType = BaseType::INT64;

	Local b{};
	b.type.isArray = false;
	b.type.baseType = BaseType::BOOL;

	Local a{};
	a.type.isArray = true;
	a.type.baseType = BaseType::INT32;

	function.parameters = { i, l, b, a };

	// %0 = load($i)
	Instruction loadI(Opcode::LOAD);
	loadI.unary.srcIdx = 0;
	loadI.unary.dstIdx = 0;

	// %1 = load($l)
	Instruction loadL(Opcode::LOAD);
	loadL.unary.srcIdx = 1;
	loadL.unary.dstIdx = 1;

	// %2 = load($b)
	Instruction loadB(Opcode::LOAD);
	loadB.unary.srcIdx = 2;
	loadB.unary.dstIdx = 2;

	// %3 = load($a)
	Instruction loadA(Opcode::LOAD);
	loadA.unary.srcIdx = 3;
	loadA.unary.dstIdx = 3;

	// %4 = const(0)
	Instruction constI(Opcode::CONST);
	constI.constant.dstIdx = 4;
	constI.constant.type.isArray = false;
	constI.constant.type.baseType = BaseType::INT32;
	constI.constant.value = 0;

	// %5 = loadIdx(%3, %4)
	Instruction loadIdx(Opcode::LOAD_IDX);
	loadIdx.unary.dstIdx = 5;
	loadIdx.unary.srcIdx = 3;

	// %6 = add(%3, %5)
	Instruction addI(Opcode::ADD);
	addI.binary.dstIdx = 6;
	addI.binary.lsrcIdx = 4;
	addI.binary.rsrcIdx = 5;

	Instruction cmpI(Opcode::EQ);
	cmpI.binary.dstIdx = 7;
	cmpI.binary.lsrcIdx = 4;
	cmpI.binary.rsrcIdx = 5;

	Instruction callB(Opcode::CALL);
	callB.call.dstIdx = 8;
	callB.call.functionIdx = 0;

	Instruction negL(Opcode::NEG);
	negL.unary.dstIdx = 9;
	negL.unary.srcIdx = loadL.unary.dstIdx;

	Instruction length(Opcode::LENGTH);
	length.array.dstIdx = 10;
	length.array.memoryIdx = loadA.unary.dstIdx;

	Instruction newA(Opcode::NEW);
	newA.alloc.dstIdx = 11;
	newA.alloc.type.isArray = true;
	newA.alloc.type.baseType = BaseType::INT64;
	newA.alloc.sizeIdx = loadI.unary.dstIdx;

	function.instructions.push_back(loadI);
	function.instructions.push_back(loadL);
	function.instructions.push_back(loadB);
	function.instructions.push_back(loadA);
	function.instructions.push_back(constI);
	function.instructions.push_back(loadIdx);
	function.instructions.push_back(addI);
	function.instructions.push_back(cmpI);
	function.instructions.push_back(callB);
	function.instructions.push_back(negL);
	function.instructions.push_back(length);
	function.instructions.push_back(newA);

	function.temporyCount = 11;

	internal::assignTypesToTemporaries(p, function);

	REQUIRE(function.temporaryTypes.size() == function.temporyCount);

	REQUIRE(function.temporaryTypes[0].isArray == false);
	REQUIRE(function.temporaryTypes[0].baseType == BaseType::INT32);

	REQUIRE(function.temporaryTypes[1].isArray == false);
	REQUIRE(function.temporaryTypes[1].baseType == BaseType::INT64);

	REQUIRE(function.temporaryTypes[2].isArray == false);
	REQUIRE(function.temporaryTypes[2].baseType == BaseType::BOOL);

	REQUIRE(function.temporaryTypes[3].isArray == true);
	REQUIRE(function.temporaryTypes[3].baseType == BaseType::INT32);

	REQUIRE(function.temporaryTypes[4].isArray == false);
	REQUIRE(function.temporaryTypes[4].baseType == BaseType::INT32);

	REQUIRE(function.temporaryTypes[5].isArray == false);
	REQUIRE(function.temporaryTypes[5].baseType == BaseType::INT32);

	REQUIRE(function.temporaryTypes[6].isArray == false);
	REQUIRE(function.temporaryTypes[6].baseType == BaseType::INT32);

	REQUIRE(function.temporaryTypes[7].isArray == false);
	REQUIRE(function.temporaryTypes[7].baseType == BaseType::BOOL);

	REQUIRE(function.temporaryTypes[8].isArray == false);
	REQUIRE(function.temporaryTypes[8].baseType == BaseType::BOOL);

	REQUIRE(function.temporaryTypes[9].isArray == false);
	REQUIRE(function.temporaryTypes[9].baseType == BaseType::INT64);

	REQUIRE(function.temporaryTypes[10].isArray == false);
	REQUIRE(function.temporaryTypes[10].baseType == BaseType::INT32);

	REQUIRE(function.temporaryTypes[11].isArray == true);
	REQUIRE(function.temporaryTypes[11].baseType == BaseType::INT64);
}
