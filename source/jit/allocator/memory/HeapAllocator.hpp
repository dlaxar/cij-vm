#pragma once

#include <jit/JitEngine.hpp>

namespace am2017s { namespace jit { namespace allocator {

[[gnu::sysv_abi]]
void* allocate(JitEngine*, u16 size);

[[gnu::sysv_abi]]
void* allocate_array(jit::JitEngine* e, u8 elementSize, u8 type, i32 numElements);

}}}
