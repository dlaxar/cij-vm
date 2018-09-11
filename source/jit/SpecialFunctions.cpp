#include <jit/SpecialFunctions.hpp>
#include <jit/JitEngine.hpp>
#include <cstdlib>
#include <interpreter/InterpretEngine.hpp>

namespace am2017s { namespace bytecode { namespace special {

u16 resolveSpecialBuiltinOpcodes(u8 builtinOpcode) {
	switch(builtinOpcode) {
	case 0: // BENCHMARK_START
		return SPECIAL_F_IDX_START;
	case 1: // BENCHMARK_END
		return SPECIAL_F_IDX_END;
	case 2:
		return SPECIAL_F_IDX_PRINT_FLOAT;
	case 3:
		return SPECIAL_F_IDX_PRINTA_INT;
	case 4:
		return SPECIAL_F_IDX_PRINT_DOUBLE;
	case 5:
		return SPECIAL_F_IDX_EXIT;
	default:
		throw std::runtime_error("invalid builtin opcode");
	}
}

[[gnu::sysv_abi]]
void begin(jit::JitEngine* e)
{
	e->_beginCpu = clock();
	e->_beginReal = std::chrono::high_resolution_clock::now();
}

[[gnu::sysv_abi]]
void begin_int(interpreter::InterpretEngine* e)
{
	e->_beginReal = std::chrono::high_resolution_clock::now();
}

[[gnu::sysv_abi]]
void end_int(interpreter::InterpretEngine* e)
{
	auto realDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - e->_beginReal).count();
	std::cout << realDuration << std::endl;
}

[[gnu::sysv_abi]]
void end(jit::JitEngine* e)
{
	auto realDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - e->_beginReal).count();
	std::cout << realDuration << std::endl;
}

[[gnu::sysv_abi]]
void print_float(jit::JitEngine* e, float f) {
	printf("%f\n", f);
}

[[gnu::sysv_abi]]
void print_double(jit::JitEngine* e, double f) {
	printf("%f\n", f);
}

[[gnu::sysv_abi]]
void printa_int(jit::JitEngine* e, i32* array) {
	i32 size = array[-1];
	if(size == 0) {
		printf("[]\n");
	}

	printf("[");
	i32 i = 0;
	for(; i < (size - 1); ++i) {
		printf("%i, ", array[i]);
	}
	printf("%i]\n", array[i]);
}

[[gnu::sysv_abi]]
void exit(jit::JitEngine* e, i32 code) {
	std::cout << "Exiting " << code << std::endl;
	std::exit(code);
}

}}}