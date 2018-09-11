#pragma once

#include <types.hpp>

namespace am2017s { namespace jit
{
	constexpr i64 const PAGE_SIZE = 4096;

	enum struct PageAccess : u8
	{
		NONE    = 0,
		READ    = 1,
		WRITE   = 2,
		EXECUTE = 4,
	};

	inline
	constexpr PageAccess operator | (PageAccess left, PageAccess right)
	{
		return (PageAccess)((u8)left | (u8)right);
	}

	enum struct PageResidence : u8
	{
		RESERVED,
		COMMITTED,
	};

	void* pagesAllocate(i64 size, PageAccess access, PageResidence residence);
	void pagesChangeAccess(void* pages, i64 size, PageAccess access);
	void pagesChangeResidence(void* pages, i64 size, PageResidence residence);
	void pagesFree(void* pages, i64 size);
}}
