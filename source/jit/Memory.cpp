#include <stdexcept>

#include <jit/Memory.hpp>

#if defined(_WIN64) || defined(__CYGWIN__)

#include <Windows.h>

namespace am2017s { namespace jit
{
	static
	std::string lastErrorString()
	{
		auto code = GetLastError();
		auto flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;
		auto lang = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
		char* errmsg;

		if(!FormatMessageA(flags, nullptr, code, lang, (char*)&errmsg, 0, nullptr))
			throw std::runtime_error("FormatMessageA failed");

		std::string result = errmsg;
		LocalFree(errmsg);
		return result;
	}

	[[noreturn]]
	static
	void throwLastError(char const* callingFunction, char const* calledFunction)
	{
		throw std::runtime_error(callingFunction + ": "s + calledFunction + ": "s + lastErrorString());
	}

// warning: case value not in enumerated type
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
	static
	DWORD convertPageAccess(PageAccess access)
	{
		switch(access)
		{
		case PageAccess::NONE:
			return PAGE_NOACCESS;

		case PageAccess::READ:
			return PAGE_READONLY;

		case PageAccess::WRITE:
		case PageAccess::WRITE | PageAccess::READ:
			return PAGE_READWRITE;

		case PageAccess::EXECUTE:
			return PAGE_EXECUTE;

		case PageAccess::EXECUTE | PageAccess::READ:
			return PAGE_EXECUTE_READ;

		case PageAccess::EXECUTE | PageAccess::WRITE:
		case PageAccess::EXECUTE | PageAccess::WRITE | PageAccess::READ:
			return PAGE_EXECUTE_READWRITE;

		default: break;
		}

		throw std::logic_error("invalid page access");
	}
#pragma GCC diagnostic pop

	static
	DWORD convertPageResidence(PageResidence residence)
	{
		switch(residence)
		{
		case PageResidence::RESERVED: return MEM_RESERVE;
		case PageResidence::COMMITTED: return MEM_COMMIT;
		default: break;
		}

		throw std::logic_error("unreachable");
	}

	void* pagesAllocate(i64 size, PageAccess access, PageResidence residence)
	{
		auto nativeAccess = convertPageAccess(access);
		auto nativeResidence = convertPageResidence(residence);
		auto pages = VirtualAlloc(nullptr, size, nativeResidence, nativeAccess);

		if(pages == nullptr)
			throwLastError("pagesAllocate", "VirtualAlloc");

		return pages;
	}

	void pagesChangeAccess(void* pages, i64 size, PageAccess access)
	{
		auto nativeAccess = convertPageAccess(access);

		DWORD ignored; // old protection
		if(!VirtualProtect(pages, size, nativeAccess, &ignored))
			throwLastError("pagesChangeAccess", "VirtualProtect");
	}

	void pagesChangeResidence(void* pages, i64 size, PageResidence residence)
	{
		switch(residence)
		{
		case PageResidence::RESERVED:
			if(!VirtualFree(pages, size, MEM_DECOMMIT))
				throwLastError("pagesChangeResidence", "VirtualFree");

			break;

		case PageResidence::COMMITTED:
			if(!VirtualAlloc(pages, size, MEM_COMMIT, PAGE_NOACCESS))
				throwLastError("pagesChangeResidence", "VirtualAlloc");

			break;
		}
	}

	void pagesFree(void* pages, i64)
	{
		if(!VirtualFree(pages, 0, MEM_RELEASE))
			throwLastError("pagesFree", "VirtualFree");
	}
}}

#else

#include <cerrno>
#include <cstring>

#include <sys/mman.h>

namespace am2017s { namespace jit
{
	[[noreturn]]
	static
	void throwLastError(char const* callingFunction, char const* calledFunction)
	{
		throw std::runtime_error(callingFunction + ": "s + calledFunction + ": "s + std::strerror(errno));
	}

	static
	int convertPageAccess(PageAccess access)
	{
		if(access == PageAccess::NONE)
			return PROT_NONE;

		auto value = (u8)access;
		int result = 0;

		if(value & (u8)PageAccess::READ)
			result |= PROT_READ;

		if(value & (u8)PageAccess::WRITE)
			result |= PROT_WRITE;

		if(value & (u8)PageAccess::EXECUTE)
			result |= PROT_EXEC;

		return result;
	}

	void* pagesAllocate(i64 size, PageAccess access, PageResidence)
	{
		auto pages = mmap(nullptr, size, convertPageAccess(access), MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

		if(pages == MAP_FAILED)
			throwLastError("pagesAllocate", "mmap");

		return pages;
	}

	void pagesChangeAccess(void* pages, i64 size, PageAccess access)
	{
		if(mprotect(pages, size, convertPageAccess(access)) == -1)
			throwLastError("pagesChangeAccess", "mprotect");
	}

	void pagesChangeResidence(void* pages, i64 size, PageResidence residence)
	{
		switch(residence)
		{
		case PageResidence::RESERVED:
			if(madvise(pages, size, MADV_DONTNEED) == -1)
				throwLastError("pagesChangeResidence", "madvise");
			break;

		case PageResidence::COMMITTED:
			break;
		}
	}

	void pagesFree(void* pages, i64 size)
	{
		if(munmap(pages, size) == -1)
			throwLastError("pagesFree", "munmap");
	}
}}

#endif
