#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <jit/CodeHeap.hpp>

namespace am2017s { namespace jit
{
	static
	std::shared_ptr<void> allocateHeap()
	{
		auto heap = pagesAllocate(HEAP_SIZE, PageAccess::NONE, PageResidence::RESERVED);

		auto deleter =
			[](void* mem)
			{
				try
				{
					pagesFree(mem, HEAP_SIZE);
				}
				catch(std::runtime_error const& e)
				{
					std::fprintf(stderr, "exception while freeing code heap: %s\n", e.what());
				}
			};

		return std::shared_ptr<void>(heap, deleter);
	}

	CodeHeap::CodeHeap()
		: _heap(allocateHeap()), _bitmap{}
	{}

	static
	i64 pagesFor(i64 size)
	{
		return size / PAGE_SIZE + (size % PAGE_SIZE != 0);
	}

	// this function can't throw (called from dtors)
	void CodeHeap::deallocate(void* address, i64 size)
	{
		auto base = (u8*)_heap.get();
		auto allocation = (u8*)address;
		auto index = (allocation - base) / PAGE_SIZE;
		auto pages = pagesFor(size);

		for(std::size_t i = 0; i != pages; ++i)
			_bitmap.set(index + i, false);

		try
		{
			pagesChangeResidence(address, size, PageResidence::RESERVED);
		}
		catch(std::runtime_error const& e)
		{
			std::fprintf(stderr, "exception while freeing code segment: %s\n", e.what());
		}
	}

	CodeSegment CodeHeap::allocate(i64 requestedSize)
	{
		auto pagesNeeded = pagesFor(requestedSize);

		if(pagesNeeded > 1)
			throw std::logic_error("allocations > 1 page are nyi");

		for(std::size_t i = 0; i != _bitmap.size(); ++i)
			if(!_bitmap[i])
			{
				_bitmap[i] = true;

				auto address = (u8*)_heap.get() + i * PAGE_SIZE;
				auto allocationSize = pagesNeeded * PAGE_SIZE;

				pagesChangeResidence(address, allocationSize, PageResidence::COMMITTED);
				pagesChangeAccess(address, allocationSize, PageAccess::READ | PageAccess::WRITE);

				auto deleter = [heap = _heap, allocationSize, this](void* mem){ deallocate(mem, allocationSize); };
				return CodeSegment(std::shared_ptr<void>(address, deleter), allocationSize);
			}

		throw std::runtime_error("out of code memory");
	}
}}
