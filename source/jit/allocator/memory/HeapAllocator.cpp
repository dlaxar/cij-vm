#include <jit/allocator/memory/HeapAllocator.hpp>
#include <log/Logger.hpp>

#include <iostream>
#include <bytecode.hpp>

namespace am2017s { namespace jit { namespace allocator {

[[gnu::sysv_abi]]
void* allocate(JitEngine*, u16 size) {
	void* address = malloc(size);
	Logger::log(Topic::RUN_ALLOC) << "allocating " << size << " bytes at (" << address << ")" << std::endl;
	return address;
}

[[gnu::sysv_abi]]
void* allocate_array(jit::JitEngine* e, u8 elementSize, u8 type, i32 numElements) {
	i32* address = (i32*) malloc(elementSize*numElements + sizeof(i32));

	Logger::log(Topic::RUN_ALLOC) << "allocating array for element size " << std::to_string(elementSize)
	                              << " (type id: " << std::to_string(type) << ") with " << numElements
	                              << " element at (+4) " << address << std::endl;

	address[0] = numElements;

	for(i32 i = 1; i <= numElements; ++i) {
		switch((bytecode::BaseType) type) {
			case bytecode::BaseType::BOOL:
			case bytecode::BaseType::INT8:
				((u8*)(address))[i] = 0;
				break;
			case bytecode::BaseType::CHAR:
			case bytecode::BaseType::INT16:
				((u16*)(address))[i] = 0;
				break;
			case bytecode::BaseType::INT32:
				((u32*)(address))[i] = 0;
				break;
			case bytecode::BaseType::INT64:
				((u64*)(address))[i] = 0;
				break;
			case bytecode::BaseType::FLP32:
				((float*)(address))[i] = 0.0; // todo use fixed float32 (not available)
				break;
			case bytecode::BaseType::FLP64:
				((double*)(address))[i] = 0.0; // todo use fixed float64 (not available)
				break;
			default:
				if(type >= 9) {
					((void**)(address))[i] = 0; // NULL
				} else {
					throw std::runtime_error("fallthrough in array creation");
				}
		}
	}

	return &(address[1]);
}

}}}
