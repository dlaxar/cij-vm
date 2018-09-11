#pragma once

#include <bitset>
#include <memory>

#include <types.hpp>
#include <jit/CodeSegment.hpp>
#include <jit/Memory.hpp>

namespace am2017s { namespace jit
{
	constexpr i64 const HEAP_SIZE = 2u * 1024 * 1024 * 1024;
	constexpr i64 const PAGES_PER_HEAP = HEAP_SIZE / PAGE_SIZE;

	class CodeHeap
	{
		std::shared_ptr<void> _heap;
		std::bitset<PAGES_PER_HEAP> _bitmap;

		void deallocate(void* pages, i64 size);

	public:
		CodeHeap();
		CodeHeap(CodeHeap&&) noexcept = default;
		CodeHeap& operator=(CodeHeap&&) noexcept = default;

		CodeHeap(CodeHeap const&) = delete;
		CodeHeap& operator=(CodeHeap const&) = delete;

		CodeSegment allocate(i64 size);
	};
}}
