#include <cstring>
#include <stdexcept>
#include <utility>

#include <jit/JitEngine.hpp>
#include <fstream>
#include <jit/lifetime/LifetimeAnalyzer.hpp>
#include <jit/allocator/register/RegisterAllocator.hpp>
#include <jit/optimizations/Optimizer.hpp>
#include <jit/architecture/Architecture.hpp>
#include <jit/lir/LIRCompiler.hpp>
#include <jit/machine/MachineCompiler.hpp>

#include <jit/SpecialFunctions.hpp>

#include <jit/JitEngine.hpp>
#include <log/Logger.hpp>

namespace am2017s { namespace jit
{
	extern "C"
	[[gnu::sysv_abi]]
	i64 jit_invoke(void** fptable, u16 fidx) asm("jit_invoke");

	extern "C"
	[[gnu::sysv_abi]]
	void jit_stub() asm("jit_stub");

	extern "C"
	[[gnu::sysv_abi]]
	void jit_member_stub() asm("jit_member_stub");

	extern "C"
	[[gnu::sysv_abi]]
	void* jit_compile(JitEngine* engine, u16 index) asm("jit_compile");
	void* jit_compile(JitEngine* engine, u16 index)
	{
		return engine->compile(index);
	}

	static
	u16 findMain(std::vector<bytecode::Function> const& functions)
	{
		for(u16 i = 0; i != functions.size(); ++i)
			if(functions[i].name == "main")
				return i;

		throw std::logic_error("main function not found");
	}

	JitEngine::JitEngine(bytecode::Program program, Options const& options)
		: _program(std::move(program))
		, _functionTable(_program.functions.size() + 1 /* JitEngine */ + 1 /* global */ + SPECIAL_FUNCTIONS, reinterpret_cast<void*>(jit_stub))
		, _options(options)
	{
		std::set<u16> virtualFunctionIndices;
		for(auto typePair : _program.types) {
			for(auto fIdx : typePair.second.vTable) {
				virtualFunctionIndices.insert(fIdx);
			}
		}

		for(auto fIdx : virtualFunctionIndices) {
			_functionTable[fIdx + 1 /* JitEngine */ + 1 /* global */ + SPECIAL_FUNCTIONS] = (void*) &jit_member_stub;
		}

		_functionTable[SPECIAL_FUNCTIONS - 1 /* JitEngine */ - SPECIAL_F_IDX_EXIT        ] = (void*) SPECIAL_F_PTR_EXIT;
		_functionTable[SPECIAL_FUNCTIONS - 1 /* JitEngine */ - SPECIAL_F_IDX_PRINT_DOUBLE] = (void*) SPECIAL_F_PTR_PRINT_DOUBLE;
		_functionTable[SPECIAL_FUNCTIONS - 1 /* JitEngine */ - SPECIAL_F_IDX_PRINTA_INT  ] = (void*) SPECIAL_F_PTR_PRINTA_INT;
		_functionTable[SPECIAL_FUNCTIONS - 1 /* JitEngine */ - SPECIAL_F_IDX_ALLOC_ARRAY ] = (void*) SPECIAL_F_PTR_ALLOC_ARRAY;
		_functionTable[SPECIAL_FUNCTIONS - 1 /* JitEngine */ - SPECIAL_F_IDX_PRINT_FLOAT ] = (void*) SPECIAL_F_PTR_PRINT_FLOAT;
		_functionTable[SPECIAL_FUNCTIONS - 1 /* JitEngine */ - SPECIAL_F_IDX_START       ] = (void*) SPECIAL_F_PTR_START;
		_functionTable[SPECIAL_FUNCTIONS - 1 /* JitEngine */ - SPECIAL_F_IDX_END         ] = (void*) SPECIAL_F_PTR_END;
		_functionTable[SPECIAL_FUNCTIONS - 1 /* JitEngine */ - SPECIAL_F_IDX_ALLOCATE    ] = (void*) SPECIAL_F_PTR_ALLOCATE;
		_functionTable[SPECIAL_FUNCTIONS] = malloc(100); // todo calculate size of global
		_functionTable[SPECIAL_FUNCTIONS + 1] = this;


		Logger::log(Topic::ADDRESS) << "JitEngine* : " << this << std::endl;
	}

	int JitEngine::execute()
	{
		auto idx = findMain(_program.functions);
		compile(idx);

		void **fptable = _functionTable.data() + SPECIAL_FUNCTIONS + 1 /* global */ + 1 /* JitEngine */;

		Logger::log(Topic::ADDRESS) << "Invoking main method, passing function table: " << fptable << std::endl;

		i64 returnCode = jit_invoke(fptable, idx);

		Logger::log(Topic::RESULT) << "Client Program exited with code " << returnCode << std::endl;

		return returnCode;
	}

	static
	void writeDebugFile(std::vector<u8> const& code, bytecode::Function const& function)
	{
		std::ofstream file("function_" + function.name + ".dump", std::ios::binary);

		for(u8 b : code)
			file << b;
	}

	void* JitEngine::compile(u16 index)
	{
		if(index >= _program.functions.size())
			throw std::logic_error("invalid function index");

		Logger::log(Topic::COMPILE) << "Compiling function " << _program.functions[index].name << std::endl;

		bytecode::Function const& func = _program.functions[index];
		auto skip = Optimizer(func).run();

		// translate to LIR
		LIRCompiler<AMD64> lirCompiler(this, _program, _program.types, func, skip);
		lirCompiler.run();

		auto liveIntervals = LifetimeAnalyzer(func, lirCompiler.blocks, lirCompiler.numberOfLIRs()).run();
		allocator::RegisterAllocation<AMD64> allocation(_program.functions[index],
		                                                liveIntervals,
		                                                lirCompiler.usages,
		                                                lirCompiler.fixedToVR,
		                                                lirCompiler.fixedXMMToVR,
		                                                lirCompiler.overflowArgToVR,
		                                                lirCompiler.vrTypes,
		                                                lirCompiler.hintSame);
		allocation.run();

		MachineCompiler machine(lirCompiler.blocks,
		                        allocation.handled,
		                        allocation.stackAllocator,
		                        lirCompiler.vrTypes,
		                        allocation.stackFrameSpills);
		machine.run();


//		exit(0);
//		auto allocations = performRegisterAllocation(func, lifetimes);
//		auto code = compileFunction(func, allocations);

		auto code = machine.builder.build();
		auto address = _fmgr.create(index, code);

		if(_options.debug)
		{
			writeDebugFile(code, _program.functions[index]);
			Logger::log(Topic::ADDRESS) << "Produced code for function " << _program.functions[index].name << " (at address " << address << ")" << std::endl;
		}

		return _functionTable[index + 1 /* JitEngine */ + 1 /* global */ + SPECIAL_FUNCTIONS] = address;
	}

i32 JitEngine::specialFunctionIndex(u16 index) {
	return 0 - index - 1 /* skip JitEngine* */ - 1 /* skip global */ - 1 /* pass *over* the qword we will read */;
}
}}
