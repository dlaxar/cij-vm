#pragma once

#include <jit/allocator/memory/HeapAllocator.hpp>
#include <jit/JitEngine.hpp>
#include <interpreter/InterpretEngine.hpp>

#define SPECIAL_FUNCTIONS (8)

#define SPECIAL_F_IDX_ALLOCATE         (0)
#define SPECIAL_F_PTR_ALLOCATE         &am2017s::jit::allocator::allocate
#define SPECIAL_F_IDX_START            (1)
#define SPECIAL_F_PTR_START            &am2017s::bytecode::special::begin
#define SPECIAL_F_IDX_END              (2)
#define SPECIAL_F_PTR_END              &am2017s::bytecode::special::end
#define SPECIAL_F_IDX_PRINT_FLOAT      (3)
#define SPECIAL_F_PTR_PRINT_FLOAT      &am2017s::bytecode::special::print_float
#define SPECIAL_F_IDX_ALLOC_ARRAY      (4)
#define SPECIAL_F_PTR_ALLOC_ARRAY      &am2017s::jit::allocator::allocate_array
#define SPECIAL_F_IDX_PRINTA_INT       (5)
#define SPECIAL_F_PTR_PRINTA_INT       &am2017s::bytecode::special::printa_int;
#define SPECIAL_F_IDX_PRINT_DOUBLE     (6)
#define SPECIAL_F_PTR_PRINT_DOUBLE     &am2017s::bytecode::special::print_double;
#define SPECIAL_F_IDX_EXIT             (7)
#define SPECIAL_F_PTR_EXIT             &am2017s::bytecode::special::exit;
////// DONT FORGET TO CHANGE SPECIAL_FUNCTIONS
////// DONT FORGET TO CHANGE SPECIAL_FUNCTIONS
////// DONT FORGET TO CHANGE SPECIAL_FUNCTIONS
////// DONT FORGET TO CHANGE SPECIAL_FUNCTIONS


namespace am2017s { namespace bytecode { namespace special {

u16 resolveSpecialBuiltinOpcodes(u8 builtinOpcode);

[[gnu::sysv_abi]]
void begin(jit::JitEngine* e);

[[gnu::sysv_abi]]
void end(jit::JitEngine* e);

[[gnu::sysv_abi]]
void print_float(jit::JitEngine* e, float f);

[[gnu::sysv_abi]]
void print_double(jit::JitEngine* e, double f);

[[gnu::sysv_abi]]
void printa_int(jit::JitEngine* e, i32* array);

[[gnu::sysv_abi]]
void exit(jit::JitEngine* e, i32 code);

[[gnu::sysv_abi]]
void begin_int(interpreter::InterpretEngine* e);

[[gnu::sysv_abi]]
void end_int(interpreter::InterpretEngine* e);

}}}
