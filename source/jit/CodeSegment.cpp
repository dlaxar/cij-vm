#include <stdexcept>

#include <jit/CodeSegment.hpp>
#include <jit/Memory.hpp>

namespace am2017s { namespace jit
{
	void CodeSegment::markWritable()
	{
		pagesChangeAccess(_address.get(), _size, PageAccess::READ | PageAccess::WRITE);
	}

	void CodeSegment::markExecutable()
	{
		pagesChangeAccess(_address.get(), _size, PageAccess::READ | PageAccess::EXECUTE);
	}
}}
