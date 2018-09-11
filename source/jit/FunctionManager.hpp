#pragma once

#include <cstring>
#include <map>
#include <vector>

#include <types.hpp>
#include <jit/CodeHeap.hpp>
#include <jit/CodeSegment.hpp>

namespace am2017s { namespace jit
{
	class FunctionManager
	{
		CodeHeap _heap;
		std::map<u16, CodeSegment> _functions;

	public:
		void* create(u16 index, std::vector<u8> const& code)
		{
			auto segment = _heap.allocate(code.size());
			std::memcpy(segment.address(), code.data(), code.size());
			segment.markExecutable();
			_functions[index] = segment;
			return segment.address();
		}
	};
}}
